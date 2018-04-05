


# Initialization
NAME=ttyfivele
obj-m += ttyfivele.o
ttyfivele-objs := main.o



build:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
	rm -rf build
	mkdir build
	mv *.mod.c *.ko .*.cmd Module* module*  *.o .tmp_versions build
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	rm -rf build
rebuild: clean build



load:
	sudo insmod ./build/$(NAME).ko
unload:
	sudo rmmod $(NAME)
reload: unload load



#-I/lib/modules/4.4.113/build/include -I/lib/modules/4.4.113/build/arch/x86/include
