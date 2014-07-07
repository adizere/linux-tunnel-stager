obj-m := ipip.o

# ipip-y := ipip_stager.o

# ipip_stager.o: ipip_stager.c ipip_stager.h

CMD	:= $(MAKE)
KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(CMD) -C $(KDIR) M=$(PWD) modules

clean:
	$(CMD) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
