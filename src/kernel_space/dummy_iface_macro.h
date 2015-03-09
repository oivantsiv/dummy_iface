/*
 * dummy_iface_macro.h
 *
 *  Created on: Mar 9, 2015
 *      Author: oivantsi
 */

#ifndef DUMMY_IFACE_MACRO_H_
#define DUMMY_IFACE_MACRO_H_

#include <linux/printk.h>

#define DI_TRACE_CALL(severity)	pr_##severity("%s\n", __FUNCTION__)

#endif /* DUMMY_IFACE_MACRO_H_ */
