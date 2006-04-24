/**************************************************************************
Etherboot -  BOOTP/TFTP Bootstrap Program
Bochs Pseudo NIC driver for Etherboot
***************************************************************************/

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * See pnic_api.h for an explanation of the Bochs Pseudo NIC.
 */

#include <stdint.h>
#include <io.h>
#include <vsprintf.h>
#include <errno.h>
#include <gpxe/pci.h>
#include <gpxe/if_ether.h>
#include <gpxe/ethernet.h>
#include <gpxe/pkbuff.h>
#include <gpxe/netdevice.h>

#include "pnic_api.h"

struct pnic {
	unsigned short ioaddr;
};

/* 
 * Utility functions: issue a PNIC command, retrieve result.  Use
 * pnic_command_quiet if you don't want failure codes to be
 * automatically printed.  Returns the PNIC status code.
 * 
 * Set output_length to NULL only if you expect to receive exactly
 * output_max_length bytes, otherwise it'll complain that you didn't
 * get enough data (on the assumption that if you not interested in
 * discovering the output length then you're expecting a fixed amount
 * of data).
 */

static uint16_t pnic_command_quiet ( struct pnic *pnic, uint16_t command,
				     void *input, uint16_t input_length,
				     void *output, uint16_t output_max_length,
				     uint16_t *output_length ) {
	int i;
	uint16_t status;
	uint16_t _output_length;

	if ( input != NULL ) {
		/* Write input length */
		outw ( input_length, pnic->ioaddr + PNIC_REG_LEN );
		/* Write input data */
		for ( i = 0; i < input_length; i++ ) {
			outb( ((char*)input)[i],
			      pnic->ioaddr + PNIC_REG_DATA );
		}
	}
	/* Write command */
	outw ( command, pnic->ioaddr + PNIC_REG_CMD );
	/* Retrieve status */
	status = inw ( pnic->ioaddr + PNIC_REG_STAT );
	/* Retrieve output length */
	_output_length = inw ( pnic->ioaddr + PNIC_REG_LEN );
	if ( output_length == NULL ) {
		if ( _output_length != output_max_length ) {
			printf ( "pnic_command %#hx: wrong data length "
				 "returned (expected %d, got %d)\n", command,
				 output_max_length, _output_length );
		}
	} else {
		*output_length = _output_length;
	}
	if ( output != NULL ) {
		if ( _output_length > output_max_length ) {
			printf ( "pnic_command %#hx: output buffer too small "
				 "(have %d, need %d)\n", command,
				 output_max_length, _output_length );
			_output_length = output_max_length;
		}
		/* Retrieve output data */
		for ( i = 0; i < _output_length; i++ ) {
			((char*)output)[i] =
				inb ( pnic->ioaddr + PNIC_REG_DATA );
		}
	}
	return status;
}

static uint16_t pnic_command ( struct pnic *pnic, uint16_t command,
			       void *input, uint16_t input_length,
			       void *output, uint16_t output_max_length,
			       uint16_t *output_length ) {
	uint16_t status = pnic_command_quiet ( pnic, command,
					       input, input_length,
					       output, output_max_length,
					       output_length );
	if ( status == PNIC_STATUS_OK ) return status;
	printf ( "PNIC command %#hx (len %#hx) failed with status %#hx\n",
		 command, input_length, status );
	return status;
}

/* Check API version matches that of NIC */
static int pnic_api_check ( uint16_t api_version ) {
	if ( api_version != PNIC_API_VERSION ) {
		printf ( "Warning: API version mismatch! "
			 "(NIC's is %d.%d, ours is %d.%d)\n",
			 api_version >> 8, api_version & 0xff,
			 PNIC_API_VERSION >> 8, PNIC_API_VERSION & 0xff );
	}
	if ( api_version < PNIC_API_VERSION ) {
		printf ( "** You may need to update your copy of Bochs **\n" );
	}
	return ( api_version == PNIC_API_VERSION );
}

/**************************************************************************
POLL - Wait for a frame
***************************************************************************/
static void pnic_poll ( struct net_device *netdev ) {
	struct pnic *pnic = netdev->priv;
	struct pk_buff *pkb;
	uint16_t length;
	uint16_t qlen;

	/* Fetch all available packets */
	while ( 1 ) {
		if ( pnic_command ( pnic, PNIC_CMD_RECV_QLEN, NULL, 0,
				    &qlen, sizeof ( qlen ), NULL )
		     != PNIC_STATUS_OK )
			break;
		if ( qlen == 0 )
			break;
		pkb = alloc_pkb ( ETH_FRAME_LEN );
		if ( ! pkb )
			break;
		if ( pnic_command ( pnic, PNIC_CMD_RECV, NULL, 0,
				    pkb->data, ETH_FRAME_LEN, &length )
		     != PNIC_STATUS_OK ) {
			free_pkb ( pkb );
			break;
		}
		pkb_put ( pkb, length );
		netdev_rx ( netdev, pkb );
	}
}

