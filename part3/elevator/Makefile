obj-m += elevator.o

KDIR := /usr/src/linux-6.17.3

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C ($KDIR) M=$(PWD) clean

install:
	make -C ($KDIR) M=$(PWD) module_install

load:
	sudo insomod elevator.ko

unload:
	sudo rmmod elevator