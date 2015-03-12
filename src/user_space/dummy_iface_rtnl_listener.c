/*
 * dummy_iface_rtnl_listener.c
 *
 *  Created on: Mar 10, 2015
 *      Author: oivantsiv
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/socket.h>

#include <linux/if_link.h>
#include <linux/netdevice.h>

#include <libnl3/netlink/netlink.h>
#include <libnl3/netlink/msg.h>
#include <libnl3/netlink/attr.h>
#include <libnl3/netlink/socket.h>

#include "dummy_iface_macro.h"

struct dummy_iface_context {
	struct nl_sock *sk;
};

typedef int (*dummy_iface_nla_cb)(int attr, struct nlattr *nla, void *context);

struct dummy_iface_nla_handler {
	dummy_iface_nla_cb cb;
};

static int di_ifla_ifname_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_address_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_mtu_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_link_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_linkinfo_handler(int attr, struct nlattr *nla, void *context);

static int di_ifla_info_kind_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_info_data_handler(int attr, struct nlattr *nla, void *context);

static int di_ifla_di_attr_0_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_di_attr_1_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_di_attr_2_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_di_attr_nest_handler(int attr, struct nlattr *nla, void *context);
static int di_ifla_di_attr_bin_handler(int attr, struct nlattr *nla, void *context);

static const char *rtmtostr(int type);

static struct nla_policy ifla_policy[IFLA_MAX+1] = {
	[IFLA_IFNAME]		= { .type = NLA_STRING, .maxlen = IFNAMSIZ-1 },
	[IFLA_ADDRESS]		= {  .maxlen = MAX_ADDR_LEN },
	[IFLA_BROADCAST]	= {  .maxlen = MAX_ADDR_LEN },
	[IFLA_MAP]		= {  .maxlen = sizeof(struct rtnl_link_ifmap) },
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
	[IFLA_IFALIAS]		= { .type = NLA_STRING, .maxlen = IFALIASZ-1 },
	[IFLA_VFINFO_LIST]	= {. type = NLA_NESTED },
	[IFLA_VF_PORTS]		= { .type = NLA_NESTED },
	[IFLA_PORT_SELF]	= { .type = NLA_NESTED },
	[IFLA_AF_SPEC]		= { .type = NLA_NESTED },
	[IFLA_EXT_MASK]		= { .type = NLA_U32 },
	[IFLA_PROMISCUITY]	= { .type = NLA_U32 },
	[IFLA_NUM_TX_QUEUES]	= { .type = NLA_U32 },
	[IFLA_NUM_RX_QUEUES]	= { .type = NLA_U32 },
	[IFLA_PHYS_PORT_ID]	= { .maxlen = MAX_ADDR_LEN },
	[IFLA_CARRIER_CHANGES]	= { .type = NLA_U32 },  /* ignored */
};

static struct dummy_iface_nla_handler ifla_nla_handler[IFLA_MAX+1] = {
	[IFLA_IFNAME]		= { .cb = di_ifla_ifname_handler },
	[IFLA_ADDRESS]		= { .cb = di_ifla_address_handler },
	[IFLA_MTU]		= { .cb = di_ifla_mtu_handler },
	[IFLA_LINK]		= { .cb = di_ifla_link_handler },
	[IFLA_LINKINFO]		= { .cb = di_ifla_linkinfo_handler },
};

static struct nla_policy ifla_info_policy[IFLA_INFO_MAX+1] = {
	[IFLA_INFO_KIND]	= { .type = NLA_STRING },
	[IFLA_INFO_DATA]	= { .type = NLA_NESTED },
	[IFLA_INFO_SLAVE_KIND]	= { .type = NLA_STRING },
	[IFLA_INFO_SLAVE_DATA]	= { .type = NLA_NESTED },
};

static struct dummy_iface_nla_handler ifla_info_nla_handler[IFLA_INFO_MAX+1] = {
	[IFLA_INFO_KIND]	= { .cb = di_ifla_info_kind_handler },
	[IFLA_INFO_DATA]	= { .cb = di_ifla_info_data_handler },
};

static struct nla_policy di_policy[IFLA_DUMMY_IFACE_MAX+1] = {
	[IFLA_DUMMY_IFACE_ATTR_0]	= { .type = NLA_U8 },
	[IFLA_DUMMY_IFACE_ATTR_1]	= { .type = NLA_U16 },
	[IFLA_DUMMY_IFACE_ATTR_2]	= { .type = NLA_U32 },
	[IFLA_DUMMY_IFACE_ATTR_NEST]	= { .type = NLA_NESTED},
	[IFLA_DUMMY_IFACE_ATTR_BIN]	= { .maxlen = sizeof(struct ifla_dummy_iface_bin_attr)}
};

