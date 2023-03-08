obj-y := sys_call.o
obj-m := barstool.o


PWD := $(shell pwd)
KDIR := /lib/modules/`uname -r`/build

default: wrapper.o
	$(MAKE) -C $(KDIR) M=$(PWD) SUBDIRS=$(PWD) modules

wrapper.o: wrappers.h
	gcc -o consumer consumer.c
	gcc -o producer producer.c
clean:
	rm -f *.o *.ko *.mod.* Module.* modules.* *.mod .*.cmd
