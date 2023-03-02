obj-m += my_timer.o
PWD := `pwd`
KERNELDIR := /lib/modules/`uname -r`/build

<<<<<<< HEAD
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)  modules

=======
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
>>>>>>> OSProj2/Pietro
clean:
	rm -f *.ko *.o Module* *mod*
