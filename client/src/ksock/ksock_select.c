


/*
 * DESCRIPTION:
 *    Select-like interface for event driven socket IO
 *    without kthreads.
 */



#ifndef KSOCK_SELECT_C
#define KSOCK_SELECT_C



#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/sock.h>

#include "../ksock/ksock.h"



#define SETELEMS_FOREACH(set, statements) {		\
	int set_ind;					\
	for (						\
		set_ind = 0;				\
		set_ind < (set)->setsz;			\
		set_ind++				\
	) {						\
		struct socket *set_elem =		\
			(set)->sockset[set_ind];	\
		{statements};				\
	}						\
}
#define SETELEMS_SWAP(set, i, j) {			\
	struct socket *tmp =				\
		(set)->sockset[(i)];			\
	(set)->sockset[(i)] =				\
		(set)->sockset[(j)];			\
	(set)->sockset[(j)] = tmp;			\
}
#define SETELEMS_REMOVE(set, i) {			\
	SETELEMS_SWAP((set), (i), (set)->setsz-1);	\
	(set)->sockset[(set)->setsz-1] = NULL;		\
	(set)->setsz -= 1;				\
}



typedef int (*status_cb_t)(struct socket*);



struct ksock_set *ksock_set_create(void) {

	struct ksock_set *set =
		kmalloc(sizeof(struct ksock_set), GFP_KERNEL);

	if ( !set ) {
		printk(KERN_ERR "WARNING: ksock_set_create: Set "
			"allocation failed");
		return NULL;
	}

	set->setsz = 0;
	memset(set->sockset, 0, sizeof(set->sockset));

	return set;

}

int ksock_contains(struct ksock_set *set, struct socket *sock) {

	SETELEMS_FOREACH(set, {
		if ( set_elem == sock )
			return 1;
	});

	return 0;

}

int ksock_insert(struct ksock_set *set, struct socket *sock) {

	if ( set->setsz >= MAX_SETSZ )
		return -1;

	set->sockset[set->setsz++] = sock;

	return 0;

}

void ksock_remove(struct ksock_set *set, struct socket *sock) {

	SETELEMS_FOREACH(set, {
		if ( set_elem == sock ) {
			SETELEMS_REMOVE(set, set_ind);
			break;
		}
	});

	return;

}

void ksock_clear(struct ksock_set *set, struct socket *sock) {

	set->setsz = 0;
	memset(set->sockset, 0, sizeof(set->sockset));

	return;

}

static inline void __filter_setelems(struct ksock_set *set, status_cb_t ready) {

	SETELEMS_FOREACH(set, {
		if ( !ready(set_elem) )
			SETELEMS_REMOVE(set, set_ind);
	});

	return;

}

static inline int __ksock_set_ready(struct ksock_set *set, status_cb_t ready) {

	SETELEMS_FOREACH(set, {
		if ( ready(set_elem) )
			return 1;
	});

	return 0;

}

/*!
 * @brief Waits until a set has a ready (for I/O) socket.
 *
 * Waits until a set has a ready (for I/O) socket or a timeout
 * has expired, then filters the sockets from the given socket
 * sets that are ready.
 *
 * @param accept_set Pointer to a struct ksock_set containing
 * the sockets listening for connections to be accepted.
 * @param recv_set Pointer to a struct ksock_set containing
 * the sockets listening for data to be received.
 * @param timeout_msecs Maximum number of milliseconds to wait
 * for before a ready socket is found.
 *
 * @return Number of sockets ready for an I/O operation.
 */
int ksock_select(struct ksock_set *accept_set, struct ksock_set *recv_set,
	unsigned long timeout_msecs) {

#define EITHER_READY (						\
	__ksock_set_ready(accept_set, ksock_accept_ready)	\
	|| __ksock_set_ready(recv_set, ksock_recv_ready)	\
)

	int n_sockets_ready;
	DECLARE_WAIT_QUEUE_HEAD(ksock_select_wait_queue);

	wait_event_timeout(ksock_select_wait_queue,
		EITHER_READY, msecs_to_jiffies(timeout_msecs));

	__filter_setelems(accept_set, ksock_accept_ready);
	__filter_setelems(recv_set, ksock_recv_ready);

	n_sockets_ready = accept_set->setsz + recv_set->setsz;

	return n_sockets_ready;

#undef EITHER_READY

}

void ksock_set_destroy(struct ksock_set *set) {

	kfree(set);

	return;

}



#undef SETELEMS_REMOVE
#undef SETELEMS_SWAP
#undef SETELEMS_FOREACH



MODULE_LICENSE("Dual BSD/GPL");



#endif /* KSOCK_SELECT_C */



