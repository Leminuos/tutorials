
obj-m += oled_drv.o
PWD := $(shell pwd)
KDIR := /home/nguyenbui/tutorial/bbb/bb-kernel/KERNEL
CROSS_COMPILE := /home/nguyenbui/tutorial/bbb/bb-kernel/dl/gcc-8.5.0-nolibc/arm-linux-gnueabi/bin/arm-linux-gnueabi-

all:
	make -C $(KDIR) M=$(PWD) modules 
	$(CROSS_COMPILE)gcc -o oled_test oled_test.c

clean:
	make -C $(KDIR) M=$(PWD) modules clean
	rm -rf oled_test

