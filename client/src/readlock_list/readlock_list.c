


/*!
 * @file readlock_list.c
 *
 * @brief Singly linked list data structure for readlock information.
 *
 * Simple linked list implementation with stack/queue insertion/removal
 * functions for recording readlock information.
 *
 * This component was created to deal with the following series of events:
 *    1) Page P of a process X is not currently present on machine A.
 *    2) X running on machine B requests the server to write to P.
 *    3) The server sends a read lock command to X running on A.
 *    4) A fails to lock the page as it is currently not present.
 *    5) A replies with "success" hoping X won't read P during the write.
 *    6) The page fault for P is triggered on A before the write.
 *    7) P is resolved on A without being readlocked.
 *    8) X running on A reads a wrong version of P.
 *
 * We maintain a list of readlock commands and add a pending readlock
 * to the list at step 5. If a read on an absent page triggers a page
 * fault then the page fault handler will only resolve the page as in
 * step 7 if a readlock was not pending on the page. When B completes
 * the write operation, the entry for P will be resolved with the page
 * data and removed later by the read unlock handler.
 */



#ifndef READLOCK_LIST_C
#define READLOCK_LIST_C



#define __HGA_KERNEL



#ifdef __HGA_KERNEL

#include <linux/slab.h>
#include <linux/pfn_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <asm/pgtable_types.h>

#define __RL_ALLOC(n) kmalloc(n, GFP_KERNEL)
#define __RL_FREE(ptr) kfree(ptr)
#define __RL_PRINT(str, ...) printk(KERN_INFO str, ##__VA_ARGS__)
#define __RL_WARN(str, ...) printk(KERN_ERR "WARNING: " str, ##__VA_ARGS__)
#define __RL_ERROR(str, ...) printk(KERN_ERR "ERROR: " str, ##__VA_ARGS__)

#else /* __HGA_KERNEL */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define __RL_ALLOC(n) malloc(n)
#define __RL_FREE(ptr) free(ptr)
#define __RL_PRINT(str, ...) printf(str "\n", ##__VA_ARGS__)
#define __RL_WARN(str, ...) printf("WARNING: " str "\n", ##__VA_ARGS__)
#define __RL_ERROR(str, ...) printf("ERROR: " str "\n", ##__VA_ARGS__)

#endif /* __HGA_KERNEL */



#include "../readlock_list/readlock_list.h"



/////////////////////////////////////////////
////////////////// HELPERS //////////////////
/////////////////////////////////////////////

/*!
 * @brief Allocate memory a readlock list node and initialize
 * with defaults.
 *
 * @return Pointer to created readlock node, or NULL on failure.
 */
static struct readlock *__readlock_new(void) {

	struct readlock *readlock =
		(struct readlock*)__RL_ALLOC(sizeof(struct readlock));

	if ( !readlock ) {
		__RL_ERROR("__readlock_new: Memory allocation failure.");
		return NULL;
	}

	memset(readlock, 0, sizeof(struct readlock));

	return readlock;

}

/*!
 * @brief Free memory allocated for a readlock_list node.
 *
 * Meant to be passed to __readlock_list_foreach as a callback
 * function in readlock_list_free and readlock_list_empty.
 *
 * @see __readlock_list_foreach
 * @see readlock_list_free
 * @see readlock_list_empty
 *
 * Pointer members are assumed to point to allocated regions if
 * not NULL, so avoid making modifications to these fields that
 * can cause problems for deallocation.
 *
 * @param readlock Pointer to the readlock node in the list.
 *
 * @return 0 on success, or -1 on failure.
 */
static int __free_readlock(struct readlock *readlock) {

	if ( readlock->resolved_page )
		__RL_FREE(readlock->resolved_page);

	__RL_FREE(readlock);

	return 0;

}

/*!
 * @brief Print a readlock node in the list.
 *
 * Meant to be passed to __readlock_list_foreach as a callback
 * function in readlock_list_print.
 *
 * @see __readlock_list_foreach
 * @see readlock_list_print
 *
 * @param readlock Pointer to the readlock node in the list.
 *
 * @return Just 0
 */
static int __print_readlock(struct readlock *readlock) {

	__RL_PRINT("readlock data:");

	// Print readlock members here
	__RL_PRINT("    PGD location: %p", readlock->pgd);
	__RL_PRINT("    Page frame number: %llu", readlock->pfn.val);

	return 0;

}

/*!
 * @brief Equate with a readlock node in the list.
 *
 * Meant to be passed to the iterator functions as a
 * callback for finding nodes with a specific pgd/pfn
 *
 * @see __readlock_list_find
 * @see __readlock_list_delete
 *
 * @param readlock Pointer to the readlock node in the list.
 *
 * @return Just 0
 */
static int __match_readlock(struct readlock *readlock, void *cb_data) {

	struct readlock *compare =
		(struct readlock*)cb_data;

	if ( readlock->pgd != compare->pgd )
		return 0;
	if ( readlock->pfn.val != compare->pfn.val )
		return 0;

	return 1;

}

