obj-y := sys_call.o
obj-m := barstool.o


PWD := $(shell pwd)
KDIR := /lib/modules/`uname -r`/build

default: consumer.o producer.o
	$(MAKE) -C $(KDIR) M=$(PWD) SUBDIRS=$(PWD) modules

consumer.o: wrappers.h
	gcc -o consumer consumer.c
producer.o: wrappers.h
	gcc -o producer producer.c
clean:
	rm -f *.o *.ko *.mod.* Module.* modules.* *.mod .*.cmd producer consumer

