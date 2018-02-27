#include "util.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init up(void) {

    return 0;
}

static void __exit down(void) {
    printk(KERN_INFO "exit");
}

module_init(up);
module_exit(down);
