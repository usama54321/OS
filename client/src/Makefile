


# Initialization
NAME=megavm_hga
obj-m += megavm_hga.o
megavm_hga-objs :=				\
	ev_handlers/handle_allow_write.o	\
	ev_handlers/handle_lock_read.o		\
	ev_handlers/handle_ping_alive.o		\
	ev_handlers/handle_resume_read.o	\
	ksock/ksock_socket.o			\
	ksock/ksock_select.o			\
	readlock_list/readlock_list.o		\
	srvcom/srvcom.o				\
	task_funcs/task_funcs.o			\
	page_monitor/page_monitor.o		\
	pte_funcs/pte_funcs.o			\
	symfind/symfind.o			\
	page_fault/page_fault.o			\
	main.o



build:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
	rm -rf ../build
	mkdir ../build
	mv *.mod.c *.ko .*.cmd Module* module*  *.o .tmp_versions ../build
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
	rm -rf ../build
rebuild: clean build



load:
	sudo insmod ../build/$(NAME).ko
unload:
	sudo rmmod $(NAME)
reload: unload load



