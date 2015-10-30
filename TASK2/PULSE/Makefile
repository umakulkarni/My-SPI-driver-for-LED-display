obj-m := pulse.o

ARCH=x86
CROSS_COMPILE=i586-poky-linux-

all:
	make -C /home/uma/SDK/sysroots/i586-poky-linux/usr/src/kernel M=$(PWD) modules


pc:
	make -C /usr/src/linux-headers-$(shell uname -r) SUBDIRS=$(PWD) modules
	
clean:
	rm -rf *.o *.ko *.mod.* *.symvers *.order *~








