#include <linux/module.h>
#include <linux/kernel.h>
#include "./hashtable/hashtable.h"
#include "main.h"

static int server_init(void){
   
    int pass;
    printk(KERN_INFO "megavm_server: Init.\n");
    hashtables_init();
    pass = hashtable_tests();
    if(!pass) {
        printk(KERN_INFO "tests failed");
        return 1;
    }
    return 0;
}


static void server_down(void) {
   printk("megavm_server down"); 
}

module_init(server_init);
module_exit(server_down);

MODULE_LICENSE("Dual BSD/GPL");
