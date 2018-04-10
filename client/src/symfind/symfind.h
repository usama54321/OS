


#ifndef SYMFIND_H
#define SYMFIND_H



#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <asm/cacheflush.h>



unsigned long find_sym_address(char *name);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* SYMFIND_H */



