


#ifndef HANDLE_PING_ALIVE_C
#define HANDLE_PING_ALIVE_C



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"
#include "../ev_handlers/ev_handlers.h"



static int alive_status(void) {

	/* TODO: -1 if we wish to leave the network */

	return 0;

}

srvcom_ackcode_t handle_ev_ping_alive(struct srvcom_ctx *ctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata, void *cb_data) {

	if ( alive_status() < 0 )
		return ACKCODE_OP_FAILURE;

	return ACKCODE_PING_ALIVE;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_PING_ALIVE_C */



