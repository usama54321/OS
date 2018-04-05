


/*!

	@file targetproc_list.c

	@brief Singly linked list data structure for targetproc information.

	Simple linked list implementation with stack/queue insertion/removal
	functions for recording targetproc information.

	Callback implementations are provided to facilitate unduplicated
	insertion. Unduplicated insertion will be in O(n) but is favored
	over memory inefficiency.

*/



#include <linux/kernel.h>
#include <linux/module.h>

#include "../targetproc_list/targetproc_list.h"



/// Maximum line size to assume when loading targetproc records from a file
#define MAX_LINESZ 1024



////////////////////////////////////////////////////////
///////////////////// USER-DEFINED /////////////////////
////////////////////////////////////////////////////////

/*!

	@brief Allocate memory an targetproc list node and initialize
	with defaults.

	@return Pointer to created targetproc node, or NULL on failure.

*/
struct targetproc *targetproc_new(void) {

	struct targetproc *targetproc =
		(struct targetproc*)kmalloc(sizeof(struct targetproc), GFP_KERNEL);

	if ( !targetproc ) {
		printk(KERN_INFO "targetproc_new: Memory allocation failure.");
		return NULL;
	}

	memset(targetproc, 0, sizeof(struct targetproc));

	// Initialization code here

	return targetproc;

}

/*!

	@brief Free memory allocated for an targetproc_list node.

	Meant to be passed to targetproc_list_foreach as a callback
	function in targetproc_list_free and targetproc_list_empty.

	@see targetproc_list_foreach
	@see targetproc_list_free
	@see targetproc_list_empty

	Pointer members are assumed to point to allocated regions if
	not NULL, so avoid making modifications to these fields that
	can cause problems for deallocation.

	@param targetproc Pointer to the targetproc node in the list.

	@return 0 on success, or -1 on failure.

*/
static int free_targetproc(struct targetproc *targetproc) {

	// Free necessary targetproc members

	kfree(targetproc);

	return 0;

}

/*!

	@brief Print an targetproc node in the list.

	Meant to be passed to targetproc_list_foreach as a callback
	function in targetproc_list_print.

	@see targetproc_list_foreach
	@see targetproc_list_print

	@param targetproc Pointer to the targetproc node in the list.

	@return Just 0

*/
static int print_targetproc(struct targetproc *targetproc) {

	printk(KERN_INFO "targetproc data:\n");

	// Print targetproc members here
	printk(KERN_INFO "    Process name %s", targetproc->procname);
	printk(KERN_INFO "    PID: %d", targetproc->pid);
	printk(KERN_INFO "    PGD table at %p", targetproc->pgd);

	return 0;

}



///////////////////////////////////////////////////////
////////////////////// AUTOMATIC //////////////////////
///////////////////////////////////////////////////////

/*!

	@brief Allocate memory for an targetproc_list and initialize
	with defaults.

	@return Pointer to the created list, or NULL on failure

*/
struct targetproc_list *targetproc_list_new(void) {

	struct targetproc_list *list =
		(struct targetproc_list*)kmalloc(sizeof(struct targetproc_list), GFP_KERNEL);

	if ( list == NULL ) {
		printk(KERN_INFO "targetproc_list_new: Allocation failure.");
		return NULL;
	}

	list->n_targetprocs = 0;

	list->head = NULL;
	list->tail = NULL;

	return list;

}

/*!

	@brief Insert an targetproc struct pointer at the head
	of the list.

	@param list Pointer to an targetproc_list struct
	@param targetproc targetproc struct pointer to insert

*/
void targetproc_list_push_front(struct targetproc_list *list, struct targetproc *targetproc) {

	if (list->n_targetprocs == 0) {

		targetproc->next = NULL;
		list->head = list->tail = targetproc;

	} else {

		targetproc->next = list->head;
		list->head = targetproc;

	}

	list->n_targetprocs += 1;

	return;

}

/*!

	@brief Insert an targetproc struct pointer at the tail
	of the list.

	@param list Pointer to an targetproc_list struct
	@param targetproc targetproc struct pointer to insert

*/
void targetproc_list_push_back(struct targetproc_list *list, struct targetproc *targetproc) {

	if (list->n_targetprocs == 0) {

		targetproc->next = NULL;
		list->head = list->tail = targetproc;

	} else {

		targetproc->next = NULL;
		list->tail->next = targetproc;
		list->tail = targetproc;

	}

	list->n_targetprocs += 1;

	return;

}

/*!

	@brief Insert an targetproc struct pointer at the head
	of the list.

	@param list Pointer to an targetproc_list struct
	@param targetproc targetproc struct pointer to insert

*/
void targetproc_list_push(struct targetproc_list *list, struct targetproc *targetproc) {

	targetproc_list_push_front(list, targetproc);

	return;

}

/*!

	@brief Insert an targetproc struct pointer at the tail
	of the list.

	@param list Pointer to an targetproc_list struct
	@param targetproc targetproc struct pointer to insert

*/
void targetproc_list_insert(struct targetproc_list *list, struct targetproc *targetproc) {

	targetproc_list_push_back(list, targetproc);

	return;

}

