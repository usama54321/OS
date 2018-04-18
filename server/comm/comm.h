


#ifndef COMM_H
#define COMM_H



#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/sock.h>

#include "../ksock/ksock.h"



#define COMM_MAX_HNDLRS 16



/* To type-check requests from responses */
typedef struct {unsigned char code;} comm_opcode_t;
typedef struct {unsigned char code;} comm_ackcode_t;

/* Requests */
#define OPCODE_REQUEST_WRITE	((comm_opcode_t){.code = 0x00})
#define OPCODE_ALLOW_WRITE	((comm_opcode_t){.code = 0x01})
#define OPCODE_COMMIT_PAGE	((comm_opcode_t){.code = 0x02})
#define OPCODE_LOCK_READ	((comm_opcode_t){.code = 0x03})
#define OPCODE_RESUME_READ	((comm_opcode_t){.code = 0x04})
#define OPCODE_INITIAL_READ	((comm_opcode_t){.code = 0x05})
#define OPCODE_PING_ALIVE	((comm_opcode_t){.code = 0x06})

/* Request codes */
#define OPCODE_REQUEST_WRITE_CODE (0x00)
#define OPCODE_ALLOW_WRITE_CODE	(0x01)
#define OPCODE_COMMIT_PAGE_CODE	(0x02)
#define OPCODE_LOCK_READ_CODE	(0x03)
#define OPCODE_RESUME_READ_CODE	(0x04)
#define OPCODE_INITIAL_READ_CODE (0x05)
#define OPCODE_PING_ALIVE_CODE	(0x06)

/* Responses */
#define ACKCODE_REQUEST_WRITE	((comm_ackcode_t){.code = 0x07})
#define ACKCODE_ALLOW_WRITE	((comm_ackcode_t){.code = 0x08})
#define ACKCODE_COMMIT_PAGE	((comm_ackcode_t){.code = 0x09})
#define ACKCODE_LOCK_READ	((comm_ackcode_t){.code = 0x0A})
#define ACKCODE_RESUME_READ	((comm_ackcode_t){.code = 0x0B})
#define ACKCODE_INITIAL_READ	((comm_ackcode_t){.code = 0x0C})
#define ACKCODE_PING_ALIVE	((comm_ackcode_t){.code = 0x0D})
#define ACKCODE_NO_RESPONSE	((comm_ackcode_t){.code = 0x0E})
#define ACKCODE_OP_FAILURE	((comm_ackcode_t){.code = 0x0F})



struct socket;
struct comm_ctx;

typedef union {
	comm_opcode_t op;
	comm_ackcode_t ack;
	unsigned char code;
} comm_code_t;

/* Returns the appropriate response code */
typedef comm_ackcode_t (*comm_handler_t)(struct comm_ctx *ctx, unsigned long vaddr,
	pid_t client_pid, pid_t server_pid, pgd_t *pgd, char *pagedata, void *cb_data, struct socket *sock);

/*
 * TODO:
 *    - Implement sequence numbers before moving
 *      to a datagram protocol
 */
struct comm_ctx {

	struct sockaddr_in serv_addr;
	struct socket *acceptor_sock;
	struct ksock_set *conn_socks;

	long msec_timeout;

	comm_handler_t handlers[COMM_MAX_HNDLRS];
	void *handler_cb_data[COMM_MAX_HNDLRS];

	struct task_struct *srv_thread;

};



struct comm_ctx *comm_ctx_new(void);
void comm_bind_addr(struct comm_ctx *ctx,
	const char *ip, int port);
void comm_set_timeout(struct comm_ctx *ctx, long msecs);
void comm_register_handler(struct comm_ctx *ctx,
	comm_opcode_t opcode, comm_handler_t handler, void *cb_data);
int comm_run(struct comm_ctx *ctx);
int comm_allow_write(struct comm_ctx *ctx, struct socket *conn_sock,
	unsigned long vaddr, pid_t client_pid, pgd_t *pgd);
int comm_lock_read(struct comm_ctx *ctx, struct socket *conn_sock,
	unsigned long vaddr, pid_t client_pid, pgd_t *pgd);
int comm_resume_read(struct comm_ctx *ctx, struct socket *conn_sock,
	unsigned long vaddr, pid_t client_pid, pgd_t *pgd, char *pagedata);
void comm_exit(struct comm_ctx *ctx);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* COMM_H */



