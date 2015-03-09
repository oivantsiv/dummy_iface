obj-m += di.o

di-y	:= dummy_iface.o dummy_iface_netlink.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

