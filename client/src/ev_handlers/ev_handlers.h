


/*
 * DESCRIPTION:
 *    Contains declarations of the handle_ev_* event
 *    handlers for messages from the central server.
 */



#ifndef EV_HANDLERS_H
#define EV_HANDLERS_H



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"



#define DECLARE_HANDLER(handler) \
	srvcom_ackcode_t handler(struct srvcom_ctx*, \
		unsigned long, pid_t, pgd_t*, char*, void*)



DECLARE_HANDLER(handle_ev_allow_write);
DECLARE_HANDLER(handle_ev_lock_read);
DECLARE_HANDLER(handle_ev_resume_read);
DECLARE_HANDLER(handle_ev_ping_alive);



#undef DECLARE_HANDLER



MODULE_LICENSE("Dual BSD/GPL");



#endif /* EV_HANDLERS_H */



