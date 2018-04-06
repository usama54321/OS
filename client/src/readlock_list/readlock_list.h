


/*!

	@file readlock_list.h
	@brief Header file for the readlock_list implementation.

*/



#ifndef READLOCK_LIST_H
#define READLOCK_LIST_H



#include <linux/pfn_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/pgtable_types.h>



/*!

	@brief Node in the readlock_list structure.

*/
struct readlock {

	/// Next node in the list
	struct readlock *next;

	int resolved;
	pgd_t *pgd;
	pfn_t pfn;
	char page_data[PAGE_SIZE];

};

/*!

	@brief List of readlock nodes

	Add description here.

*/
struct readlock_list {

	/// Number of nodes in the list
	int n_readlocks;

	/// List head
	struct readlock *head;

	/// List tail
	struct readlock *tail;

	/// Lock for list operations
	spinlock_t lock; /* TODO: Use this lock */

};

/*!

	@brief Callback function type for readlock_list_foreach.

	The function pointer passed to readlock_list_foreach will
	be casted to this type when the cb_data argument passed to the
	function is non-null.

*/
typedef int (*readlock_callback)(struct readlock*, void*);

/*!

	@brief Callback function type for readlock_list_foreach
	without a pointer to callback data.

	The function pointer passed to readlock_list_foreach will
	be casted to this type when the cb_data argument passed to the
	function is NULL. This removes the need to add a callback data
	argument in the callback functions where it is unnecessary.

*/
typedef int (*readlock_callback_nocbdata)(struct readlock*);



struct readlock *readlock_new(void);
struct readlock_list *readlock_list_new(void);

void readlock_list_push_front(struct readlock_list *list, struct readlock *readlock); // At head
void readlock_list_push_back(struct readlock_list *list, struct readlock *readlock); // At tail
void readlock_list_push(struct readlock_list *list, struct readlock *readlock); // At head
void readlock_list_insert(struct readlock_list *list, struct readlock *readlock); // At tail

struct readlock *readlock_list_front(struct readlock_list *list); // Head
struct readlock *readlock_list_back(struct readlock_list *list); // Tail
struct readlock *readlock_list_top(struct readlock_list *list); // Head

int readlock_list_pop_front(struct readlock_list *list); // Head
int readlock_list_pop(struct readlock_list *list); // Head

int readlock_list_foreach(struct readlock_list *list, void *readlock_cb, void *cb_data);
struct readlock *readlock_list_find(struct readlock_list *list, void *readlock_cb, void *cb_data);

void readlock_list_delete(struct readlock_list *list, void *readlock_cb, void *cb_data);
void readlock_list_delete_all(struct readlock_list *list, void *readlock_cb, void *cb_data);
void readlock_list_empty(struct readlock_list *list);
void readlock_list_free(struct readlock_list *list);

void readlock_list_print(struct readlock_list *list);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* READLOCK_LIST_H */



