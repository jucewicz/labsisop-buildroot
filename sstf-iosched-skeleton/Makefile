obj-m := sstf-iosched.o

BUILDROOT_DIR := $(abspath ..)
KDIR := $(BUILDROOT_DIR)/output/build/linux-custom

CROSS_COMPILE := $(BUILDROOT_DIR)/output/host/bin/i686-buildroot-linux-gnu-
COMPILER := $(CROSS_COMPILE)gcc

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) modules
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=x86 CROSS_COMPILE=$(CROSS_COMPILE) modules_install INSTALL_MOD_PATH=$(BUILDROOT_DIR)/output/target
	$(COMPILER) -o sector_read sector_read.c
	cp sector_read $(BUILDROOT_DIR)/output/target/bin/

clean:
	rm -f *.o *.ko .*.cmd
	rm -f modules.order
	rm -f Module.symvers
	rm -f sstf-iosched.mod.c
	rm -f sector_read