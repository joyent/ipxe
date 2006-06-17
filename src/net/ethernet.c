/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <assert.h>
#include <vsprintf.h>
#include <gpxe/if_arp.h>
#include <gpxe/if_ether.h>
#include <gpxe/netdevice.h>
#include <gpxe/pkbuff.h>
#include <gpxe/ethernet.h>

/** @file
 *
 * Ethernet protocol
 *
 */

/** Ethernet broadcast MAC address */
static uint8_t eth_broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/**
 * Transmit Ethernet packet
 *
 * @v pkb		Packet buffer
 * @v netdev		Network device
 * @v net_protocol	Network-layer protocol
 * @v ll_dest		Link-layer destination address
 *
 * Prepends the Ethernet link-layer header and transmits the packet.
 */
static int eth_tx ( struct pk_buff *pkb, struct net_device *netdev,
		    struct net_protocol *net_protocol, const void *ll_dest ) {
	struct ethhdr *ethhdr = pkb_push ( pkb, sizeof ( *ethhdr ) );

	/* Build Ethernet header */
	memcpy ( ethhdr->h_dest, ll_dest, ETH_ALEN );
	memcpy ( ethhdr->h_source, netdev->ll_addr, ETH_ALEN );
	ethhdr->h_protocol = net_protocol->net_proto;

	/* Hand off to network device */
	return netdev_tx ( netdev, pkb );
}

/**
 * Process received Ethernet packet
 *
 * @v pkb	Packet buffer
 * @v netdev	Network device
 *
 * Strips off the Ethernet link-layer header and passes up to the
 * network-layer protocol.
 */
static void eth_rx ( struct pk_buff *pkb, struct net_device *netdev ) {
	struct ethhdr *ethhdr = pkb->data;

	/* Sanity check */
	if ( pkb_len ( pkb ) < sizeof ( *ethhdr ) ) {
		DBG ( "Ethernet packet too short (%d bytes)\n",
		      pkb_len ( pkb ) );
		return;
	}

	/* Strip off Ethernet header */
	pkb_pull ( pkb, sizeof ( *ethhdr ) );

	/* Hand off to network-layer protocol */
	net_rx ( pkb, netdev, ethhdr->h_protocol, ethhdr->h_source );
}

/**
 * Transcribe Ethernet address
 *
 * @v ll_addr	Link-layer address
 * @ret string	Link-layer address in human-readable format
 */
static const char * eth_ntoa ( const void *ll_addr ) {
	static char buf[18]; /* "00:00:00:00:00:00" */
	const uint8_t *eth_addr = ll_addr;

	sprintf ( buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		  eth_addr[0], eth_addr[1], eth_addr[2],
		  eth_addr[3], eth_addr[4], eth_addr[5] );
	return buf;
}

/** Ethernet protocol */
struct ll_protocol ethernet_protocol = {
	.name		= "Ethernet",
	.ll_proto	= htons ( ARPHRD_ETHER ),
	.ll_addr_len	= ETH_ALEN,
	.ll_broadcast	= eth_broadcast,
	.tx		= eth_tx,
	.rx		= eth_rx,
	.ntoa		= eth_ntoa,
};

LL_PROTOCOL ( ethernet_protocol );
