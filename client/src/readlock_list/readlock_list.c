


/*!
 *
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
 *
 */



#ifndef READLOCK_LIST_C
#define READLOCK_LIST_C



#include <linux/pfn_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/pgtable_types.h>

#include "../readlock_list/readlock_list.h"



////////////////////////////////////////////////////////
///////////////////// USER-DEFINED /////////////////////
////////////////////////////////////////////////////////

/*!

	@brief Allocate memory an readlock list node and initialize
	with defaults.

	@return Pointer to created readlock node, or NULL on failure.

*/
struct readlock *readlock_new(void) {

	struct readlock *readlock =
		(struct readlock*)kmalloc(sizeof(struct readlock), GFP_KERNEL);

	if ( !readlock ) {
		printk(KERN_INFO "readlock_new: Memory allocation failure.\n");
		return NULL;
	}

	memset(readlock, 0, sizeof(struct readlock));

	return readlock;

}

/*!

	@brief Free memory allocated for an readlock_list node.

	Meant to be passed to readlock_list_foreach as a callback
	function in readlock_list_free and readlock_list_empty.

	@see readlock_list_foreach
	@see readlock_list_free
	@see readlock_list_empty

	Pointer members are assumed to point to allocated regions if
	not NULL, so avoid making modifications to these fields that
	can cause problems for deallocation.

	@param readlock Pointer to the readlock node in the list.

	@return 0 on success, or -1 on failure.

*/
static int free_readlock(struct readlock *readlock) {

	kfree(readlock);

	return 0;

}

/*!

	@brief Print an readlock node in the list.

	Meant to be passed to readlock_list_foreach as a callback
	function in readlock_list_print.

	@see readlock_list_foreach
	@see readlock_list_print

	@param readlock Pointer to the readlock node in the list.

	@return Just 0

*/
static int print_readlock(struct readlock *readlock) {

	printk(KERN_INFO "readlock data:\n");

	// Print readlock members here
	printk(KERN_INFO "    Resolved status: %d", readlock->resolved);
	printk(KERN_INFO "    PGD location: %p", readlock->pgd);
	printk(KERN_INFO "    Page frame number: %lu", readlock->pfn);

	return 0;

}



///////////////////////////////////////////////////////
////////////////////// AUTOMATIC //////////////////////
///////////////////////////////////////////////////////

/*!

	@brief Allocate memory for an readlock_list and initialize
	with defaults.

	@return Pointer to the created list, or NULL on failure

*/
struct readlock_list *readlock_list_new(void) {

	struct readlock_list *list =
		(struct readlock_list*)kmalloc(sizeof(struct readlock_list), GFP_KERNEL);

	if ( list == NULL ) {
		printk(KERN_INFO "readlock_list_new: Allocation failure.\n");
		return NULL;
	}

	list->n_readlocks = 0;

	list->head = NULL;
	list->tail = NULL;

	return list;

}

/*!

	@brief Insert an readlock struct pointer at the head
	of the list.

	@param list Pointer to an readlock_list struct
	@param readlock readlock struct pointer to insert

*/
void readlock_list_push_front(struct readlock_list *list, struct readlock *readlock) {

	if (list->n_readlocks == 0) {

		readlock->next = NULL;
		list->head = list->tail = readlock;

	} else {

		readlock->next = list->head;
		list->head = readlock;

	}

	list->n_readlocks += 1;

	return;

}

