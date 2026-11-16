ARCH = arm64
CROSS_COMPILE = aarch64-linux-
obj-m = trivMod.o
ROOTDIR=/home/student/Downloads/rpi-buildroot-2026.08/output/build/linux-custom
PWD = $(shell pwd)
default:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C $(ROOTDIR) ARCH=$(ARCH) M=$(PWD) modules

clean:
	$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) -C $(ROOTDIR) ARCH=$(ARCH) M=$(PWD) clean
