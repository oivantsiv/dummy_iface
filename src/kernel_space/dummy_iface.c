#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros

#include "dummy_iface.h"
#include "dummy_iface_macro.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("oleksandr.ivantsiv");
MODULE_DESCRIPTION("A Dummy Interface module");

static int __init dummy_iface_init(void)
{
	DI_TRACE_CALL(err);

	dummy_iface_netlink_init();

	return 0;
}

static void __exit dummy_iface_cleanup(void)
{
	DI_TRACE_CALL(err);

	dummy_iface_netlink_fini();
}

module_init(dummy_iface_init);
module_exit(dummy_iface_cleanup);
