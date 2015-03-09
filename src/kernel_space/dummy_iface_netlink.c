/*
 * dummy_iface_netlink.c
 *
 *  Created on: Feb 22, 2015
 *      Author: oivantsi
 */

#define pr_fmt(fmt)	"(dummy iface netlink): " fmt

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_link.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <net/netlink.h>
#include <net/rtnetlink.h>

#include "dummy_iface.h"
#include "dummy_iface_macro.h"

static void di_setup(struct net_device *dev);
static void di_free(struct net_device *dev);
static int di_validate(struct nlattr *tb[], struct nlattr *data[]);
static int di_newlink(struct net *src_net,
			struct net_device *dev,
			struct nlattr *tb[],
			struct nlattr *data[]);
static int di_changelink(struct net_device *dev,
			   struct nlattr *tb[],
			   struct nlattr *data[]);
static void di_dellink(struct net_device *dev,
			 struct list_head *head);
static size_t di_get_size(const struct net_device *dev);
static int di_fill_info(struct sk_buff *skb,
			  const struct net_device *dev);
static int di_dev_init(struct net_device *dev);
static void di_dev_uninit(struct net_device *dev);

static int di_set_nest_opt(struct dummy_iface *di,
			   int nest_type,
			   struct nlattr *data[]);
static int di_set_opt(struct dummy_iface *di, struct nlattr *data[],
		      int type, struct nlattr *nla);

#define DI_SET_OPT(_di, _data, _type) do { \
		int err = di_set_opt(_di, _data, \
				 _type, \
				 _data[_type]); \
		if (err) \
			return err;\
	} while(0)

static const struct nla_policy di_policy[IFLA_DUMMY_IFACE_MAX + 1] = {
		[IFLA_DUMMY_IFACE_ATTR_0]	= { .type = NLA_U8 },
		[IFLA_DUMMY_IFACE_ATTR_1]	= { .type = NLA_U16 },
		[IFLA_DUMMY_IFACE_ATTR_2]	= { .type = NLA_U32 },
		[IFLA_DUMMY_IFACE_ATTR_NEST]	= { .type = NLA_NESTED},
		[IFLA_DUMMY_IFACE_ATTR_BIN]	= { .type = NLA_BINARY,
						    .len = sizeof(struct ifla_dummy_iface_bin_attr)}
};

struct rtnl_link_ops di_link_ops __read_mostly = {
	/* Identifier ("interface type") */
	.kind		= "dummy_iface",
	/* sizeof net_device private space */
	.priv_size	= sizeof(struct dummy_iface),
	/* net_device setup function */
	.setup		= di_setup,
	/* Highest device specific netlink attribute number */
	.maxtype	= IFLA_DUMMY_IFACE_MAX,
	/* Netlink policy for device specific attribute validation */
	.policy		= di_policy,
	/* Optional validation function for netlink/changelink parameters */
	.validate	= di_validate,
	/* Function for configuring and registering a new device */
	.newlink	= di_newlink,
	/* Function for changing parameters of an existing device */
	.changelink	= di_changelink,
	/* Function to remove a device */
	.dellink	= di_dellink,
	/* Function to calculate required room for dumping device specific netlink attributes */
	.get_size	= di_get_size,
	/* Function to dump device specific netlink attributes */
	.fill_info	= di_fill_info,
};

static const struct net_device_ops di_netdev_ops = {
	.ndo_init		= di_dev_init,
	.ndo_uninit		= di_dev_uninit,
	//.ndo_start_xmit		= ,
	.ndo_validate_addr	= eth_validate_addr,
	//.ndo_set_rx_mode	= set_multicast_list,
	.ndo_set_mac_address	= eth_mac_addr,
	//.ndo_get_stats64	= dummy_get_stats64,
	//.ndo_change_carrier	= dummy_change_carrier,
};


/*         -----------------
 *        |                 |
 *        |   rtnl_newlink  |
 *        |                 |
 *         -----------------
 *                 |
 *                 |
 *                 |
 *         -----------------
 *        |                 |
 *        | rtnl_create_link|
 *        |                 |
 *         -----------------
 *                 |
 *                 |
 *                 |
 *         -----------------
 *        |                 |
 *        | alloc_netdev_mqs|
 *        |                 |
 *         -----------------
 *                 |
 *                 |
 *                 |
 *         -----------------
 *        |                 |
 *        |      setup      |
 *        |                 |
 *         -----------------
 *
 * Register a rtnetlink message type:
 *
 *     rtnl_register(PF_UNSPEC, RTM_NEWLINK, rtnl_newlink, NULL, NULL);
 *
 *
 * rtnl_newlink:
 * 	Function for configuring and registering a new device
 * 	(parsing netlink attributes, creating/setuping link,
 * 	setting attributes (standard and device specific),
 * 	device registration)
 *
 * rtnl_create_link:
 * 	Link creation (device allocation, device initialization,
 * 	setting standard attributes)
 *
 * alloc_netdev_mqs:
 * 	allocate network device (allocate a struct net_device with
 * 	private data area for driver use and perform basic
 * 	initialization. Also allocate subqueue struct for
 * 	 each queue on the device)
 *
 * setup:
 * 	net_device setup function. All net_device related initializations
 * 	("ops", flags, prived data, etc.)
 */
