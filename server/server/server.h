#ifndef HGA_SERVER
#define HGA_SERVER

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "server.h"
#include "../pgtable/pgtable.h"
#include "../comm/comm.h"
#include "../hashtable/hashtable.h"
#include "../ev_handlers/ev_handlers.h"

int init_server(void);
void attach_handlers(struct comm_ctx*);

#endif
