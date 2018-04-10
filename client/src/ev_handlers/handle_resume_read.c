


/*
 *
 * NOTE:
 *
 *    Allow handlers can fail if the page is not present.
 *
 *    This does not cause problems for the writelocking
 *    part since failing some times is feasible for the
 *    writelocker. The server will get a failure reply
 *    and continue to look for other write requests.
 *
 *    Though this should not be done for reading. If the
 *    readlocker handler (the function that locks reads
 *    on the server's request) fails with probability p
 *    then with 1/p machines, 1 is expected to fail for
 *    every readlock multicast. We don't want to expect
 *    one failure whenever a machine requests for write
 *    permission. We must send a success reply whether
 *    or not the PTE modification succeeded.
 *
 *    No problems with write control:
 *        Writes disabled by default
 *        Process triggers fault handler
 *        Fault handler requests write
 *        Allow handler grants write
 *        Allow handler revokes write
 *
 *    Problems with read control without a pending readlock list:
 *        Reads enabled by default
 *        Revoke handler tries to block read (success):
 *            - Process triggers fault handler
 *            - Fault handler denies read
 *            - Allow handler unblocks read (success):
 *                o All goes good
 *            - Allow handler unblocks read (failure):
 *                o If we enlisted the readlock we can unlock the
 *                  page at the next readlock-generated fault if
 *                  the readlock was resolved (we have this flag).
 *                o If we did not then the page is fucked forever.
 *        Revoke handler tries to block read (failure):
 *            - Process triggers fault handler due to
 *              page not present
 *                o If we enlisted the readlock, the fault handler
 *                  can prevent making the page available if the
 *                  readlock was not resolved and could make the
 *                  page available otherwise.
 *                o If we did not then the process can read wrong
 *                  page data.
 *            - Assume fault handler successfully denies
 *              read
 *            - Allow handler unblocks read (success):
 *                o All goes good
 *            - Allow handler unblocks read (failure):
 *                o Using readlock lists we simply resolve the node
 *                  blocking the faults.
 *                o Without lists we couldn't even proceed after the
 *                  first step.
 *
 *    Solution with pending readlock list:
 *        - Reads allowed by default
 *        - Server sends readlock command
 *        - Peer tries to lock read (success or failure,
 *          the process will trigger the fault handler
 *          to access the page anyway)
 *        - Peer adds readlock entry to pending list
 *        - Process triggers the fault handler for that
 *          page
 *        - Page fault handler gets a read violation
 *          and allows/disallows based on the pending
 *          readlocks (and commits a readlock if it
 *          was resolved)
 *        - Page fault handler gets a "not present"
 *          violation and resolves/does not resolve
 *          based on the pending readlocks (again,
 *          committing the readlock if resolved)
 *        - Server sends a resume command
 *        - Peer marks readlock resolved
 *        - Process triggers fault handler
 *        - Fault handler sees resolved page
 *        - Fault handler commits resolved page
 *        - Fault handler resumes read by either making
 *          the page available or removing the readlock
 *
 *    SUBNOTE:
 *        Committing a readlock is writing the new
 *        page data and deleting the readlock from
 *        the list...
 *
 * TL;DR we must keep a record of readlocked pages
 * to ensure reliable and fault tolerant page reads
 *
 */



#ifndef HANDLE_RESUME_READ_C
#define HANDLE_RESUME_READ_C



#include <linux/pfn_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/pgtable_types.h>

#include "../srvcom/srvcom.h"
#include "../ev_handlers/ev_handlers.h"
#include "../readlock_list/readlock_list.h"



static int resolve_readlock(unsigned long vaddr, pgd_t *pgd,
	char *pagedata, struct readlock_list *pending_readlocks) {

	const pfn_t pfn =
		{.val = vaddr>>PAGE_SHIFT};

	printk(KERN_INFO "Resolving page %p", (void*)vaddr);

	/* Just unresolve and leave the rest to the page fault handler */
	if ( readlock_list_resolve(pending_readlocks, pgd, pfn, pagedata) < 0 )
		return -1;

	return 0;

}

srvcom_ackcode_t handle_ev_resume_read(struct srvcom_ctx *ctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata,
	void *cb_data) {

	struct readlock_list *pending_readlocks =
		(struct readlock_list*)cb_data;

	if ( resolve_readlock(vaddr, pgd, pagedata, pending_readlocks) < 0 )
		return ACKCODE_OP_FAILURE;

	return ACKCODE_RESUME_READ;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_RESUME_READ_C */



