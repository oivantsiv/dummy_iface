/*
 * dummy_iface_macro.h
 *
 *  Created on: Mar 12, 2015
 *      Author: oivantsiv
 */

#ifndef SRC_USER_SPACE_DUMMY_IFACE_MACRO_H_
#define SRC_USER_SPACE_DUMMY_IFACE_MACRO_H_

#include <stdio.h>

#define DI_TRACE_CALL(severity)	printf("%s\n", __FUNCTION__)
#define ARRAY_SIZE(arr)		sizeof(arr)/sizeof(*arr)

#endif /* SRC_USER_SPACE_DUMMY_IFACE_MACRO_H_ */
