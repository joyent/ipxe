#ifndef _GPXE_ARP_H
#define _GPXE_ARP_H

/** @file
 *
 * Address Resolution Protocol
 *
 */

#include <gpxe/tables.h>

struct net_device;
struct net_protocol;

/** A network-layer protocol that relies upon ARP */
struct arp_net_protocol {
	/** Network-layer protocol */
	struct net_protocol *net_protocol;
	/** Check existence of address
	 *
	 * @v netdev	Network device
	 * @v net_addr	Network-layer address
	 * @ret rc	Return status code
	 */
	int ( * check ) ( struct net_device *netdev,
			  const void *net_addr );
};

/**
 * Register an ARP protocol
 *
 * @v protocol		ARP protocol
 */
#define ARP_NET_PROTOCOL( protocol ) \
	struct arp_net_protocol protocol __table ( arp_net_protocols, 01 )

extern int arp_resolve ( struct net_device *netdev,
			 struct net_protocol *net_protocol,
			 const void *dest_net_addr,
			 const void *source_net_addr,
			 void *dest_ll_addr );

#endif /* _GPXE_ARP_H */
