
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

char *exec_page;

static int call_func(void) {

	int i, sum = 0;
	for (i=0; i<1024; i++)
		if (i % 3 == 0)
			sum -= i;
		else
			sum += i;

	return sum;

}

typedef int (*func_t)(void);

static int __init up_megavm(void) {

	func_t fun;
	int result;

	if ( !(exec_page = __vmalloc(PAGE_SIZE, GFP_KERNEL, PAGE_KERNEL_EXEC)) ) {
		printk(KERN_INFO "\nfind_sym_address: No symbol found\n\n");
		return -1;
	}

	memcpy(exec_page, call_func, PAGE_SIZE);

	fun = (func_t)exec_page;

	printk(KERN_ALERT "Page starts at address %p...", exec_page);

	result = fun();

	printk(KERN_ALERT "Got result %d...", result);

	return 0;

}

static void __exit down_megavm(void) {

	if ( !exec_page )
		vfree(exec_page);

	printk(KERN_INFO "\nmodule unloaded\n\n");

	return;

}

MODULE_LICENSE("GPL");

module_init(up_megavm);
module_exit(down_megavm);

