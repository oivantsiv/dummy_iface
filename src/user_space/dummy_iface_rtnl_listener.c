/*
 * dummy_iface_rtnl_listener.c
 *
 *  Created on: Mar 10, 2015
 *      Author: oivantsiv
 */

#include <stdio.h>
#include <stdbool.h>

#include <linux/netdevice.h>
#include <linux/if_link.h>
#include <libnl3/netlink/netlink.h>
#include <libnl3/netlink/msg.h>
#include <libnl3/netlink/attr.h>
#include <libnl3/netlink/socket.h>

struct dummy_iface_context {
	struct nl_sock *sk;
};

static const struct nla_policy ifla_policy[IFLA_MAX+1] = {
	[IFLA_IFNAME]		= { .type = NLA_STRING, .len = IFNAMSIZ-1 },
	[IFLA_ADDRESS]		= { .type = NLA_BINARY, .len = MAX_ADDR_LEN },
	[IFLA_BROADCAST]	= { .type = NLA_BINARY, .len = MAX_ADDR_LEN },
	[IFLA_MAP]		= { .len = sizeof(struct rtnl_link_ifmap) },
	[IFLA_MTU]		= { .type = NLA_U32 },
	[IFLA_LINK]		= { .type = NLA_U32 },
	[IFLA_MASTER]		= { .type = NLA_U32 },
	[IFLA_CARRIER]		= { .type = NLA_U8 },
	[IFLA_TXQLEN]		= { .type = NLA_U32 },
	[IFLA_WEIGHT]		= { .type = NLA_U32 },
	[IFLA_OPERSTATE]	= { .type = NLA_U8 },
	[IFLA_LINKMODE]		= { .type = NLA_U8 },
	[IFLA_LINKINFO]		= { .type = NLA_NESTED },
	[IFLA_NET_NS_PID]	= { .type = NLA_U32 },
	[IFLA_NET_NS_FD]	= { .type = NLA_U32 },
	[IFLA_IFALIAS]	        = { .type = NLA_STRING, .len = IFALIASZ-1 },
	[IFLA_VFINFO_LIST]	= {. type = NLA_NESTED },
	[IFLA_VF_PORTS]		= { .type = NLA_NESTED },
	[IFLA_PORT_SELF]	= { .type = NLA_NESTED },
	[IFLA_AF_SPEC]		= { .type = NLA_NESTED },
	[IFLA_EXT_MASK]		= { .type = NLA_U32 },
	[IFLA_PROMISCUITY]	= { .type = NLA_U32 },
	[IFLA_NUM_TX_QUEUES]	= { .type = NLA_U32 },
	[IFLA_NUM_RX_QUEUES]	= { .type = NLA_U32 },
	[IFLA_PHYS_PORT_ID]	= { .type = NLA_BINARY, .len = MAX_PHYS_PORT_ID_LEN },
	[IFLA_CARRIER_CHANGES]	= { .type = NLA_U32 },  /* ignored */
};

static const struct nla_policy ifla_info_policy[IFLA_INFO_MAX+1] = {
	[IFLA_INFO_KIND]	= { .type = NLA_STRING },
	[IFLA_INFO_DATA]	= { .type = NLA_NESTED },
	[IFLA_INFO_SLAVE_KIND]	= { .type = NLA_STRING },
	[IFLA_INFO_SLAVE_DATA]	= { .type = NLA_NESTED },
};

static const struct nla_policy di_policy[IFLA_DUMMY_IFACE_MAX + 1] = {
	[IFLA_DUMMY_IFACE_ATTR_0]	= { .type = NLA_U8 },
	[IFLA_DUMMY_IFACE_ATTR_1]	= { .type = NLA_U16 },
	[IFLA_DUMMY_IFACE_ATTR_2]	= { .type = NLA_U32 },
	[IFLA_DUMMY_IFACE_ATTR_NEST]	= { .type = NLA_NESTED},
	[IFLA_DUMMY_IFACE_ATTR_BIN]	= { .type = NLA_BINARY,
					    .len = sizeof(struct ifla_dummy_iface_bin_attr)}
};

static int di_callback(struct nl_msg *msg, void *context)
{
	printf("%s\n", __FUNCTION__);

	return 0;
}

int main(int argc, char *argv[])
{
	int err = 0;
	struct nl_sock *sk;
	struct dummy_iface_context di_context;

	sk = nl_socket_alloc();

	di_context.sk = sk;

	nl_socket_disable_seq_check(sk);

	err = nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM,
			di_callback, &di_context);
	if (err) {
		perror("Failed to add callback");
		goto free_hdl;
	}

	err = nl_connect(sk, NETLINK_ROUTE);
	if (err) {
		perror("Failed to connect to NETLINK_ROUTE");
		goto free_hdl;
	}

	err = nl_socket_add_membership(sk, RTNLGRP_LINK);
	if (err) {
		perror("Failed subscribe to link notification group");
		goto free_hdl;
	}

	while (true) {
		nl_recvmsgs_default(sk);
	}

free_hdl:
	nl_socket_free(sk);

	return err;
}