static void di_setup(struct net_device *dev)
{
	struct dummy_iface *di = netdev_priv(dev);

	DI_TRACE_CALL(err);

	/* Fill in the fields of the device structure with Ethernet-generic values. */
	ether_setup(dev);

	/* Drivers should call free_netdev() in ->destructor
	 * and unregister it on failure after registration
	 * so that device could be finally freed in rtnl_unlock.
	 */
	dev->destructor = di_free;

	/* Includes several pointers to callbacks,
	 *if one wants to override the ndo_*() functions
	 */
	dev->netdev_ops = &di_netdev_ops;

	di->dev = dev;

	eth_hw_addr_random(dev);
}

/* Called from unregister.
 * Drivers should call free_netdev() in ->destructor
 */
static void di_free(struct net_device *dev)
{
	DI_TRACE_CALL(err);

	/* Free network device.
	 * This function does the last stage of destroying an allocated device
	 * interface. The reference to the device object is released.
	 * If this is the last reference then it will be freed.
	 */
	free_netdev(dev);
}

/* Optional validation function for netlink/changelink parameters
 */
static int di_validate(struct nlattr *tb[], struct nlattr *data[])
{
	DI_TRACE_CALL(err);

	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != ETH_ALEN)
			return -EINVAL;
		if (!is_valid_ether_addr(nla_data(tb[IFLA_ADDRESS])))
			return -EADDRNOTAVAIL;
	}
	return 0;
}

/*         -----------------
 *        |                 |
 *        |   rtnl_newlink  |
 *        |                 |
 *         -----------------
 *                 |
 *                 |
 *                 |
 *         -----------------
 *        |                 |
 *        | rtnl_create_link|
 *        |                 |
 *         -----------------
 *                 |
 *                 |---------------
 *                 |               |
 *         -----------------       |
 *        |                 |      |
 *        | alloc_netdev_mqs|      |
 *        |                 |      |
 *         -----------------       |
 *                 |               |
 *                 |               |
 *                 |               |
 *         -----------------       |
 *        |                 |      |
 *        |      setup      |      |
 *        |                 |      |
 *         -----------------       |
 *                                 |
 *                                 |
 *                                 |
 *                         -----------------
 *                        |                 |
 *                        |     newlink     |
 *                        |                 |
 *                         -----------------
 * newlink:
 * 	Function for configuring and registering a new device.
 *
 * None:
 * 	System device attributes is handled in rtnl_create_link
 * 	function before newlink call.
 *
 * 	Device specific attributes hold in IFLA_INFO_DATA container.
 *
 * 	Drivers should call register_netdevice from ->newlink
 * */
static int di_newlink(struct net *src_net, /* Device namespace */
		      struct net_device *dev,
		      struct nlattr *tb[], /* system device attributes */
		      struct nlattr *data[])
{
	int err;

	DI_TRACE_CALL(err);

	err = di_changelink(dev, tb, data);
	if (err < 0)
		return err;

	return register_netdevice(dev);
}


static int di_changelink(struct net_device *dev,
			   struct nlattr *tb[],
			   struct nlattr *data[])
{
	struct dummy_iface *di = netdev_priv(dev);

	DI_TRACE_CALL(err);

	if (!data)
		return 0;

	if (data[IFLA_DUMMY_IFACE_ATTR_0])
		DI_SET_OPT(di, data, IFLA_DUMMY_IFACE_ATTR_0);

	if (data[IFLA_DUMMY_IFACE_ATTR_1])
		DI_SET_OPT(di, data, IFLA_DUMMY_IFACE_ATTR_1);

	if (data[IFLA_DUMMY_IFACE_ATTR_2])
		DI_SET_OPT(di, data, IFLA_DUMMY_IFACE_ATTR_2);

	if (data[IFLA_DUMMY_IFACE_ATTR_NEST])
		DI_SET_OPT(di, data, IFLA_DUMMY_IFACE_ATTR_NEST);

	if (data[IFLA_DUMMY_IFACE_ATTR_BIN])
		DI_SET_OPT(di, data, IFLA_DUMMY_IFACE_ATTR_BIN);

	return 0;
}

/*         -----------------
 *        |                 |
 *        |   rtnl_dellink  |
 *        |                 |
 *         -----------------
 *                 |
 *                 |
 *                 |
 *         -----------------
 *        |                 |
 *        |     dellink     |
 *        |                 |
 *         -----------------
 *
 * dellink:
 * 	Function to remove a device
 */
static void di_dellink(struct net_device *dev,
			 struct list_head *head)
{
	DI_TRACE_CALL(err);

	unregister_netdevice_queue(dev, head);
}

