


#ifndef HANDLE_EXAMPLE_C
#define HANDLE_EXAMPLE_C



#include <linux/kernel.h>
#include <linux/module.h>

#include "../srvcom/srvcom.h"
#include "../ev_handlers/ev_handlers.h"



struct handler_ctx {
};



srvcom_ackcode_t handle_ev_example(struct srvcom_ctx *ctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata, void *cb_data) {
}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* HANDLE_EXAMPLE_C */



