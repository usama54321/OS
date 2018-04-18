#include <linux/module.h>
#include <linux/kernel.h>
#include "./hashtable/hashtable.h"
#include "main.h"

static int server_init(void){
   
    printk(KERN_INFO "megavm_server: Init.\n");
    hashtable_init();
    if (!init_server()) {
        printk(KERN_INFO "failed to initialize server");
        return 1;
    }
    /*
    pass = hashtable_tests();
    if(!pass) {
        printk(KERN_INFO "tests failed");
        return 1;
    }
    */
    return 0;
}


static void server_down(void) {
   printk("megavm_server down"); 
}

module_init(server_init);
module_exit(server_down);

MODULE_LICENSE("Dual BSD/GPL");