/*!

	@brief Insert an readlock struct pointer at the tail
	of the list.

	@param list Pointer to an readlock_list struct
	@param readlock readlock struct pointer to insert

*/
void readlock_list_push_back(struct readlock_list *list, struct readlock *readlock) {

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

	@brief Insert an readlock struct pointer at the head
	of the list.

	@param list Pointer to an readlock_list struct
	@param readlock readlock struct pointer to insert

*/
void readlock_list_push(struct readlock_list *list, struct readlock *readlock) {

	readlock_list_push_front(list, readlock);

	return;

}

/*!

	@brief Insert an readlock struct pointer at the tail
	of the list.

	@param list Pointer to an readlock_list struct
	@param readlock readlock struct pointer to insert

*/
void readlock_list_insert(struct readlock_list *list, struct readlock *readlock) {

	readlock_list_push_back(list, readlock);

	return;

}

/*!

	@brief Retrieve a pointer to the head of the list.

	@param list Pointer to an readlock_list struct

	@return Pointer to the head of the list

*/
struct readlock *readlock_list_front(struct readlock_list *list) {

	return list->head;

}

/*!

	@brief Retrieve a pointer to the tail of the list.

	@param list Pointer to an readlock_list struct

	@return Pointer to the tail of the list

*/
struct readlock *readlock_list_back(struct readlock_list *list) {

	return list->tail;

}

/*!

	@brief Retrieve a pointer to the head of the list.

	@param list Pointer to an readlock_list struct

	@return Pointer to the head of the list

*/
struct readlock *readlock_list_top(struct readlock_list *list) {

	return list->head;

}

/*!

	@brief Remove the head of the list.

	@param list Pointer to an readlock_list struct

	@return 0 on success, -1 if there was nothing to remove

*/
int readlock_list_pop_front(struct readlock_list *list) {

	struct readlock *next;

	if (list->n_readlocks == 0)
		return -1;

	if (list->n_readlocks == 1) {

		free_readlock(list->head);
		list->head = list->tail = NULL;
		list->n_readlocks = 0;

		return 0;

	}

	next = list->head->next;
	free_readlock(list->head);
	list->head = next;
	list->n_readlocks -= 1;

	return 0;

}

/*!

	@brief Remove the head of the list.

	@param list Pointer to an readlock_list struct

	@return 0 on success, -1 if there was nothing to remove

*/
int readlock_list_pop(struct readlock_list *list) {

	return readlock_list_pop_front(list);

}

/*! Selects the callback to call and calls it */
#define CALL_CB (						\
	cb_data ?						\
	((readlock_callback)readlock_cb)(curr, cb_data):	\
	((readlock_callback_nocbdata)readlock_cb)(curr)		\
)

/*!

	@brief Invoke a callback function on all readlock pointers
	in the list.

	The callback function must take a pointer to a readlock
	struct, optionally a pointer to callback data, and return an
	integer result. The return value of this function will be the
	sum of results returned by each invocation of the callback
	function. This is intended to be useful where the callback
	would be an indicator function, and we want the number of
	successes after iterating over all list nodes.

	readlock_cb is taken as a void pointer to a function and
	the actual function type is decided at runtime as either a
	function taking an readlock struct pointer or a function
	taking both an readlock struct pointer and callback data.
	If cb_data is NULL then readlock_cb is assumed to be of
	the former type, otherwise it is assumed to be of the latter
	type and cb_data will be passed to the function as a second
	argument.

	@param list Pointer to an readlock_list struct
	@param readlock_cb Function pointer to callback function
	@param cb_data Pointer to callback data

	@see readlock_callback
	@see readlock_callback_nocbdata

	@return Integer sum of invocation results

*/
int readlock_list_foreach(struct readlock_list *list,
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

/* Undocumented */
struct readlock *readlock_list_find(struct readlock_list *list,
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

/* Undocumented */
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

	free_readlock(curr);

	list->n_readlocks -= 1;

	return 1;

}

#undef CALL_CB

/* Undocumented */
void readlock_list_delete(struct readlock_list *list,
	void *readlock_cb, void *cb_data) {

	__readlock_list_delete(list, readlock_cb, cb_data);

	return;

}

/* Undocumented */
void readlock_list_delete_all(struct readlock_list *list,
	void *readlock_cb, void *cb_data) {

	while (__readlock_list_delete(list, readlock_cb, cb_data));

	return;

}

/*!

	@brief Destroy all nodes and empty the list.

	@param list Pointer to an readlock_list struct

*/
void readlock_list_empty(struct readlock_list *list) {

	readlock_list_foreach(list, free_readlock, NULL);

	list->n_readlocks = 0;

	list->head = NULL;
	list->tail = NULL;

	return;

}

/*!

	@brief Destroy all nodes along with the list.

	@param list Pointer to an readlock_list struct

*/
void readlock_list_free(struct readlock_list *list) {

	readlock_list_foreach(list, free_readlock, NULL);

	kfree(list);

	return;

}

/*!

	@brief Print all nodes in the list.

	Printing will be done as specified in print_readlock.

	@see print_readlock

	@param list Pointer to an readlock_list struct

*/
void readlock_list_print(struct readlock_list *list) {

	readlock_list_foreach(list, print_readlock, NULL);

	return;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* READLOCK_LIST_C */



