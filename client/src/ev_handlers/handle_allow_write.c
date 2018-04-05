


#ifndef HANDLE_ALLOW_WRITE_C
#define HANDLE_ALLOW_WRITE_C



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"
#include "../pte_funcs/pte_funcs.h"
#include "../ev_handlers/ev_handlers.h"
#include "../page_monitor/page_monitor.h"



struct handler_ctx {

	pid_t pid;
	pgd_t *pgd;
	unsigned long vaddr;
	struct srvcom_ctx *srvctx;

};



static int resume_writelock(void *cb_data) {

	int ret_code;
	char *modified_page;
	struct handler_ctx *ctx =
		(struct handler_ctx*)cb_data;

	printk(KERN_INFO "resume_writelock called");

	if ( !(modified_page = kmalloc(PAGE_SIZE, GFP_KERNEL)) ) {
		kfree(ctx);
		return -1;
	}

	/* Lock the page to prevent modification */
	if ( for_pte_pgd(ctx->pgd, ctx->vaddr, __hga_writelock) < 0 ) {
		// Shouldn't happen
		printk(KERN_ERR "WARNING: Re-locking failed after "
			"writelock suspension...");
		kfree(modified_page);
		kfree(ctx);
		return -1;
	}

	/* Get the modified page data */
	if ( get_page_data(ctx->pgd, ctx->vaddr, modified_page) < 0 ) {
		// Shouldn't happen
		printk(KERN_ERR "WARNING: Page fetch failed after "
			"writelock suspension...");
		kfree(modified_page);
		kfree(ctx);
		return -1;
	}

	/* Send it off to the server */
	ret_code = srvcom_commit_page(ctx->srvctx,
		ctx->vaddr, ctx->pid, ctx->pgd, modified_page);

	kfree(modified_page);
	kfree(ctx);

	return ret_code;

}

static int suspend_writelock(unsigned long vaddr, pid_t pid,
	pgd_t *pgd, char *pagedata, struct srvcom_ctx *srvctx) {

	int writelocked;
	struct handler_ctx *ctx;

	if ( (writelocked = for_pte_pgd(pgd, vaddr, __hga_writelocked)) < 0 )
		return -1;

	if ( !writelocked )
		/* Already suspended, possible duplicate */
		return 0;

	printk(KERN_INFO "Unlocking page %p and starting "
		"page monitor thread...", (void*)vaddr);

	if ( !(ctx = kmalloc(sizeof(struct handler_ctx), GFP_KERNEL)) )
		return -1;
	ctx->pid = pid;
	ctx->pgd = pgd;
	ctx->vaddr = vaddr;
	ctx->srvctx = srvctx;

	if ( for_pte_pgd(pgd, vaddr, __hga_writeunlock) < 0 )
		return -1;

	if ( page_monitor_waitout_write(pgd, vaddr, resume_writelock, ctx) < 0 )
		return -1;

	return 0;

}

srvcom_ackcode_t handle_ev_allow_write(struct srvcom_ctx *srvctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata, void *cb_data) {

	if ( suspend_writelock(vaddr, pid, pgd, pagedata, srvctx) < 0 )
		return ACKCODE_OP_FAILURE;

	return ACKCODE_ALLOW_WRITE;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_ALLOW_WRITE_C */



