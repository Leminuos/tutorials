# Tên module (tạo ra file my_module.ko)
obj-m := driver.o

# Thư mục chứa mã nguồn Linux Kernel (cần sửa nếu khác)
KDIR := /lib/modules/$(shell uname -r)/build

# Trình biên dịch Kernel
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

# Quy tắc clean (xoá file tạm)
clean:
	make -C $(KDIR) M=$(PWD) clean