#include <linux/module.h>
#include <linux/kernel.h>
#include "util.h"

typedef int (*swap_handler_custom) (struct mm_struct*, struct vm_area_struct*, unsigned long, unsigned int);

struct my_hook temp = {
        .name="handle_mm_fault"
};

static int my_temp(struct mm_struct *mm, struct vm_area_struct *vma, unsigned long address, unsigned int flags) {
	//printk(KERN_INFO "hooked successfully");
    hijack_pause((void*)temp.address);
    int result = ((swap_handler_custom)temp.address)(mm, vma, address, flags);
    hijack_resume((void*)temp.address);
	return 1;
}


static int __init up_megavm(void) {
    
    
    int a = find_sym_address(&temp);
    if (temp.address == 0) {
        printk(KERN_INFO "Could not find page fault  handler");
        return 0;
    }

    printk(KERN_INFO "address1: %lu", temp.address);
    hijack_start((void*) temp.address,(void*) my_temp);
	printk(KERN_INFO "moudle loaded!");
	return 0;
}

static void __exit down_megavm(void) {
	hijack_stop((void*) temp.address); 
    printk(KERN_INFO "module unloaded");
	return;
}

MODULE_LICENSE("GPL v2");

module_init(up_megavm);
module_exit(down_megavm);
