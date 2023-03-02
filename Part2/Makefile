obj-m += my_timer.o
PWD := `pwd`
KERNELDIR := /lib/modules/`uname -r`/build

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)  modules

clean:
	rm -f *.ko *.o Module* *mod*
