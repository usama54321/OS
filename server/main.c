#include <linux/module.h>
#include <linux/kernel.h>
#include "./hashtable/hashtable.h"

int init(void){
    printk(KERN_INFO "megavm_server: Init.\n");
    init_pid_hashtable();
    init_vaddr_hashtable();
    return 0;
}
