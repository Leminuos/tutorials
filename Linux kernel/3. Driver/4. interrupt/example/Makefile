ARCH := arm
obj-m := interrupt.o

PWD := $(shell pwd)
KDIR := /home/nguyenbui/tutorial/bbb/bb-kernel/KERNEL
CROSS := /home/nguyenbui/tutorial/bbb/bb-kernel/dl/gcc-8.5.0-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
