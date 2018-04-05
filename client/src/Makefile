


# Initialization
NAME=megavm_hga
obj-m += megavm_hga.o
megavm_hga-objs :=			\
	page_monitor/page_monitor.o	\
	pte_funcs/pte_funcs.o		\
	symfind/symfind.o		\
	page_fault/page_fault.o		\
	main.o
#megavm_hga-objs := $(patsubst %.c,%.o,$(wildcard *.c))



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



#-I/lib/modules/4.4.113/build/include -I/lib/modules/4.4.113/build/arch/x86/include
