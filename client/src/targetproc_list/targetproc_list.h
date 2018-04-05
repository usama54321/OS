


/*!

	@file targetproc_list.h
	@brief Header file for the targetproc_list implementation.

*/



#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pgtable_types.h>

#include "../common/hga_defs.h"



/*!

	@brief Node in the targetproc_list structure.

*/
struct targetproc {

	/// Next node in the list
	struct targetproc *next;

	// Your data here
	int pid; pgd_t *pgd;
	char procname[PROCNAME_MAXLEN];

};

/*!

	@brief List of targetproc nodes

	Add description here.

*/
struct targetproc_list {

	/// Number of nodes in the list
	int n_targetprocs;

	/// List head
	struct targetproc *head;

	/// List tail
	struct targetproc *tail;

};

/*!

	@brief Callback function type for targetproc_list_foreach.

	The function pointer passed to targetproc_list_foreach will
	be casted to this type when the cb_data argument passed to the
	function is non-null.

*/
typedef int (*targetproc_callback)(struct targetproc*, void*);

/*!

	@brief Callback function type for targetproc_list_foreach
	without a pointer to callback data.

	The function pointer passed to targetproc_list_foreach will
	be casted to this type when the cb_data argument passed to the
	function is NULL. This removes the need to add a callback data
	argument in the callback functions where it is unnecessary.

*/
typedef int (*targetproc_callback_nocbdata)(struct targetproc*);



struct targetproc *targetproc_new(void);
struct targetproc_list *targetproc_list_new(void);
void targetproc_list_push_front(struct targetproc_list *list, struct targetproc *targetproc); // At head
void targetproc_list_push_back(struct targetproc_list *list, struct targetproc *targetproc); // At tail
void targetproc_list_push(struct targetproc_list *list, struct targetproc *targetproc); // At head
void targetproc_list_insert(struct targetproc_list *list, struct targetproc *targetproc); // At tail
struct targetproc *targetproc_list_front(struct targetproc_list *list); // Head
struct targetproc *targetproc_list_back(struct targetproc_list *list); // Tail
struct targetproc *targetproc_list_top(struct targetproc_list *list); // Head
int targetproc_list_pop_front(struct targetproc_list *list); // Head
int targetproc_list_pop(struct targetproc_list *list); // Head
int targetproc_list_foreach(struct targetproc_list *list, void *targetproc_cb, void *cb_data);
struct targetproc *targetproc_list_find(struct targetproc_list *list, void *targetproc_cb, void *cb_data);
void targetproc_list_empty(struct targetproc_list *list);
void targetproc_list_free(struct targetproc_list *list);
void targetproc_list_print(struct targetproc_list *list);



MODULE_LICENSE("Dual BSD/GPL");



