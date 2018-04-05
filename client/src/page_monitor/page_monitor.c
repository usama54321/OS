


/*

	DESCRIPTION:
		Kernel module to monitor page activity

*/

#ifndef PAGE_MONITOR_C
#define PAGE_MONITOR_C



/*
 * Defined if we are comparing page data to
 * determine page modification rather than
 * page table entry bits. Use this implemen-
 * tation until we find a reliable one using
 * flags.
 */
#define PAGE_MONITOR_FULLCOMPARE



#include <asm/highmem.h>

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/moduleparam.h>

#include "../pte_funcs/pte_funcs.h"
#include "../page_monitor/page_monitor.h"
#include "../symfind/symfind.h"



#ifndef PAGE_MONITOR_FULLCOMPARE
#error Full page comparison must be enabled
#endif /* PAGE_MONITOR_FULLCOMPARE */



#define MAX_RESCHEDULES 256



struct page_monitor_info {
	pgd_t *pgd;
	unsigned long addr;
	page_monitor_cb_t callback;
	void *cb_data;
};



DEFINE_SPINLOCK(mon_lock);



#ifdef PAGE_MONITOR_FULLCOMPARE

static int pages_equal(char *p1, char *p2) {

	int retval = (memcmp(p1, p2, PAGE_SIZE) == 0) ? 1 : 0;

	return retval;

}

#endif /* PAGE_MONITOR_FULLCOMPARE */



/*
 * Page monitor thread:
 *    Wait until the page containing the virtual address
 *    info->addr is no longer being modified, then call
 *    info->callback with argument info->cb_data. We are
 *    only concerned with detecting write inactivity and
 *    do not guarantee that the callback reads the same
 *    page data as of the time of detection.
 */
static int __waitout_write(void *cb_data) {



#define __DBG(x) printk(KERN_INFO "_DBG::Here %d...", x);

#ifdef PAGE_MONITOR_FULLCOMPARE
#define __EXIT(x) {			\
	kfree(old_page);		\
	kfree(new_page);		\
	return x;			\
}
#else /* PAGE_MONITOR_FULLCOMPARE */
#define __EXIT(x) return x;
#endif /* PAGE_MONITOR_FULLCOMPARE */

// #define __ERROR_EXIT break;
#define __ERROR_EXIT __EXIT(-1)



	struct page_monitor_info *info =
		(struct page_monitor_info*)cb_data;
	int remaining_reschedules = MAX_RESCHEDULES;

#ifdef PAGE_MONITOR_FULLCOMPARE

	char *old_page;
	char *new_page;

	if ( !(old_page = kmalloc(PAGE_SIZE, GFP_KERNEL)) )
		return -1;

	if ( !(new_page = kmalloc(PAGE_SIZE, GFP_KERNEL)) ) {
		kfree(old_page);
		return -1;
	}

	if ( get_page_data(info->pgd, info->addr, new_page) < 0 ) {
		printk(KERN_ERR "page_monitor: Faulty page table, initial page fetch failed");
		__ERROR_EXIT;
	}