static size_t di_get_size(const struct net_device *dev)
{
	DI_TRACE_CALL(err);

	return nla_total_size(sizeof(__u8)) + /* IFLA_DUMMY_IFACE_ATTR_0 */
		nla_total_size(sizeof(__u16)) + /* IFLA_DUMMY_IFACE_ATTR_1 */
		nla_total_size(sizeof(__u32)) + /* IFLA_DUMMY_IFACE_ATTR_1 */
		nla_total_size(sizeof(struct nlattr)) + /* IFLA_DUMMY_IFACE_ATTR_NEST */
		nla_total_size(sizeof(__u32)) + /* IFLA_DUMMY_IFACE_ATTR_NEST_A */
		nla_total_size(sizeof(__u32)) + /* IFLA_DUMMY_IFACE_ATTR_NEST_B */
		nla_total_size(sizeof(struct ifla_dummy_iface_bin_attr)); /* IFLA_DUMMY_IFACE_ATTR_BIN */
}

static int di_fill_info(struct sk_buff *skb,
			  const struct net_device *dev)
{
	int err;
	struct nlattr *nla_nest;
	struct dummy_iface *di = netdev_priv(dev);

	DI_TRACE_CALL(err);

	err = nla_put_u8(skb, IFLA_DUMMY_IFACE_ATTR_0, di->params.attr0);
	if (err)
		goto err_out;

	err = nla_put_u16(skb, IFLA_DUMMY_IFACE_ATTR_1, di->params.attr1);
	if (err)
		goto err_out;

	err = nla_put_u32(skb, IFLA_DUMMY_IFACE_ATTR_2, di->params.attr2);
	if (err)
		goto err_out;

	err = nla_put(skb, IFLA_DUMMY_IFACE_ATTR_BIN,
			sizeof(struct ifla_dummy_iface_bin_attr), &di->params.attr_bin);
	if (err)
		goto err_out;

	nla_nest= nla_nest_start(skb, IFLA_DUMMY_IFACE_ATTR_NEST);
	if (nla_nest == NULL)
		goto err_out;

	err = nla_put_u32(skb, IFLA_DUMMY_IFACE_ATTR_NEST_A, di->params.attr_nest_a);
	if (err)
		goto err_out;

	err = nla_put_u32(skb, IFLA_DUMMY_IFACE_ATTR_NEST_B, di->params.attr_nest_b);
	if (err)
		goto err_out;

	nla_nest_end(skb, nla_nest);

	return 0;

err_out:
	return err;
}

static int di_dev_init(struct net_device *dev)
{
	DI_TRACE_CALL(err);

	return 0;
}

static void di_dev_uninit(struct net_device *dev)
{
	DI_TRACE_CALL(err);
}

static int di_set_nest_opt(struct dummy_iface *di,
			   int nest_type,
			   struct nlattr *data[])
{
	DI_TRACE_CALL(err);

	if (nest_type != IFLA_DUMMY_IFACE_ATTR_NEST)
		return -EINVAL;

	if (data[IFLA_DUMMY_IFACE_ATTR_NEST_A])
		di->params.attr_nest_a = nla_get_u32(data[IFLA_DUMMY_IFACE_ATTR_NEST_A]);

	if (data[IFLA_DUMMY_IFACE_ATTR_NEST_B])
		di->params.attr_nest_b = nla_get_u32(data[IFLA_DUMMY_IFACE_ATTR_NEST_A]);

	return 0;
}

static int di_set_opt(struct dummy_iface *di, struct nlattr *data[],
		      int type, struct nlattr *nla)
{
	int err = 0;

	DI_TRACE_CALL(err);

	switch (type) {
	case IFLA_DUMMY_IFACE_ATTR_0:
		di->params.attr0 = nla_get_u8(nla);
		break;
	case IFLA_DUMMY_IFACE_ATTR_1:
		di->params.attr1 = nla_get_u16(nla);
		break;
	case IFLA_DUMMY_IFACE_ATTR_2:
		di->params.attr2 = nla_get_u32(nla);
		break;
	case IFLA_DUMMY_IFACE_ATTR_NEST:
		err = di_set_nest_opt(di, type, nla_data(nla));
		break;
	case IFLA_DUMMY_IFACE_ATTR_BIN:
		if (nla_len(nla) < sizeof (struct ifla_dummy_iface_bin_attr))
			err = -EINVAL;
		else
			memcpy(&di->params.attr_bin, nla_data(nla),
			       sizeof (struct ifla_dummy_iface_bin_attr));
		break;
	default:
		err = -EINVAL;
	}

	if (!err)
		call_netdevice_notifiers(NETDEV_CHANGEINFODATA, di->dev);

	return err;
}

int dummy_iface_netlink_init(void)
{
	DI_TRACE_CALL(err);

	return rtnl_link_register(&di_link_ops);
}

void dummy_iface_netlink_fini(void)
{
	DI_TRACE_CALL(err);

	rtnl_link_unregister(&di_link_ops);
}

bool is_dummy_iface(struct net_device *dev)
{
	DI_TRACE_CALL(err);

	return false;
}
