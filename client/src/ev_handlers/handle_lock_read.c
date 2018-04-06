


#ifndef HANDLE_LOCK_READ_C
#define HANDLE_LOCK_READ_C



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"
#include "../ev_handlers/ev_handlers.h"
#include "../readlock_list/readlock_list.h"



struct cb_data_t {

	pgd_t *pgd;
	pfn_t pfn;

};



static int match_pending_page(struct readlock *readlock, void *data) {

	struct cb_data_t *cb_data =
		(struct cb_data_t*)data;

	if ( readlock->resolved )
		return 0;
	if ( readlock->pgd != cb_data->pgd )
		return 0;
	if ( readlock->pfn != cb_data->pfn )
		return 0;

	return 1;

}

static int is_pending(struct readlock_list *list, pgd_t *pgd, pfn_t pfn) {

	struct cb_data_t cb_data =
		{.pgd = pgd, .pfn = pfn};
	struct readlock *pending =
		readlock_list_find(list, match_pending_page, &cb_data);

	return pending ? 1 : 0;

}

static int start_readlock(unsigned long vaddr, pgd_t *pgd,
	struct readlock_list *pending_readlocks) {

	pfn_t pfn =
		{.val = vaddr>>PAGE_SHIFT};
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
		if ( is_pending(pending_readlocks, pgd, pfn) )
			/* Do not duplicate readlocks! */
			return 0;
		if ( !(new_pending = readlock_new()) )
			return -1;
		new_pending->pgd = pgd;
		new_pending->pfn = pfn;
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

/*

Writing:
	Writes disabled by default
	Process triggers fault handler
	Fault handler requests write
	Allow handler grants write
	Allow handler revokes write

Reading:
	Reads enabled by default
	Revoke handler tries to block read (success):
		- Process triggers fault handler
		- Fault handler denies read
		- Allow handler unblocks read
		- Allow handler can fail too!
	Revoke handler tries to block read (failure):
		- // To be continued

*/

srvcom_ackcode_t handle_ev_lock_read(struct srvcom_ctx *ctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata,
	void *cb_data) {

	struct readlock_list *pending_readlocks =
		(struct readlock_list*)cb_data;

	if ( start_readlock(vaddr, pgd, pending_readlocks) < 0 )
		return ACKCODE_OP_FAILURE;

	return ACKCODE_LOCK_READ;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_LOCK_READ_C */



