


#ifndef PAGE_FAULT_C
#define PAGE_FAULT_C



#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/pfn.h>
#include <linux/percpu.h>
#include <linux/gfp.h>
#include <linux/pci.h>
#include <linux/vmalloc.h>

#include <asm/processor.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <linux/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/proto.h>
#include <asm/pat.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/traps.h>
#include <asm/pgtable_types.h>
#include <asm/desc_defs.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>

#include "../srvcom/srvcom.h"
#include "../common/hga_defs.h"
#include "../symfind/symfind.h"
#include "../pte_funcs/pte_funcs.h"
#include "../task_funcs/task_funcs.h"
#include "../ev_handlers/ev_handlers.h"
#include "../page_monitor/page_monitor.h"
#include "../readlock_list/readlock_list.h"

// PGFAULT_NR is the interrupt number of page fault. It is platform specific.
#if defined(CONFIG_X86_64)
#define PGFAULT_NR X86_TRAP_PF
#else
#error This module is only for X86_64 kernel
#endif

static unsigned long new_idt_table_page;
static struct desc_ptr default_idtr;

//addresses of some symbols
unsigned long addr_dft_page_fault = 0UL;		//address of default 'page_fault'
unsigned long addr_dft_do_page_fault = 0UL;	//address of default 'do_page_fault'
unsigned long addr_pv_irq_ops = 0UL;		//address of 'pv_irq_ops'
unsigned long addr_adjust_exception_frame;	//content of pv_irq_ops.adjust_exception_frame, it's a function
unsigned long addr_error_entry = 0UL;
unsigned long addr_error_exit = 0UL;

module_param(addr_dft_page_fault, ulong, S_IRUGO);
module_param(addr_dft_do_page_fault, ulong, S_IRUGO);
module_param(addr_pv_irq_ops, ulong, S_IRUGO);
module_param(addr_error_entry, ulong, S_IRUGO);
module_param(addr_error_exit, ulong, S_IRUGO);