/**************************************************************************
TRANSMIT - Transmit a frame
***************************************************************************/
static int pnic_transmit ( struct net_device *netdev, struct pk_buff *pkb ) {
	struct pnic *pnic = netdev->priv;

	pnic_command ( pnic, PNIC_CMD_XMIT, pkb, pkb_len ( pkb ),
		       NULL, 0, NULL );
	free_pkb ( pkb );
	return 0;
}

/**************************************************************************
IRQ - Handle card interrupt status
***************************************************************************/
#if 0
static void pnic_irq ( struct net_device *netdev, irq_action_t action ) {
	struct pnic *pnic = netdev->priv;
	uint8_t enabled;

	switch ( action ) {
	case DISABLE :
	case ENABLE :
		enabled = ( action == ENABLE ? 1 : 0 );
		pnic_command ( pnic, PNIC_CMD_MASK_IRQ,
			       &enabled, sizeof ( enabled ), NULL, 0, NULL );
		break;
	case FORCE :
		pnic_command ( pnic, PNIC_CMD_FORCE_IRQ,
			       NULL, 0, NULL, 0, NULL );
		break;
	}
}
#endif

/**************************************************************************
DISABLE - Turn off ethernet interface
***************************************************************************/
static void pnic_remove ( struct pci_device *pci ) {
	struct net_device *netdev = pci_get_drvdata ( pci );
	struct pnic *pnic = netdev->priv;

	unregister_netdev ( netdev );
	pnic_command ( pnic, PNIC_CMD_RESET, NULL, 0, NULL, 0, NULL );
	free_netdev ( netdev );
}

/**************************************************************************
PROBE - Look for an adapter, this routine's visible to the outside
***************************************************************************/
static int pnic_probe ( struct pci_device *pci ) {
	struct net_device *netdev;
	struct pnic *pnic;
	uint16_t api_version;
	uint16_t status;
	int rc;

	/* Allocate net device */
	netdev = alloc_etherdev ( sizeof ( *pnic ) );
	if ( ! netdev ) {
		rc = -ENOMEM;
		goto err;
	}
	pnic = netdev->priv;
	pci_set_drvdata ( pci, netdev );
	pnic->ioaddr = pci->ioaddr;

	/* API version check */
	status = pnic_command_quiet ( pnic, PNIC_CMD_API_VER, NULL, 0,
				      &api_version,
				      sizeof ( api_version ), NULL );
	if ( status != PNIC_STATUS_OK ) {
		printf ( "PNIC failed installation check, code %#hx\n",
			 status );
		rc = -EIO;
		goto err;
	}
	pnic_api_check ( api_version );

	/* Get MAC address */
	status = pnic_command ( pnic, PNIC_CMD_READ_MAC, NULL, 0,
				netdev->ll_addr, ETH_ALEN, NULL );

	/* Point to NIC specific routines */
	netdev->poll	 = pnic_poll;
	netdev->transmit = pnic_transmit;

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err;

	return 0;

 err:
	/* Free net device */
	free_netdev ( netdev );
	return rc;
}

static struct pci_id pnic_nics[] = {
/* genrules.pl doesn't let us use macros for PCI IDs...*/
PCI_ROM ( 0xfefe, 0xefef, "pnic", "Bochs Pseudo NIC Adaptor" ),
};

static struct pci_driver pnic_driver = {
	.ids = pnic_nics,
	.id_count = ( sizeof ( pnic_nics ) / sizeof ( pnic_nics[0] ) ),
	.class = PCI_NO_CLASS,
	//	.probe = pnic_probe,
	//	.remove = pnic_remove,
};

// PCI_DRIVER ( pnic_driver );


static int pnic_hack_probe ( void *dummy, struct pci_device *pci ) {
	return ( pnic_probe ( pci ) == 0 );
}

static void pnic_hack_disable ( void *dummy, struct pci_device *pci ) {
	pnic_remove ( pci );
}

#include "dev.h"
extern struct type_driver test_driver;

DRIVER ( "PNIC", test_driver, pci_driver, pnic_driver,
	 pnic_hack_probe, pnic_hack_disable );
