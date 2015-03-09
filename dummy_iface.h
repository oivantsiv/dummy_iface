/*
 * dummy-iface.h
 *
 *  Created on: Feb 22, 2015
 *      Author: oivantsi
 */

#ifndef DUMMY_IFACE_H_
#define DUMMY_IFACE_H_

#include <linux/if_link.h>
#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/types.h>


struct dummy_iface_params {
	__u8  attr0;
	__u16 attr1;
	__u32 attr2;

	__u32 attr_nest_a;
	__u32 attr_nest_b;

	struct ifla_dummy_iface_bin_attr attr_bin;
};


struct dummy_iface {
	struct net_device *dev;
	struct dummy_iface_params params;
};

int dummy_iface_netlink_init(void);
void dummy_iface_netlink_fini(void);
bool is_dummy_iface(struct net_device *dev);

#endif /* DUMMY_IFACE_H_ */
