


# Initialization
NAME=megavm_server
obj-m += megavm_server.o
megavm_server-objs :=				\
	server/server.o				\
	hashtable/hashtable.o		\
	tests/test.o				\
	main.o



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
	sudo insmod build/$(NAME).ko
unload:
	sudo rmmod $(NAME)
reload: unload load



