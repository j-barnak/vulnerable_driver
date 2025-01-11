MODULE_NAME := vuln_driver

KDIR := /lib/modules/$(shell uname -r)/build

obj-m := $(MODULE_NAME).o
# needed for pr_*
CFLAGS_vuln_driver.o := -DDEBUG

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod $(MODULE_NAME).ko

remove:
	sudo rmmod $(MODULE_NAME)

build_exploit:
	gcc exploit.c -o exploit

exploit:
	sudo ./exploit