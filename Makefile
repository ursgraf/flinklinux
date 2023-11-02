# Makefile for the fLink linux kernel modules
# 2014-01-14 martin@zueger.eu

ifeq ($(KERNELRELEASE),)

ifeq ($(CHROOT),)
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
else
	KERNELDIR ?= /usr/src/linux
	CHROOT_CMD ?= schroot -c $(CHROOT) --
endif

	PWD := $(shell pwd)

modules: flink_ioctl.h flink_fmi.c
	$(CHROOT_CMD) $(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules
	
modules_install:
	$(CHROOT_CMD) $(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers
	rm -rf mpc5200/*.ko mpc5200/*.mod.c mpc5200/*.o
	rm -rf imx6/*.ko imx6/*.mod.c imx6/*.o
	rm -f flink_ioctl.h
	rm -f flink_fmi.c

flink_ioctl.h: flinkinterface/ioctl/create_flink_ioctl.h.sh flinkinterface/func_id/func_id_definitions.sh
	flinkinterface/ioctl/create_flink_ioctl.h.sh
	
flink_fmi.c: flinkinterface/func_id/create_flink_fmi.c.sh flinkinterface/func_id/func_id_definitions.sh
	flinkinterface/func_id/create_flink_fmi.c.sh

.PHONY: modules clean

else
#	EXTRA_CFLAGS += -DDEBUG
	ccflags-y := -std=gnu99
	obj-m := flink.o
	obj-m += flink_axi.o

ifeq ($(CONFIG_PCI),y) 
	obj-m += flink_pci.o 
endif

ifeq ($(CONFIG_SPI),y) 
	obj-m += flink_spi.o
endif

ifeq ($(CONFIG_PPC_MPC5200_SIMPLE),y)
	obj-m += mpc5200/flink_lpb.o
endif

ifneq ($(CONFIG_IMX_WEIM),)
	obj-m += imx6/flink_eim.o
endif
	
	flink-objs := flink_core.o
endif


