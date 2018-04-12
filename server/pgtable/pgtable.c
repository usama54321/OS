#include "pgtable.h"

int get_page_data(pgd_t *pgd, unsigned long addr, char *page_buf) {
	
	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pte_entry;
	struct page *page;

	pgd_entry = pgd + pgd_index(addr);
	if ( pgd_none(*pgd_entry) || pgd_bad(*pgd_entry) )
		return -1;

	pud_entry = pud_offset(pgd_entry, addr);
	if ( pud_none(*pud_entry) || pud_bad(*pud_entry) )
		return -1;

	pmd_entry = pmd_offset(pud_entry, addr);
	if ( pmd_none(*pmd_entry) || pmd_bad(*pmd_entry) )
		return -1;

	pte_entry = pte_offset_kernel(pmd_entry, addr);
	if ( !pte_entry )
		return -1;

	page = pte_page(*pte_entry);
    //address of struct page?
	memcpy(page_buf, page_address(page), PAGE_SIZE);

	pte_unmap(pte_entry);

	return 0;

}

int set_page_data(pgd_t *pgd, unsigned long addr, char *page_buf) {

	pgd_t *pgd_entry;
	pud_t *pud_entry;
	pmd_t *pmd_entry;
	pte_t *pte_entry;
	struct page *page;

	pgd_entry = pgd + pgd_index(addr);
	if ( pgd_none(*pgd_entry) || pgd_bad(*pgd_entry) )
		return -1;

	pud_entry = pud_offset(pgd_entry, addr);
	if ( pud_none(*pud_entry) || pud_bad(*pud_entry) )
		return -1;

	pmd_entry = pmd_offset(pud_entry, addr);
	if ( pmd_none(*pmd_entry) || pmd_bad(*pmd_entry) )
		return -1;

	pte_entry = pte_offset_kernel(pmd_entry, addr);
	if ( !pte_entry )
		return -1;

	page = pte_page(*pte_entry);
	memcpy(page_address(page), page_buf, PAGE_SIZE);

	pte_unmap(pte_entry);

	return 0;

}
