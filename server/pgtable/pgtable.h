#ifndef HGA_PGTABLE
#define HGA_PGTABLE

#include <asm/pgtable_types.h>
#include <linux/mm.h>

int get_page_data(pgd_t *pgd, unsigned long addr, char *page_buf);
int set_page_data(pgd_t *pgd, unsigned long addr, char *page_buf);

#endif
