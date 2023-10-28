KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := pmccntr.o

.PHONY: all
all:
	make -C $(KDIR) M=$(PWD) modules

.PHONY: clean
clean:
	make -C $(KDIR) M=$(PWD) clean
