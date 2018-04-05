


#ifndef HANDLE_LOCK_READ_C
#define HANDLE_LOCK_READ_C



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"
#include "../ev_handlers/ev_handlers.h"



struct handler_ctx {

};



// CONTINUE FILLING IN THIS FUNCTION...
static int start_readlock(unsigned long vaddr, char *procname,
	struct proc_cache_ctx *cachectx) {

	pgd_t *pgd;
	int readlocked;
	struct handler_ctx *ctx;

	if ( proc_cache_retrieve_by_name(cachectx, procname, NULL, &pgd, NULL) != 1 )
		return -1;

	if ( (readlocked = for_pte_pgd(pgd, vaddr, __hga_readlocked)) < 0 ) {
		// Insert this page into the readlock list
		return 0;
	}

	if ( readlocked )
		/* Already locked, possible duplicate */
		return 0;

	printk(KERN_INFO "Read-locking page %p", (void*)vaddr);

	if ( for_pte_pgd(pgd, vaddr, __hga_readlock) < 0 )
		/* for_pte_pgd should have failed before */
		return -1;

	return 0;

}

srvcom_ackcode_t handle_ev_lock_read(struct srvcom_ctx *ctx,
	unsigned long vaddr, char *procname, char *pagedata, void *cb_data) {

	void **args = (void**)cb_data;

	struct proc_cache_ctx *cachectx =
		(struct proc_cache_ctx*)args[0];

	if ( start_readlock() < 0 )
		return ACKCODE_OP_FAILURE;

	return ACKCODE_LOCK_READ;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_LOCK_READ_C */