/*!

	@brief Retrieve a pointer to the head of the list.

	@param list Pointer to an targetproc_list struct

	@return Pointer to the head of the list

*/
struct targetproc *targetproc_list_front(struct targetproc_list *list) {

	return list->head;

}

/*!

	@brief Retrieve a pointer to the tail of the list.

	@param list Pointer to an targetproc_list struct

	@return Pointer to the tail of the list

*/
struct targetproc *targetproc_list_back(struct targetproc_list *list) {

	return list->tail;

}

/*!

	@brief Retrieve a pointer to the head of the list.

	@param list Pointer to an targetproc_list struct

	@return Pointer to the head of the list

*/
struct targetproc *targetproc_list_top(struct targetproc_list *list) {

	return list->head;

}

/*!

	@brief Remove the head of the list.

	@param list Pointer to an targetproc_list struct

	@return 0 on success, -1 if there was nothing to remove

*/
int targetproc_list_pop_front(struct targetproc_list *list) {

	struct targetproc *next;

	if (list->n_targetprocs == 0)
		return -1;

	if (list->n_targetprocs == 1) {

		free_targetproc(list->head);
		list->head = list->tail = NULL;
		list->n_targetprocs = 0;

		return 0;

	}

	next = list->head->next;
	free_targetproc(list->head);
	list->head = next;
	list->n_targetprocs -= 1;

	return 0;

}

/*!

	@brief Remove the head of the list.

	@param list Pointer to an targetproc_list struct

	@return 0 on success, -1 if there was nothing to remove

*/
int targetproc_list_pop(struct targetproc_list *list) {

	return targetproc_list_pop_front(list);

}

/*!

	@brief Invoke a callback function on all targetproc pointers
	in the list.

	The callback function must take a pointer to a targetproc
	struct, optionally a pointer to callback data, and return an
	integer result. The return value of this function will be the
	sum of results returned by each invocation of the callback
	function. This is intended to be useful where the callback
	would be an indicator function, and we want the number of
	successes after iterating over all list nodes.

	targetproc_cb is taken as a void pointer to a function and
	the actual function type is decided at runtime as either a
	function taking an targetproc struct pointer or a function
	taking both an targetproc struct pointer and callback data.
	If cb_data is NULL then targetproc_cb is assumed to be of
	the former type, otherwise it is assumed to be of the latter
	type and cb_data will be passed to the function as a second
	argument.

	@param list Pointer to an targetproc_list struct
	@param targetproc_cb Function pointer to callback function
	@param cb_data Pointer to callback data

	@see targetproc_callback
	@see targetproc_callback_nocbdata

	@return Integer sum of invocation results

*/
int targetproc_list_foreach(struct targetproc_list *list, void *targetproc_cb, void *cb_data) {

	int sum = 0;

	struct targetproc *iter = list->head;

	if ( cb_data )

		while (iter) {
			struct targetproc *next = iter->next;
			sum += ((targetproc_callback)targetproc_cb)(iter, cb_data);
			iter = next;
		}

	else

		while (iter) {
			struct targetproc *next = iter->next;
			sum += ((targetproc_callback_nocbdata)targetproc_cb)(iter);
			iter = next;
		}

	return sum;

}

/* Undocumented */
struct targetproc *targetproc_list_find(struct targetproc_list *list, void *targetproc_cb, void *cb_data) {

	struct targetproc *iter = list->head;

	if ( cb_data ) {

		targetproc_callback cb =
			(targetproc_callback)targetproc_cb;

		while ( iter ) {
			struct targetproc *next = iter->next;
			if ( cb(iter, cb_data) ) return iter;
			iter = next;
		}

	} else {

		targetproc_callback_nocbdata cb =
			(targetproc_callback_nocbdata)targetproc_cb;

		while ( iter ) {
			struct targetproc *next = iter->next;
			if ( cb(iter) ) return iter;
			iter = next;
		}

	}

	return NULL;

}

/*!

	@brief Destroy all nodes and empty the list.

	@param list Pointer to an targetproc_list struct

*/
void targetproc_list_empty(struct targetproc_list *list) {

	targetproc_list_foreach(list, free_targetproc, NULL);

	list->n_targetprocs = 0;

	list->head = NULL;
	list->tail = NULL;

	return;

}

/*!

	@brief Destroy all nodes along with the list.

	@param list Pointer to an targetproc_list struct

*/
void targetproc_list_free(struct targetproc_list *list) {

	targetproc_list_foreach(list, free_targetproc, NULL);

	kfree(list);

	return;

}

/*!

	@brief Print all nodes in the list.

	Printing will be done as specified in print_targetproc.

	@see print_targetproc

	@param list Pointer to an targetproc_list struct

*/
void targetproc_list_print(struct targetproc_list *list) {

	targetproc_list_foreach(list, print_targetproc, NULL);

	return;

}



MODULE_LICENSE("Dual BSD/GPL");