/*!
 * @brief Insert a readlock struct pointer at the tail
 * of the list.
 *
 * @param list Pointer to a readlock_list struct
 * @param readlock readlock struct pointer to insert
 */
static void __readlock_list_push_back(struct readlock_list *list, struct readlock *readlock) {

	if (list->n_readlocks == 0) {

		readlock->next = NULL;
		list->head = list->tail = readlock;

	} else {

		readlock->next = NULL;
		list->tail->next = readlock;
		list->tail = readlock;

	}

	list->n_readlocks += 1;

	return;

}

/*!
 * @brief Selects the callback to call and calls it
 *
 * readlock_cb is taken as a void pointer to a function and
 * the actual function type is decided at runtime as either a
 * function taking a readlock struct pointer or a function
 * taking both a readlock struct pointer and callback data.
 * If cb_data is NULL then readlock_cb is assumed to be of
 * the former type, otherwise it is assumed to be of the latter
 * type and cb_data will be passed to the function as a second
 * argument.
 */
#define CALL_CB (						\
	cb_data ?						\
	((readlock_callback)readlock_cb)(curr, cb_data):	\
	((readlock_callback_nocbdata)readlock_cb)(curr)		\
)

/*!
 * @brief Invoke a callback function on all readlock pointers
 * in the list.
 *
 * The callback function must take a pointer to a readlock
 * struct, optionally a pointer to callback data, and return an
 * integer result. The return value of this function will be the
 * sum of results returned by each invocation of the callback
 * function. This is intended to be useful where the callback
 * would be an indicator function, and we want the number of
 * successes after iterating over all list nodes.
 *
 * @param list Pointer to a readlock_list struct
 * @param readlock_cb Function pointer to callback function
 * @param cb_data Pointer to callback data
 *
 * @see readlock_callback
 * @see readlock_callback_nocbdata
 *
 * @return Integer sum of invocation results
 */
static int __readlock_list_foreach(struct readlock_list *list,
	void *readlock_cb, void *cb_data) {

	int sum = 0;

	struct readlock *curr = list->head, *next;

	while ( curr ) {
		next = curr->next;
		sum += CALL_CB;
		curr = next;
	}

	return sum;

}

/*!
 * @brief Invoke a callback function on all readlock pointers
 * in the list and return the first one which gives a non-zero
 * return value.
 *
 * @param list Pointer to a readlock_list struct
 * @param readlock_cb Function pointer to callback function
 * @param cb_data Pointer to callback data
 *
 * @see readlock_callback
 * @see readlock_callback_nocbdata
 *
 * @return First readlock node pointer on which the callback
 * returns a non-zero result.
 */
static struct readlock *__readlock_list_find(struct readlock_list *list,
	void *readlock_cb, void *cb_data) {

	struct readlock *curr = list->head, *next;

	while ( curr ) {
		next = curr->next;
		if ( CALL_CB )
			return curr;
		curr = next;
	}

	return NULL;

}

/*!
 * @brief Invoke a callback function on all readlock pointers
 * in the list and delete the first one which gives a non-zero
 * return value.
 *
 * @param list Pointer to a readlock_list struct
 * @param readlock_cb Function pointer to callback function
 * @param cb_data Pointer to callback data
 *
 * @see readlock_callback
 * @see readlock_callback_nocbdata
 *
 * @return 1 if a node was deleted, 0 otherwise
 */
static int __readlock_list_delete(struct readlock_list *list,
	void *readlock_cb, void *cb_data) {

	struct readlock
		*prev = NULL,
		*curr = list->head,
		*next = NULL
	;

	while ( curr ) {
		next = curr->next;
		if ( CALL_CB )
			break;
		prev = curr;
		curr = next;
	}

	if ( !curr )
		return 0;

	if ( prev )
		prev->next = next;
	else
		list->head = next;

	if ( !next )
		list->tail = prev;

	__free_readlock(curr);

	list->n_readlocks -= 1;

	return 1;

}

#undef CALL_CB

////////////////////////////////////////////////
////////////////// INTERFACES //////////////////
////////////////////////////////////////////////

/*!
 * @brief Allocate memory for a readlock_list and initialize
 * with defaults.
 *
 * @return Pointer to the created list, or NULL on failure
 */
struct readlock_list *readlock_list_new(void) {

	struct readlock_list *list =
		(struct readlock_list*)__RL_ALLOC(sizeof(struct readlock_list));

	if ( list == NULL ) {
		__RL_ERROR("readlock_list_new: Allocation failure.");
		return NULL;
	}

	list->n_readlocks = 0;

	list->head = NULL;
	list->tail = NULL;

	spin_lock_init(&list->lock);
	spin_unlock(&list->lock);

	return list;

}

