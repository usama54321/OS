


#ifndef PTE_FUNCS_C
#define PTE_FUNCS_C



#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/traps.h>
#include <asm/pgalloc.h>
#include <asm/pgtable_types.h>
#include <asm/desc_defs.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>

#include "../pte_funcs/pte_funcs.h"



#define PT_EDIT_MODE(statements) {	\
	unsigned long __cr0;		\
	preempt_disable();		\
	barrier();			\
	__cr0 = read_cr0();		\
	write_cr0(__cr0 & ~X86_CR0_WP);	\
	{statements;}			\
	write_cr0(__cr0);		\
	barrier();			\
	preempt_enable();		\
	__flush_tlb();			\
}

/* Setters/Clearers */
#define __HGA_MARK(pte_entry) { /* Associate a page with this module */	\
	PT_EDIT_MODE(							\
		*(pte_entry) =						\
			pte_set_flags(*(pte_entry), _PAGE_PCD);		\
	)								\
}
#define __HGA_WRITELOCK(pte_entry) { /* Only write with permission */	\
	PT_EDIT_MODE(							\
		*(pte_entry) =						\
			pte_clear_flags(*(pte_entry), _PAGE_RW);	\
	)								\
}
#define __HGA_WRITEUNLOCK(pte_entry) { /* Free to write */		\
	PT_EDIT_MODE(							\
		*(pte_entry) =						\
			pte_set_flags(*(pte_entry), _PAGE_RW);		\
	)								\
}
#define __HGA_READLOCK(pte_entry) { /* Only read with permission */	\
	PT_EDIT_MODE(							\
		*(pte_entry) =						\
			pte_clear_flags(*(pte_entry), _PAGE_USER);	\
	)								\
}
#define __HGA_READUNLOCK(pte_entry) { /* Free to read */		\
	PT_EDIT_MODE(							\
		*(pte_entry) =						\
			pte_set_flags(*(pte_entry), _PAGE_USER);	\
	)								\
}

/* Testers */
#define __HGA_MARKED(pte_entry) (pte_flags(*(pte_entry)) & _PAGE_PCD)
#define __HGA_WRITEUNLOCKED(pte_entry) (pte_flags(*(pte_entry)) & _PAGE_RW)
#define __HGA_READUNLOCKED(pte_entry) (pte_flags(*(pte_entry)) & _PAGE_USER)
#define __HGA_SHAREABLE(pte_entry) (pte_flags(*(pte_entry)) & _PAGE_NX)



// PTE handlers - later to be updated
int __hga_mark(pte_t *pte_entry) {__HGA_MARK(pte_entry); return 0;}
int __hga_writelock(pte_t *pte_entry) {__HGA_WRITELOCK(pte_entry); return 0;}
int __hga_writeunlock(pte_t *pte_entry) {__HGA_WRITEUNLOCK(pte_entry); return 0;}
int __hga_readlock(pte_t *pte_entry) {__HGA_READLOCK(pte_entry); return 0;}
int __hga_readunlock(pte_t *pte_entry) {__HGA_READUNLOCK(pte_entry); return 0;}
int __hga_marked(pte_t *pte_entry) {return __HGA_MARKED(pte_entry)?1:0;}
int __hga_writelocked(pte_t *pte_entry) {return __HGA_WRITEUNLOCKED(pte_entry)?0:1;}
int __hga_readlocked(pte_t *pte_entry) {return __HGA_READUNLOCKED(pte_entry)?0:1;}
int __hga_shareable(pte_t *pte_entry) {return __HGA_SHAREABLE(pte_entry)?1:0;}
int __hga_printflags(pte_t *pte_entry) {

#define FOR_BIT(bitid) printk(KERN_INFO "    Bit _PAGE_" #bitid " is %s...", (flags & ( _PAGE_ ## bitid )) ? "set" : "clear");

	pteval_t flags = pte_flags(*pte_entry);

	printk("Printing page table entry flags:");

	FOR_BIT(PRESENT);
	FOR_BIT(NX);
	FOR_BIT(RW);
	FOR_BIT(USER);
	FOR_BIT(PROTNONE);
	FOR_BIT(ACCESSED);
	FOR_BIT(DIRTY);
	FOR_BIT(PWT);
	FOR_BIT(PCD);
/*
	FOR_BIT(PSE);
	FOR_BIT(GLOBAL);
	FOR_BIT(SOFTW1);
	FOR_BIT(SOFTW2);
	FOR_BIT(PAT);
	FOR_BIT(PAT_LARGE);
	FOR_BIT(SPECIAL);
	FOR_BIT(CPA_TEST);
	FOR_BIT(PKEY_BIT0);
	FOR_BIT(PKEY_BIT1);
	FOR_BIT(PKEY_BIT2);
	FOR_BIT(PKEY_BIT3);
	FOR_BIT(SOFT_DIRTY);
	FOR_BIT(DEVMAP);
*/
	return 0;

#undef FOR_BIT

}

int for_pte_pgd(pgd_t *pgd, unsigned long addr, pte_handler_t pte_handler) {

	int retval;

	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pte_entry;

	pgd_entry = pgd + pgd_index(addr);
	if ( pgd_none(*pgd_entry) || pgd_bad(*pgd_entry) )
		return -1;

	pud_entry = pud_offset(pgd_entry, addr);
	if ( pud_none(*pud_entry) || pud_bad(*pud_entry) )
		return -1;

	pmd_entry = pmd_offset(pud_entry, addr);
	if ( pmd_none(*pmd_entry) || pmd_bad(*pmd_entry) )
		return -1;

	pte_entry = pte_offset_map(pmd_entry, addr);
	if ( !pte_entry )
		return -1;

	retval = pte_handler(pte_entry);

	pte_unmap(pte_entry);

	return retval;

}

int for_pte(struct mm_struct *mm, unsigned long addr, pte_handler_t pte_handler) {

#define _DBG_ printk(KERN_INFO "for_pte[%p]::At %d...", (void*)addr, __LINE__);

	int retval;

	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pte_entry;

	if ( !mm )
		return -1;

	pgd_entry = pgd_offset(mm, addr);
	if ( pgd_none(*pgd_entry) || pgd_bad(*pgd_entry) )
		return -1;

	pud_entry = pud_offset(pgd_entry, addr);
	if ( pud_none(*pud_entry) || pud_bad(*pud_entry) )
		return -1;

	pmd_entry = pmd_offset(pud_entry, addr);
	if ( pmd_none(*pmd_entry) || pmd_bad(*pmd_entry) )
		return -1;

	pte_entry = pte_offset_map(pmd_entry, addr);
	if ( !pte_entry )
		return -1;

	retval = pte_handler(pte_entry);

	pte_unmap(pte_entry);

	return retval;

#undef _DBG_

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* PTE_FUNCS_C */



