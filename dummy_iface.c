#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/printk.h>

#include "dummy_iface.h"
#include "dummy_iface_macro.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("oleksandr.ivantsiv");
MODULE_DESCRIPTION("A Dummy Interface module");

static int __init hello_init(void)
{
	DI_TRACE_CALL(error);

	dummy_iface_netlink_init();

	return 0;
}

static void __exit hello_cleanup(void)
{
	DI_TRACE_CALL(error);

	dummy_iface_netlink_fini();
}

module_init(hello_init);
module_exit(hello_cleanup);
