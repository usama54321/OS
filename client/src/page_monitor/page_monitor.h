


#ifndef PAGE_MONITOR_H
#define PAGE_MONITOR_H



#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>



typedef int (*page_monitor_cb_t)(void*);



int get_page_data(pgd_t *pgd, unsigned long addr, char *page_buf);
int set_page_data(pgd_t *pgd, unsigned long addr, char *page_buf);
int page_monitor_waitout_write(pgd_t *pgd, unsigned long addr,
	page_monitor_cb_t callback, void *cb_data);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* PAGE_MONITOR_H */