/*!
 * @brief Destroy all nodes along with the list.
 *
 * @param list Pointer to a readlock_list struct
 */
void readlock_list_free(struct readlock_list *list) {

	spin_lock(&list->lock);

	__readlock_list_foreach(list, __free_readlock, NULL);

	__RL_FREE(list);

	spin_unlock(&list->lock);

	return;

}

/*!
 * @brief Print all nodes in the list.
 *
 * Printing will be done as specified in __print_readlock.
 *
 * @see __print_readlock
 *
 * @param list Pointer to a readlock_list struct
 */
void readlock_list_print(struct readlock_list *list) {

	spin_lock(&list->lock);

	__readlock_list_foreach(list, __print_readlock, NULL);

	spin_unlock(&list->lock);

	return;

}

/*!
 * @brief Add a readlock to a readlock list with replacement
 *
 * If a readlock with the provided pgd/pfn is found, it is
 * simply marked unresolved, otherwise a new unresolved node
 * is added to the list (at the tail).
 *
 * @param list Pointer to a readlock_list struct
 * @param pgd Pointer to the PGD
 * @param pfn Page frame number
 *
 * @return 0 if the readlock was added or replaced, or -1 on
 * failure
 */
int readlock_list_add_pending(struct readlock_list *list, pgd_t *pgd,
	pfn_t pfn) {

	struct readlock *readlock;

	spin_lock(&list->lock);

	/* Find a readlock containing pgd and pfn */
	readlock = __readlock_list_find(list, __match_readlock,
		&(struct readlock){.pgd = pgd, .pfn = pfn});

	if ( readlock ) {
		/* The readlock is present; reset it */
		if ( readlock->resolved_page ) {
			__RL_FREE(readlock->resolved_page);
			readlock->resolved_page = NULL;
		}
		spin_unlock(&list->lock);
		return 0;
	}

	/* Not present; add a new readlock */
	if ( !(readlock = __readlock_new()) ) {
		spin_unlock(&list->lock);
		return -1;
	}
	readlock->pgd = pgd;
	readlock->pfn = pfn;
	__readlock_list_push_back(list, readlock);

	spin_unlock(&list->lock);

	return 0;

}

/*!
 * @brief Mark a readlock resolved with page data
 *
 * The resolved_page pointer member is used to
 * indicate whether or not the page is resolved,
 * so we just allocate page data for this field
 * and copy all the page content.
 *
 * @param list Pointer to a readlock_list struct
 * @param pgd Pointer to the PGD
 * @param pfn Page frame number
 * @param page Pointer to page data
 *
 * @return 0 on success, or -1 on failure or if no
 * readlock matched the given pgd and pfn
 */
int readlock_list_resolve(struct readlock_list *list, pgd_t *pgd,
	pfn_t pfn, char *page) {

	struct readlock *readlock;

	spin_lock(&list->lock);

	/* Find a readlock containing pgd and pfn */
	readlock = __readlock_list_find(list, __match_readlock,
		&(struct readlock){.pgd = pgd, .pfn = pfn});

	if ( !readlock ) {
		/* Shouldn't happen */
		__RL_WARN("Unable to resolve missing readlock");
		spin_unlock(&list->lock);
		return -1;
	}

	if ( !(readlock->resolved_page = __RL_ALLOC(PAGE_SIZE)) ) {
		spin_unlock(&list->lock);
		return -1;
	}
	memcpy(readlock->resolved_page, page, PAGE_SIZE);

	spin_unlock(&list->lock);

	return 0;

}

/*!
 * @brief Find a readlock node with a specific pgd and pfn
 *
 * @param list Pointer to a readlock_list struct
 * @param pgd Pointer to the PGD
 * @param pfn Page frame number
 *
 * @return Pointer to the first readlock node matching the
 * given pgd and pfn, or NULL on no match
 */
struct readlock *readlock_list_find(struct readlock_list *list, pgd_t *pgd,
	pfn_t pfn) {

	struct readlock *ret;

	spin_lock(&list->lock);

	ret = __readlock_list_find(list, __match_readlock,
		&(struct readlock){.pgd = pgd, .pfn = pfn});

	spin_unlock(&list->lock);

	return ret;

}

/*!
 * @brief Remove a readlock from the list
 *
 * Removes only the first readlock node matching
 * the given pgd and pfn.
 *
 * @param list Pointer to a readlock_list struct
 * @param pgd Pointer to the PGD
 * @param pfn Page frame number
 *
 * @return 0 on success, or -1 on failure or if no
 * readlock matched the given pgd and pfn
 */
int readlock_list_remove(struct readlock_list *list, pgd_t *pgd,
	pfn_t pfn) {

	int ret;

	spin_lock(&list->lock);

	ret = __readlock_list_delete(list, __match_readlock,
		&(struct readlock){.pgd = pgd, .pfn = pfn});

	spin_unlock(&list->lock);

	return ret;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* READLOCK_LIST_C */