#endif /* PAGE_MONITOR_FULLCOMPARE */

	// spin_lock(&mon_lock);

	while ( remaining_reschedules --> 0 ) {

#ifdef PAGE_MONITOR_FULLCOMPARE

		int page_inactive;

		memcpy(old_page, new_page, PAGE_SIZE);

#else /* PAGE_MONITOR_FULLCOMPARE */

		int page_dirty;

		/* Assume clean page */
		if ( for_pte_pgd(info->pgd, info->addr, __hga_reset) < 0 ) {
			printk(KERN_ERR "page_monitor: Faulty page table, __hga_reset failed");
			__ERROR_EXIT;
		}
		printk(KERN_INFO "page_monitor[%p]: Cleaned page", (void*)(info->addr));

#endif /* PAGE_MONITOR_FULLCOMPARE */

		/* Resume the writing process */
		schedule();

#ifdef PAGE_MONITOR_FULLCOMPARE

		if ( get_page_data(info->pgd, info->addr, new_page) < 0 ) {
			printk(KERN_ERR "page_monitor: Faulty page table, new page fetch failed");
			__ERROR_EXIT;
		}

		/* Check if page was modified while we were asleep */
		page_inactive = pages_equal(old_page, new_page);

		if ( page_inactive )
			break; /* Assume the process stopped writing */

#else /* PAGE_MONITOR_FULLCOMPARE */

		/* Check if page was modified while we were asleep */
		if ( (page_dirty = for_pte_pgd(info->pgd, info->addr, __hga_written)) < 0 ) {
			printk(KERN_ERR "page_monitor: Faulty page table, __hga_written failed");
			__ERROR_EXIT;
		}

		/*
		 * FIXME:
		 *    The page is reported clean on the first check even on long loops
		 *    Either usleep_range is sleeping without interrupts, or the dirty
		 *    bit is not being set!
		 */
		printk(KERN_INFO "page_monitor[%p]: Dirty status = %d", (void*)(info->addr), page_dirty);

		if ( !page_dirty )
			break; /* Assume the process stopped writing */

#endif /* PAGE_MONITOR_FULLCOMPARE */

	}

	// spin_unlock(&mon_lock);

	info->callback(info->cb_data);

	kfree(info);

	__EXIT(0);

	/* To make GCC happy */
	for_pte_pgd(0, 0, 0);



#undef __ERROR_EXIT
#undef __EXIT
#undef __DBG



}



int get_page_data(pgd_t *pgd, unsigned long addr, char *page_buf) {

	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pte_entry;
	struct page *page;

	pgd_entry = pgd + pgd_index(addr);
	if ( pgd_none(*pgd_entry) || pgd_bad(*pgd_entry) )
		return -1;

	pud_entry = pud_offset(pgd_entry, addr);
	if ( pud_none(*pud_entry) || pud_bad(*pud_entry) )
		return -1;

	pmd_entry = pmd_offset(pud_entry, addr);
	if ( pmd_none(*pmd_entry) || pmd_bad(*pmd_entry) )
		return -1;

	pte_entry = pte_offset_kernel(pmd_entry, addr);
	if ( !pte_entry )
		return -1;

	page = pte_page(*pte_entry);
	memcpy(page_buf, page_address(page), PAGE_SIZE);

	pte_unmap(pte_entry);

	return 0;

}

int set_page_data(pgd_t *pgd, unsigned long addr, char *page_buf) {

	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pte_entry;
	struct page *page;

	pgd_entry = pgd + pgd_index(addr);
	if ( pgd_none(*pgd_entry) || pgd_bad(*pgd_entry) )
		return -1;

	pud_entry = pud_offset(pgd_entry, addr);
	if ( pud_none(*pud_entry) || pud_bad(*pud_entry) )
		return -1;

	pmd_entry = pmd_offset(pud_entry, addr);
	if ( pmd_none(*pmd_entry) || pmd_bad(*pmd_entry) )
		return -1;

	pte_entry = pte_offset_kernel(pmd_entry, addr);
	if ( !pte_entry )
		return -1;

	page = pte_page(*pte_entry);
	memcpy(page_address(page), page_buf, PAGE_SIZE);

	pte_unmap(pte_entry);

	return 0;

}

/* Run __waitout_write in a kthread */
int page_monitor_waitout_write(pgd_t *pgd, unsigned long addr,
	page_monitor_cb_t callback, void *cb_data) {

	struct page_monitor_info *info;

	info = kmalloc(sizeof(struct page_monitor_info), GFP_KERNEL);

	info->pgd = pgd;
	info->addr = addr;
	info->callback = callback;
	info->cb_data = cb_data;

	if ( kthread_run(__waitout_write, info, "Page monitor thread") ) {
		printk(KERN_INFO "Thread creation successful!");
		return 0;
	} else {
		printk(KERN_ERR "Thread creation failed!");
		return -1;
	}

}



MODULE_LICENSE("Dual BSD/GPL");



#undef PAGE_MONITOR_FULLCOMPARE



#endif /* PAGE_MONITOR_C */



