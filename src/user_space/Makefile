CC       = gcc
CFLAGS   = -g $(shell pkg-config --cflags libnl-3.0)
RM       = rm -f
INC      = -I/usr/include/libnl3/netlink/
LIB	 = $(shell pkg-config --libs libnl-3.0)
LIB_PATH = -L/usr/lib64


default: all

all: rtnl rtnl_listener

rtnl: dummy_iface_rtnl.c
	$(CC) $(CFLAGS) $(INC) $(LIB_PATH) $(LIB) -o di_rtnl dummy_iface_rtnl.c

rtnl_listener: dummy_iface_rtnl_listener.c
	$(CC) $(CFLAGS) $(INC) $(LIB_PATH) $(LIB) -o di_rtnl_listener dummy_iface_rtnl_listener.c

clean:
	$(RM) rtnl rtnl_listener
