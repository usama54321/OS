


#ifndef TASK_FUNCS_H
#define TASK_FUNCS_H



#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/pgtable_types.h>

#include "../common/hga_defs.h"



int task_targeted(struct task_struct *task);
int task_get_name(struct task_struct *task, char *name);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* TASK_FUNCS_H */