static struct dummy_iface_nla_handler ifla_di_nla_handler[IFLA_DUMMY_IFACE_MAX+1] = {
	[IFLA_DUMMY_IFACE_ATTR_0]	= { .cb = di_ifla_di_attr_0_handler },
	[IFLA_DUMMY_IFACE_ATTR_1]	= { .cb = di_ifla_di_attr_1_handler },
	[IFLA_DUMMY_IFACE_ATTR_2]	= { .cb = di_ifla_di_attr_2_handler },
	[IFLA_DUMMY_IFACE_ATTR_NEST]	= { .cb = di_ifla_di_attr_nest_handler },
	[IFLA_DUMMY_IFACE_ATTR_BIN]	= { .cb = di_ifla_di_attr_bin_handler },
};


static int di_valid_msg_cb(struct nl_msg *msg, void *context)
{
	int i, err, rem;
	struct nlmsghdr *hdr, *stream;
	struct nlattr *ifla_tb[IFLA_MAX+1];
	int ifla_attrs_to_parse[] = { IFLA_IFNAME, IFLA_ADDRESS, IFLA_MTU,
			IFLA_LINK, IFLA_LINKINFO };

	DI_TRACE_CALL(err);

	stream = nlmsg_hdr(msg);
	rem = nlmsg_get_max_size(msg);

	for (hdr = stream; nlmsg_ok(hdr, rem); hdr = nlmsg_next(hdr, &rem)) {
		printf("Message:\n");
		printf("    Type:  %s\n", rtmtostr(hdr->nlmsg_type));
		printf("    PID:   %"PRIu32"\n", hdr->nlmsg_pid);
		printf("    Len:   %"PRIu32"\n", hdr->nlmsg_len);
		printf("    Seq:   %"PRIu32"\n", hdr->nlmsg_seq);
		printf("    Flags: %"PRIu32"\n\n", hdr->nlmsg_flags);

		err = nlmsg_parse(hdr, NLMSG_HDRLEN, ifla_tb,
				IFLA_MAX, ifla_policy);
		if (err) {
			perror("Failed to parse nlmsg");
			continue;
		}

		for (i = 0; i < ARRAY_SIZE(ifla_attrs_to_parse); ++i) {
			int attr = ifla_attrs_to_parse[i];
			if (ifla_tb[attr] && ifla_nla_handler[attr].cb) {
				err = ifla_nla_handler[attr].cb(
						attr,
						ifla_tb[attr],
						context);
				if (err)
					printf("Failed to handle %d attr", attr);
			}
		}
	}

	return 0;
}

static int di_ifla_ifname_handler(int attr, struct nlattr *nla, void *context)
{
	printf("IFLA_IFNAME: %s\n", nla_get_string(nla));

	return 0;
}

static int di_ifla_address_handler(int attr, struct nlattr *nla, void *context)
{
	uint8_t data[MAX_ADDR_LEN];
	memcpy(data, nla_data(nla), sizeof(data));

	printf("IFLA_ADDRESS: %02x:%02x:%02x:%02x:%02x:%02x\n",
			data[0], data[1], data[2],
			data[3], data[4], data[5]);

	return 0;
}

static int di_ifla_mtu_handler(int attr, struct nlattr *nla, void *context)
{
	printf("IFLA_MTU: %"PRIu32"\n", nla_get_u32(nla));

	return 0;
}

static int di_ifla_link_handler(int attr, struct nlattr *nla, void *context)
{
	printf("IFLA_LINK: %"PRIu32"\n", nla_get_u32(nla));

	return 0;
}

static int di_ifla_linkinfo_handler(int attr, struct nlattr *nla, void *context)
{
	int rem;
	struct nlattr *nla_iter;

	nla_for_each_nested(nla_iter, nla, rem) {
		int attr = nla_type(nla_iter);
		if (ifla_info_nla_handler[attr].cb)
			ifla_info_nla_handler[attr].cb(attr, nla_iter, context);
	}

	return 0;
}

static int di_ifla_info_kind_handler(int attr, struct nlattr *nla, void *context)
{
	printf("IFLA_INFO_KIND: %s\n", nla_get_string(nla));

	return 0;
}

static int di_ifla_info_data_handler(int attr, struct nlattr *nla, void *context)
{
	printf("IFLA_INFO_DATA: \n");

	return 0;
}

static int di_ifla_di_attr_0_handler(int attr, struct nlattr *nla, void *context)
{
	return 0;
}

static int di_ifla_di_attr_1_handler(int attr, struct nlattr *nla, void *context)
{
	return 0;
}

static int di_ifla_di_attr_2_handler(int attr, struct nlattr *nla, void *context)
{
	return 0;
}

static int di_ifla_di_attr_nest_handler(int attr, struct nlattr *nla, void *context)
{
	return 0;
}

static int di_ifla_di_attr_bin_handler(int attr, struct nlattr *nla, void *context)
{
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
			di_valid_msg_cb, &di_context);
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

static const char *rtmtostr(int type)
{
	const char *str;

	switch (type) {
	case RTM_NEWLINK:
		str = "RTM_NEWLINK";
		break;
	case RTM_SETLINK:
		str = "RTM_SETLINK";
		break;
	case RTM_DELLINK:
		str = "RTM_DELLINK";
		break;
	default:
		str = "UNKNOWN";
		break;
	}

	return str;
}

