


#ifndef SYMFIND_H
#define SYMFIND_H



#include <linux/types.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>

#include <linux/ftrace.h>



unsigned long find_sym_address(char *name);



#endif /* SYMFIND_H */



