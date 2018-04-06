


#ifndef HANDLE_RESUME_READ_C
#define HANDLE_RESUME_READ_C



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"
#include "../ev_handlers/ev_handlers.h"
#include "../readlock_list/readlock_list.h"



static int stop_readlock(unsigned long vaddr, pgd_t *pgd,
	char *pagedata, struct readlock_list *pending_readlocks) {

	int readlocked;

	if ( (readlocked = for_pte_pgd(pgd, vaddr, __hga_readlocked)) < 0 ) {
		/*
		 * If the PTE fetch fails then there's a good chance
		 * the reason of failure was a missing table in the
		 * page traversal. If this is the case, and we leave
		 * the page unlocked, a process can read the page at
		 * some point in time before the writer process is
		 * done writing. To prevent this we record the page
		 * to be read-locked so that the page fault handler
		 * ignores its read requests during this time.
		 */
		struct readlock *new_pending;
		if ( !(new_pending = readlock_new()) )
			return -1;
		new_pending->pgd = pgd;
		new_pending->pfn = {.val = vaddr>>PAGE_SHIFT};
		readlock_list_insert(pending_readlocks, new_pending);
		return 0;
	}

	if ( readlocked )
		/* Already locked, possible duplicate */
		return 0;

	printk(KERN_INFO "Read-locking page %p", (void*)vaddr);

	if ( for_pte_pgd(pgd, vaddr, __hga_readlock) < 0 )
		/* for_pte_pgd should have failed before though */
		return -1;

	return 0;

}

srvcom_ackcode_t handle_ev_resume_read(struct srvcom_ctx *ctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata,
	void *cb_data) {

	struct readlock_list *pending_readlocks =
		(struct readlock_list*)cb_data;

	if ( stop_readlock(vaddr, pgd, pagedata, pending_readlocks) < 0 )
		return ACKCODE_OP_FAILURE;

	return ACKCODE_RESUME_READ;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_RESUME_READ_C */



