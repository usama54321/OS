


#ifndef SRVCOM_H
#define SRVCOM_H



#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/sock.h>



#define SRVCOM_MAX_HNDLRS 16



/* To type-check requests from responses */
typedef struct {unsigned char code;} srvcom_opcode_t;
typedef struct {unsigned char code;} srvcom_ackcode_t;

/* Requests */
#define OPCODE_REQUEST_WRITE	(srvcom_opcode_t){.code = 0x00}
#define OPCODE_ALLOW_WRITE	(srvcom_opcode_t){.code = 0x01}
#define OPCODE_COMMIT_PAGE	(srvcom_opcode_t){.code = 0x02}
#define OPCODE_LOCK_READ	(srvcom_opcode_t){.code = 0x03}
#define OPCODE_RESUME_READ	(srvcom_opcode_t){.code = 0x04}
#define OPCODE_PING_ALIVE	(srvcom_opcode_t){.code = 0x05}
/* Responses */
#define ACKCODE_REQUEST_WRITE	(srvcom_ackcode_t){.code = 0x06}
#define ACKCODE_ALLOW_WRITE	(srvcom_ackcode_t){.code = 0x07}
#define ACKCODE_COMMIT_PAGE	(srvcom_ackcode_t){.code = 0x08}
#define ACKCODE_LOCK_READ	(srvcom_ackcode_t){.code = 0x09}
#define ACKCODE_RESUME_READ	(srvcom_ackcode_t){.code = 0x0A}
#define ACKCODE_PING_ALIVE	(srvcom_ackcode_t){.code = 0x0B}
#define ACKCODE_NO_RESPONSE	(srvcom_ackcode_t){.code = 0x0C}
#define ACKCODE_OP_FAILURE	(srvcom_ackcode_t){.code = 0x0D}



typedef union {
	srvcom_opcode_t op;
	srvcom_ackcode_t ack;
} srvcom_code_t;



/* Return the appropriate response code */
typedef srvcom_ackcode_t (*srvcom_handler_t)(struct srvcom_ctx *ctx,
	unsigned long vaddr, pid_t pid, pgd_t *pgd, char *pagedata, void *cb_data);



/*
 * TODO:
 *    - Implement sequence numbers before moving
 *      to a datagram protocol
 */
struct srvcom_ctx {

	struct socket *listener_sock;
	struct sockaddr_in serv_addr;

	long msec_timeout;

	struct task_struct *listener_thread;
	spinlock_t listener_inject_lock;
	srvcom_handler_t handlers[SRVCOM_MAX_HNDLRS];
	void *handler_cb_data[SRVCOM_MAX_HNDLRS];

	int write_try_count;

};

struct srvcom_ctx *srvcom_ctx_new(void);
void srvcom_set_serv_addr(struct srvcom_ctx *ctx,
	const char *ip, int port);
void srvcom_set_timeout(struct srvcom_ctx *ctx, long msecs);
void srvcom_register_handler(struct srvcom_ctx *ctx,
	srvcom_opcode_t opcode, srvcom_handler_t handler, void *cb_data);
int srvcom_run(struct srvcom_ctx *ctx);
int srvcom_request_write(struct srvcom_ctx *ctx, unsigned long addr,
	 pid_t pid, pgd_t *pgd);
int srvcom_commit_page(struct srvcom_ctx *ctx, unsigned long addr,
	 pid_t pid, pgd_t *pgd, char *pagedata);
void srvcom_exit(struct srvcom_ctx *ctx);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* SRVCOM_H */