#define CHECK_PARAM(x) do{\
    if(!x){\
        printk(KERN_INFO "my_virt_drv: Error: need to set '%s'\n", #x);\
        is_any_unset = 1;\
    }\
    printk(KERN_INFO "my_virt_drv: %s=0x%lx\n", #x, x);\
} while(0)

static int check_parameters(void){
    int is_any_unset = 0;
    CHECK_PARAM(addr_dft_page_fault);
    CHECK_PARAM(addr_dft_do_page_fault);
    CHECK_PARAM(addr_pv_irq_ops);
    CHECK_PARAM(addr_error_entry);
    CHECK_PARAM(addr_error_exit);
    return is_any_unset;
}

typedef void (*do_page_fault_t)(struct pt_regs*, unsigned long);



/////////////////////////////////////////////
// NO HGA CODE BEFORE THIS COMMENT BARRIER //
// NO HGA CODE BEFORE THIS COMMENT BARRIER //
// NO HGA CODE BEFORE THIS COMMENT BARRIER //
/////////////////////////////////////////////



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

/*
 * XXX:
 *    - For now we're assuming a unique PID
 *      for every process name and that the
 *      same PID will not be used for future
 *      processes.
 */



#define SERVER_IP	"192.168.1.2"
#define SERVER_PORT	1324



/* Globals */
static struct srvcom_ctx *srvctx;
static struct readlock_list *pending_readlocks;



/* Initialization */
static int __init_srvcom(void);
static int __init_readlocks(void);
static int my_fault_init(void);
/* Deinitialization */
static void __exit_srvcom(void);
static void __exit_readlocks(void);
static void my_fault_exit(void);
/* For dealing with read faults */
static inline void __handle_usermode_read_violation(pgd_t *pgd,
	unsigned long pf_vaddr, struct pt_regs* regs, unsigned long error_code);
static inline void __handle_usermode_read_missingpage(pgd_t *pgd,
	unsigned long pf_vaddr, struct pt_regs* regs, unsigned long error_code);
static void __handle_usermode_read(pgd_t *pgd, unsigned long pf_vaddr,
	struct pt_regs* regs, unsigned long error_code);



#define FOR_PTE(func) \
	(for_pte_pgd(pgd, pf_vaddr, __hga_##func))

#define ACCESS_VIOLATE	(1 << 0)
#define WRITE_ATTEMPT	(1 << 1)
#define USERMODE	(1 << 2)
#define ERRCODE_MASK \
	(ACCESS_VIOLATE|WRITE_ATTEMPT|USERMODE)

#define IS_USERMODE_WRITE_VIOLATION(x) \
	(((x)&ERRCODE_MASK) == (ACCESS_VIOLATE|WRITE_ATTEMPT|USERMODE))
#define IS_USERMODE_READ(x) \
	(((x)&(WRITE_ATTEMPT|USERMODE)) == USERMODE)

void my_do_page_fault(struct pt_regs* regs, unsigned long error_code) {

	int marked, shareable;
	struct task_struct *task = current;
	unsigned long pf_vaddr = read_cr2();
	do_page_fault_t pfault =
		(do_page_fault_t)addr_dft_do_page_fault;
	pid_t pid = task->pid;
	pgd_t *pgd = task->mm->pgd;

	if ( task_targeted(task) != 1 ) {
		/* Error or not a target process */
		pfault(regs, error_code);
		return;
	}

	/* -1 on error, 0 if the page is not to be shared and 1 otherwise */
	shareable = FOR_PTE(shareable);

	/* TODO: Identify the segment and remove this garbage */
	if ( shareable == 0 ) {
		pfault(regs, error_code);
		return;
	}

	if ( IS_USERMODE_READ(error_code) ) {
		__handle_usermode_read(pgd, pf_vaddr, regs, error_code);
		return;
	}

	/*
	 * Some pages are apparently accessible but for_pte fails to
	 * fetch a page table entry before the first page fault for
	 * that page, and hence fails to determine its shareability
	 * status. If it fails, we execute the default handler and re-
	 * attempt the fetch. If it fails on the second attempt then
	 * we assume inaccessibility.
	 */
	/* TODO: Test first and then use the present flag instead */
	if ( shareable < 0 ) {
		pfault(regs, error_code);
		/* Second attempt */
		if ( (shareable = FOR_PTE(shareable)) <= 0 )
			return; /* Either still not available or available and not shareable */
	}

	/* At this point the page fault is from our target process and involves a shareable page */

	/* TODO: Test for whether we even need this */
	marked = FOR_PTE(marked);

	/*
	 * If the fault was not generated by a userspace
	 * write-access permission violation or originated
	 * from an unassociated page then just prepare the
	 * page for interception and leave without taking
	 * further action.
	 */
	if ( !marked || !IS_USERMODE_WRITE_VIOLATION(error_code) ) {
		pfault(regs, error_code);
		if ( error_code&USERMODE ) { /* Only deal with usermode requests */
			FOR_PTE(mark); /* To make sure it is marked next time */
			FOR_PTE(writelock); /* To make sure it is 7 next time */
		}
		return;
	}

	/*
	 * If a process will write anything to a shareable page it will have
	 * to cross this point with a fault code of 7 (violate write access)
	 * and the accessed page would have already been associated.
	 */

	if ( srvcom_request_write(srvctx, pf_vaddr, pid, pgd) < 0 ) {
		printk(KERN_INFO "WARNING: Write request failed");
		return;
	}

	return;

}



static void __handle_usermode_read(pgd_t *pgd, unsigned long pf_vaddr,
	struct pt_regs* regs, unsigned long error_code) {

	if ( error_code & ACCESS_VIOLATE )
		__handle_usermode_read_violation(pgd, pf_vaddr, regs, error_code);
	else
		__handle_usermode_read_missingpage(pgd, pf_vaddr, regs, error_code);

	return;

}

static inline void __handle_usermode_read_violation(pgd_t *pgd,
	unsigned long pf_vaddr, struct pt_regs* regs, unsigned long error_code) {

	pfn_t pfn = {.val = pf_vaddr>>PAGE_SHIFT};
	struct readlock *readlocked =
		readlock_list_find(pending_readlocks, pgd, pfn);

	/*
	 * Remember that it is okay for for_pte_pgd
	 * to fail since that would cause the next
	 * read to trigger another page fault.
	 */

	if ( !readlocked ) {
		for_pte_pgd(pgd, pf_vaddr, __hga_readunlock);
		return;
	}

	if ( !readlocked->resolved_page )
		return;

	if ( for_pte_pgd(pgd, pf_vaddr, __hga_readunlock) < 0 )
		return;
	if ( set_page_data(pgd, pf_vaddr, readlocked->resolved_page) < 0 )
		/* Shouldn't happen */
		for_pte_pgd(pgd, pf_vaddr, __hga_readlock);
	else
		readlock_list_remove(pending_readlocks, pgd, pfn);

	return;

}

static inline void __handle_usermode_read_missingpage(pgd_t *pgd,
	unsigned long pf_vaddr, struct pt_regs* regs, unsigned long error_code) {

	do_page_fault_t pfault =
		(do_page_fault_t)addr_dft_do_page_fault;
	pfn_t pfn = {.val = pf_vaddr>>PAGE_SHIFT};
	struct readlock *readlocked =
		readlock_list_find(pending_readlocks, pgd, pfn);

	/*
	 * Remember that it is okay for for_pte_pgd
	 * to fail since that would cause the next
	 * read to trigger another page fault.
	 */

	if ( !readlocked ) {
		pfault(regs, error_code);
		return;
	}

	if ( !readlocked->resolved_page )
		return;

	pfault(regs, error_code);
	if ( set_page_data(pgd, pf_vaddr, readlocked->resolved_page) < 0 )
		/* Shouldn't happen */
		for_pte_pgd(pgd, pf_vaddr, __hga_readlock);
	else
		readlock_list_remove(pending_readlocks, pgd, pfn);

	return;

}

#undef IS_USERMODE_READ_MISSINGPAGE
#undef IS_USERMODE_WRITE_VIOLATION
#undef IS_USERMODE_READ_VIOLATION

#undef ERRCODE_MASK
#undef USERMODE
#undef WRITE_ATTEMPT
#undef ACCESS_VIOLATE

#undef FOR_PTE



static int my_fault_init(void) {

	addr_dft_page_fault =
		find_sym_address("page_fault");		//address of default 'page_fault'
	addr_dft_do_page_fault =
		find_sym_address("do_page_fault");	//address of default 'do_page_fault'
	addr_pv_irq_ops =
		find_sym_address("pv_irq_ops");		//address of 'pv_irq_ops'
	addr_error_entry =
		find_sym_address("error_entry");
	addr_error_exit =
		find_sym_address("error_exit");

	// check all the module_parameters are set properly
	if ( check_parameters() )
		return -1;

	// get the address of 'adjust_exception_frame' from pv_irq_ops struct
	addr_adjust_exception_frame = *(unsigned long *)(addr_pv_irq_ops + 0x30);

	if ( __init_srvcom() < 0 )
		return -1;
	if ( __init_readlocks() < 0 )
		return -1;

	return 0;

}

static int __init_srvcom(void) {

	if ( !(srvctx = srvcom_ctx_new()) ) {
		printk(KERN_INFO "__init_srvcom: Failed allocation");
		return -1;
	}

	srvcom_set_serv_addr(srvctx, SERVER_IP, SERVER_PORT);

	/* srvcom context, opcode, callback, callback data */
	srvcom_register_handler(srvctx, OPCODE_ALLOW_WRITE, handle_ev_allow_write, NULL);
	srvcom_register_handler(srvctx, OPCODE_LOCK_READ, handle_ev_lock_read, pending_readlocks);
	srvcom_register_handler(srvctx, OPCODE_RESUME_READ, handle_ev_resume_read, pending_readlocks);
	srvcom_register_handler(srvctx, OPCODE_PING_ALIVE, handle_ev_ping_alive, NULL);

	if ( srvcom_run(srvctx) < 0 ) {
		printk(KERN_INFO "__init_srvcom: Failed to start srvcom");
		return -1;
	}

	return 0;

}

static int __init_readlocks(void) {

	if ( !(pending_readlocks = readlock_list_new()) ) {
		printk(KERN_INFO "__init_readlocks: Failed allocation");
		return -1;
	}

	return 0;

}



static void my_fault_exit(void) {

	__exit_srvcom();
	__exit_readlocks();

	return;

}

static void __exit_srvcom(void) {

	srvcom_exit(srvctx);

	return;

}

static void __exit_readlocks(void) {

	readlock_list_free(pending_readlocks);

	return;

}



#undef SERVER_PORT
#undef SERVER_IP
#undef TARGET_PROC



////////////////////////////////////////////
// NO HGA CODE AFTER THIS COMMENT BARRIER //
// NO HGA CODE AFTER THIS COMMENT BARRIER //
// NO HGA CODE AFTER THIS COMMENT BARRIER //
////////////////////////////////////////////



asmlinkage void my_page_fault(void);
asm("   .text");
asm("   .type my_page_fault,@function");
asm("my_page_fault:");
asm("   .byte 0x66");
asm("   xchg %ax, %ax");
asm("   callq *addr_adjust_exception_frame");
asm("   sub $0x78, %rsp");
asm("   callq *addr_error_entry");
asm("   mov %rsp, %rdi");
asm("   mov 0x78(%rsp), %rsi");
asm("   movq $0xffffffffffffffff, 0x78(%rsp)");
asm("   callq my_do_page_fault");
asm("   jmpq *addr_error_exit");
asm("   nopl (%rax)");

//this function is copied from kernel source
static inline void pack_gate(gate_desc *gate, unsigned type, unsigned long func,
                         unsigned dpl, unsigned ist, unsigned seg){
    gate->offset_low    = PTR_LOW(func);
    gate->segment       = __KERNEL_CS;
    gate->ist       = ist;
    gate->p         = 1;
    gate->dpl       = dpl;
    gate->zero0     = 0;
    gate->zero1     = 0;
    gate->type      = type;
    gate->offset_middle = PTR_MIDDLE(func);
    gate->offset_high   = PTR_HIGH(func);
}

static void my_load_idt(void *info){
    struct desc_ptr *idtr_ptr = (struct desc_ptr *)info;
    load_idt(idtr_ptr);
}

int register_my_page_fault_handler(void){
    struct desc_ptr idtr;
    gate_desc *old_idt, *new_idt;
    int retval;

    //first, do some initialization work.
    retval = my_fault_init();
    if(retval)
        return retval;

    //record the default idtr
    store_idt(&default_idtr);

    //read the content of idtr register and get the address of old IDT table
    old_idt = (gate_desc *)default_idtr.address; //'default_idtr' is initialized in 'my_virt_drv_init'

    //allocate a page to store the new IDT table
    printk(KERN_INFO "my_virt_drv: alloc a page to store new idt table.\n");
    new_idt_table_page = __get_free_page(GFP_KERNEL);
    if(!new_idt_table_page)
        return -ENOMEM;

    idtr.address = new_idt_table_page;
    idtr.size = default_idtr.size;

    //copy the old idt table to the new one
    new_idt = (gate_desc *)idtr.address;
    memcpy(new_idt, old_idt, idtr.size);
    pack_gate(&new_idt[PGFAULT_NR], GATE_INTERRUPT, (unsigned long)my_page_fault, 0, 0, __KERNEL_CS);

    //load idt for all the processors
    printk(KERN_INFO "my_virt_drv: load the new idt table.\n");
    load_idt(&idtr);
    printk(KERN_INFO "my_virt_drv: new idt table loaded.\n");
    smp_call_function(my_load_idt, (void *)&idtr, 1); //wait till all are finished
    printk(KERN_INFO "my_virt_drv: all CPUs have loaded the new idt table.\n");

    return 0;

}

void unregister_my_page_fault_handler(void){

	struct desc_ptr idtr;

	store_idt(&idtr);

	// if the current idt is not the default one, restore the default one
	if( idtr.address != default_idtr.address || idtr.size != default_idtr.size ) {

		load_idt(&default_idtr);
		smp_call_function(my_load_idt, (void *)&default_idtr, 1);
		free_page(new_idt_table_page);

	}

	my_fault_exit();

	return;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* PAGE_FAULT_C */



