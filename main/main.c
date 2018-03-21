#include <linux/module.h>
#include <linux/kernel.h>
#include "util.h"

#define HIJACK_SIZE 12

static int __init up_megavm(void) {
    
    struct my_hook temp = {
        .name="test"
    };
    int a = find_sym_address(&temp);
	printk(KERN_INFO "moudle loaded!");
	return 0;
}

static void __exit down_megavm(void) {
    printk(KERN_INFO "module unloaded");
	return;
}

MODULE_LICENSE("GPL v2");

module_init(up_megavm);
module_exit(down_megavm);

