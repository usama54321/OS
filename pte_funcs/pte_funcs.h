


#ifndef PTE_FUNCS_H
#define PTE_FUNCS_H



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



typedef int (*pte_handler_t)(pte_t*);



// PTE handlers - later to be updated
int __hga_mark(pte_t *pte_entry);
int __hga_writelock(pte_t *pte_entry);
int __hga_writeunlock(pte_t *pte_entry);
int __hga_reset(pte_t *pte_entry);
int __hga_marked(pte_t *pte_entry);
int __hga_writelocked(pte_t *pte_entry);
int __hga_written(pte_t *pte_entry);
int __hga_shareable(pte_t *pte_entry);
int __hga_printflags(pte_t *pte_entry);

int for_pte_pgd(pgd_t *pgd, unsigned long addr, pte_handler_t pte_handler);
int for_pte(struct mm_struct *mm, unsigned long addr, pte_handler_t pte_handler);



MODULE_LICENSE("Dual BSD/GPL");



#undef PTE_FUNCS_H



