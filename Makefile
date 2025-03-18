# Makefile for building raw_blk kernel module

ifneq ($(KERNELRELEASE),)
# 内核构建系统调用的部分
obj-m := nvme_read_write.o

else
# 用户空间部分：指定内核源码路径，根据实际情况修改内核源码路径
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

endif
