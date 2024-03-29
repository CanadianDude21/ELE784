ifneq ($(KERNELRELEASE),)
# call from kernel build system

obj-m := pilote_serie.o
pilote_serie-y := driver784.o bufcirc.o port_config.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif


clean:
	rm -rf *.mod.c .*.cmd *.o *~ core .tmp_versions 
