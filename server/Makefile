


# Initialization
NAME=megavm_server
obj-m += megavm_server.o
megavm_server-objs :=				\
	server/server.o				\
	comm/comm.o				\
	ksock/ksock_socket.o			\
	ksock/ksock_select.o			\
	hashtable/hashtable.o			\
	ev_handlers/handle_commit_page.o \
	ev_handlers/handle_initial_read.o \
	ev_handlers/handle_request_write.o \
	ev_handlers/helpers.o			\
	tests/test.o				\
	pgtable/pgtable.o			\
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



