
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "partial_hooker.h"

// typedef void (*trapfunc)(struct pt_regs *ptregs, long);

char *message = KERN_ALERT "Called divide error handler...***********(((((((((((((((((()))))))))))))))))))))*********************(((((((((((((((((((()))))))))))))))))*****************))))))))))))((((((((((((";

unsigned long long dde_addr = 0xFFFFFFFF8102EA50;
static void overrider(struct pt_regs *regs, long bong) {

	// trapfunc original_dde = (trapfunc)dde_addr;
/*
	siginfo_t info = {
		.si_code = FPE_INTDIV
	};
*/
	printk(message);
	// printk(KERN_ALERT "Called divide error handler...***********(((((((((((((((((()))))))))))))))))))))*********************(((((((((((((((((((()))))))))))))))))*****************))))))))))))((((((((((((");

	// force_sig_info(SIGFPE, &info, current);

	return;

}

struct hook_ctx hctx;

static int __init up_megavm(void) {

	struct my_hook temp = {
		.name = "do_divide_error"
	};

	if ( !find_sym_address(&temp) || !temp.found ) {
		printk(KERN_INFO "\nfind_sym_address: No symbol found\n\n");
		return -1;
	}

	dde_addr = temp.address;

	// hijack_start((void*)dde_addr, (void*)overrider);
	hook_routine(&hctx, (trapfunc)dde_addr, overrider, NULL);

	printk(KERN_INFO "\nmodule loaded!\n\n");

	return 0;

}

static void __exit down_megavm(void) {

	unhook_routine(&hctx);

	printk(KERN_INFO "\nmodule unloaded\n\n");

	return;

}

MODULE_LICENSE("GPL");

module_init(up_megavm);
module_exit(down_megavm);

