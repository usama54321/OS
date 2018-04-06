


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
 * XXX:
 *    - For now we're assuming a unique PID
 *      for every process name and that the
 *      same PID will not be used for future
 *      processes.
 */

/*
 * TODO:
 *    - Implement pending readlocks
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



void my_do_page_fault(struct pt_regs* regs, unsigned long error_code) {

#define FOR_PTE(func) (for_pte(task->mm, pf_vaddr, __hga_##func))

#define ACCESS_VIOLATE	(1 << 0)
#define WRITE_ATTEMPT	(1 << 1)
#define USERMODE	(1 << 2)
#define ERRCODE_MASK	( ACCESS_VIOLATE | WRITE_ATTEMPT | USERMODE )



	pid_t pid; pgd_t *pgd;
	int marked, shareable;
	const struct task_struct *task = current;
	const unsigned long pf_vaddr = read_cr2();
	const do_page_fault_t pfault =
		(do_page_fault_t)addr_dft_do_page_fault;



	if ( task_targeted(task) != 1 ) {
		/* Error or not a target process */
		pfault(regs, error_code);
		return;
	}

	pid = task->pid;
	pgd = task->mm->pgd;

	/* -1 on error, 0 if the page is not to be shared and 1 otherwise */
	shareable = FOR_PTE(shareable);

	if ( shareable == 0 ) {
		pfault(regs, error_code);
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
	if ( shareable < 0 ) {
		pfault(regs, error_code);
		/* Second attempt */
		if ( (shareable = FOR_PTE(shareable)) <= 0 )
			return; /* Either still not available or available and not shareable */
	}

	/* At this point the page fault is from our target process and involves a shareable page */

	marked = FOR_PTE(marked);

	/*
	 * If the fault was not generated by a userspace
	 * write-access permission violation or originated
	 * from an unassociated page then just prepare the
	 * page for interception and leave without taking
	 * further action.
	 */
	if ( !marked || (error_code&ERRCODE_MASK) != (ACCESS_VIOLATE|WRITE_ATTEMPT|USERMODE) ) {
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



#undef ERRCODE_MASK
#undef USERMODE
#undef WRITE_ATTEMPT
#undef ACCESS_VIOLATE

#undef FOR_PTE

}



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

	srvcom_register_handler(srvctx, OPCODE_ALLOW_WRITE, handle_ev_allow_write, NULL);
	srvcom_register_handler(srvctx, OPCODE_LOCK_READ, handle_ev_lock_read, pending_readlocks);
	srvcom_register_handler(srvctx, OPCODE_RESUME_READ, handle_ev_resume_read, pending_readlocks);

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



