


#ifndef COMM_C
#define COMM_C



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
#include "../comm/comm.h"



#define DFT_TIMEOUT_MSECS	10
#define CONN_BACKLOG		16

#define ISNUM(c) ('0' <= (c) && (c) <= '9')
#define TONUM(c) ((int)(c - '0'))



struct comm_msg_hdr {

	comm_code_t mcode;

	unsigned long vaddr;
	pid_t client_pid;
	pid_t server_pid;
	pgd_t *pgd;

	int payload_len;

} __attribute__((packed));

struct comm_msg_data {

	char payload[0];

} __attribute__((packed));

struct comm_msg {

	struct comm_msg_hdr hdr;
	struct comm_msg_data data;

} __attribute__((packed));



///////////////////////////////////////////////////
///////////////////// HELPERS /////////////////////
///////////////////////////////////////////////////

static int str2ip(const char *ipstr) {

	int ip, i;

	--ipstr;

	for ( i = 0; i < 4; i++ ) {
		int byte = 0;
		while ( ISNUM(*++ipstr) )
			byte = 10*byte + TONUM(*ipstr);
		ip <<= 8;
		ip += byte;
	}

	return ip;

}

static int comm_send(struct socket *sock, struct comm_msg *msg) {

	return ksock_send(sock, (char*)msg,
		sizeof(msg->hdr) + msg->hdr.payload_len);

}

static int comm_recv(struct socket *sock, struct comm_msg *msg) {

	/* Receive header */
	if ( ksock_recv(sock, (char*)&msg->hdr, sizeof(msg->hdr)) < 0 ) {
		printk(KERN_INFO "comm_recv: ksock_recv failed");
		return -1;
	}

	/* Receive payload */
	if ( ksock_recv(sock, (char*)&msg->data, msg->hdr.payload_len) < 0 ) {
		printk(KERN_INFO "comm_recv: ksock_recv failed");
		return -1;
	}

	return 0;

}

static int comm_timeout_recv(struct socket *sock, struct comm_msg *msg,
	unsigned long msec_timeout) {

	int err_code;

	/* Receive header */
	err_code = ksock_recv_timeout(sock, (char*)&msg->hdr,
		sizeof(msg->hdr), msec_timeout);
	if ( err_code < 0 ) {
		printk(KERN_INFO "comm_timeout_recv: ksock_recv failed");
		return -1;
	} else if ( err_code > 0 ) {
		printk(KERN_INFO "comm_timeout_recv: ksock_recv timed out");
		return 1;
	}

	/* Receive payload */
	err_code = ksock_recv(sock, (char*)&msg->data, msg->hdr.payload_len);

	return err_code;

}

//////////////////////////////////////////////////////////
///////////////////// MAIN FUNCTIONS /////////////////////
//////////////////////////////////////////////////////////

static int __handle_accept(struct socket *acceptor_sock, struct comm_ctx *ctx) {

	struct sockaddr_in addr;
	int addrlen = sizeof(addr);

	struct socket *conn_sock = ksock_accept(acceptor_sock,
		(struct sockaddr*)&addr, &addrlen);

	if ( !conn_sock ) {
		printk(KERN_ERR "__handle_accept: Failed to accept connection");
		return -1;
	}

	if ( ksock_insert(ctx->conn_socks, conn_sock) < 0 ) {
		printk(KERN_ERR "__handle_accept: Connect limit full");
		ksock_socket_destroy(conn_sock);
		return -1;
	}

	return 0;

}

static int __handle_recv(struct socket *conn_sock, struct comm_ctx *ctx) {

	struct comm_msg *msg, ack;
	comm_ackcode_t ack_code;
	comm_handler_t msg_handler;
	void *handler_cb_data;

	unsigned mcode;
	pgd_t *msg_pgd;
	char *msg_page;
	unsigned long msg_vaddr;
	pid_t msg_cpid, msg_spid;

	/* Message buffer */
	msg = (struct comm_msg*)kmalloc(
		sizeof(struct comm_msg) + PAGE_SIZE, GFP_KERNEL);
	if ( !msg ) {
		printk(KERN_ERR "__handle_recv: Allocation failure");
		return -1;
	}

	/* Receive message */
	if ( comm_recv(conn_sock, msg) < 0 ) {
		printk(KERN_INFO "__handle_recv: Lost connection "
			"with the client");
		goto err;
	}

	/* Get appropriate handler */
	mcode = (unsigned)(msg->hdr.mcode.op.code);
	msg_handler = ctx->handlers[mcode];
	handler_cb_data = ctx->handler_cb_data[mcode];

	if ( !msg_handler )
		goto out;

	/* Run handler and get response code */
	msg_vaddr = msg->hdr.vaddr;
	msg_page = msg->data.payload;
	msg_cpid = msg->hdr.client_pid;
	msg_spid = msg->hdr.server_pid;
	msg_pgd = msg->hdr.pgd;
	ack_code = msg_handler(ctx, msg_vaddr, msg_cpid,
		msg_spid, msg_pgd, msg_page, handler_cb_data);

	if ( ack_code.code == ACKCODE_NO_RESPONSE.code )
		goto out;

	/* Send off acknowledgement */
	ack.hdr.mcode = (comm_code_t)ack_code;
	ack.hdr.vaddr = msg_vaddr;
	ack.hdr.client_pid = msg_cpid;
	ack.hdr.server_pid = msg_spid;
	ack.hdr.pgd = msg_pgd;
	ack.hdr.payload_len = 0;
	if ( comm_send(conn_sock, &ack) < 0 ) {
		printk(KERN_INFO "__handle_recv: Lost connection "
			"with the client");
		goto err;
	}

out:
	kfree(msg);
	return 0;

err:
	ksock_socket_destroy(conn_sock);
	ksock_remove(ctx->conn_socks, conn_sock);
	kfree(msg);
	return -1;

}

/*
 * Main server loop
 *    Started in comm_run() after the context is initialized.
 *    The thread will listen for incoming connections from the
 *    acceptor socket and incoming messages from the connection
 *    sockets and call the designated handler registered in ctx
 *    with the received message.
 */
static int __server_loop_run(void *thrdata) {

	struct comm_ctx *ctx =
		(struct comm_ctx*)thrdata;
	struct ksock_set *accept_set, *recv_set;

	/* Accept set */
	if ( !(accept_set = ksock_set_create()) ) {
		printk(KERN_ERR "__server_loop_run: Failed to create accept set");
		return -1;
	}

	/* Receive set */
	if ( !(recv_set = ksock_set_create()) ) {
		printk(KERN_ERR "__server_loop_run: Failed to create receive set");
		ksock_set_destroy(accept_set);
		return -1;
	}

	allow_signal(SIGKILL|SIGTERM);

	while ( !kthread_should_stop() ) {

		int err_code;
		int n_sockets_selected;
		int accept_count, recv_count, i;

		ksock_clear(accept_set);
		ksock_clear(recv_set);

		ksock_insert(accept_set, ctx->acceptor_sock);
		ksock_setcpy(recv_set, ctx->conn_socks);

		n_sockets_selected =
			ksock_select(accept_set, recv_set, ctx->msec_timeout);

		if ( n_sockets_selected == 0 )
			continue;

		accept_count = accept_set->setsz;
		recv_count = recv_set->setsz;

		err_code = 0;
		for ( i = 0; i < accept_count && err_code == 0; i++ )
			err_code = __handle_accept(accept_set->sockset[i], ctx);
		for ( i = 0; i < recv_count && err_code == 0; i++ )
			err_code = __handle_recv(recv_set->sockset[i], ctx);

		if ( err_code < 0 )
			printk(KERN_INFO "__server_loop_run: Handler failure");

	}

	ksock_set_destroy(accept_set);
	ksock_set_destroy(recv_set);

	return 0;

}

/////////////////////////////////////////////////////
///////////////////// INTERFACE /////////////////////
/////////////////////////////////////////////////////

struct comm_ctx *comm_ctx_new(void) {

	struct comm_ctx *ctx;

	if ( !(ctx = kmalloc(sizeof(struct comm_ctx), GFP_KERNEL)) ) {
		printk(KERN_ERR "comm_ctx_new: Allocation failure");
		return NULL;
	}

	memset(&(ctx->serv_addr), 0, sizeof(ctx->serv_addr));
	ctx->acceptor_sock = NULL;
	if ( !(ctx->conn_socks = ksock_set_create()) ) {
		printk(KERN_ERR "comm_ctx_new: Set creation failure");
		kfree(ctx);
		return NULL;
	}
	ctx->msec_timeout = DFT_TIMEOUT_MSECS;
	memset(ctx->handlers, 0, sizeof(ctx->handlers));
	memset(ctx->handler_cb_data, 0, sizeof(ctx->handler_cb_data));
	ctx->srv_thread = NULL;

	return ctx;

}

void comm_bind_addr(struct comm_ctx *ctx,
	const char *ip, int port) {

	memset(&(ctx->serv_addr), 0, sizeof(ctx->serv_addr));
	ctx->serv_addr.sin_family = PF_INET;
	ctx->serv_addr.sin_port = htons(port);
	ctx->serv_addr.sin_addr.s_addr = htonl(str2ip(ip));

	return;

}

void comm_set_timeout(struct comm_ctx *ctx, long msecs) {

	ctx->msec_timeout = msecs;

	return;

}

void comm_register_handler(struct comm_ctx *ctx,
	comm_opcode_t opcode, comm_handler_t handler, void *cb_data) {

	unsigned opcode_index = (unsigned)opcode.code;

	ctx->handlers[opcode_index] = handler;
	ctx->handler_cb_data[opcode_index] = cb_data;

	return;

}

/* Start the main server loop */
int comm_run(struct comm_ctx *ctx) {

	int err_code;

	if ( !(ctx->acceptor_sock = ksock_socket_create()) ) {
		printk(KERN_ERR "comm_run: Socket creation failure");
		return -1;
	}

	err_code = ksock_bind(ctx->acceptor_sock,
		(struct sockaddr*)&ctx->serv_addr, sizeof(ctx->serv_addr));
	if ( err_code < 0 ) {
		printk(KERN_ERR "comm_run: Address binding failed");
		ksock_socket_destroy(ctx->acceptor_sock);
		ctx->acceptor_sock = NULL;
		return -1;
	}

	if ( ksock_listen(ctx->acceptor_sock, CONN_BACKLOG) < 0 ) {
		printk(KERN_ERR "comm_run: Failed to set acceptor socket "
			"in listening state");
		ksock_socket_destroy(ctx->acceptor_sock);
		ctx->acceptor_sock = NULL;
		return -1;
	}

	ctx->srv_thread =
		kthread_run(__server_loop_run, ctx, "Main server thread");
	if ( !ctx->srv_thread ) {
		printk(KERN_ERR "comm_run: Failed to run main server thread");
		ksock_socket_destroy(ctx->acceptor_sock);
		ctx->acceptor_sock = NULL;
		return -1;
	}

	return 0;

}

/*
 * @brief Command a client to allow writing on a page
 *
 * @param ctx Server context
 * @param conn_sock Socket with which the client connected
 * @param vaddr Virtual address of the page to unlock
 * @param client_pid PID of the process running on the
 * target machine
 * @param pgd Pointer to the PGD table of the page to
 * be unlocked
 *
 * @return 1 if the request was sent AND acknowledged,
 * -1 on error and 0 otherwise
 */
int comm_allow_write(struct comm_ctx *ctx, struct socket *conn_sock,
	unsigned long vaddr, pid_t client_pid, pgd_t *pgd) {

	int n_tries_remaining = 8;

	while ( n_tries_remaining --> 0 ) {

		int err_code;
		struct comm_msg msg = { .hdr = {
			.mcode = (comm_code_t)OPCODE_ALLOW_WRITE,
			.vaddr = vaddr,
			.client_pid = client_pid,
			.pgd = pgd,
			.payload_len = 0,
		}};

		if ( comm_send(conn_sock, &msg) < 0 ) {
			printk(KERN_INFO "comm_allow_write: Lost connection "
				"with the client");
			goto err;
		}

		err_code = comm_timeout_recv(conn_sock, &msg,
			ctx->msec_timeout);
		if ( err_code < 0 ) {
			printk(KERN_INFO "comm_allow_write: Lost connection "
				"with the client");
			goto err;
		} else if ( err_code > 0 ) {
			printk(KERN_INFO "comm_allow_write: Client timed out");
			continue;
		}

		/* Should not happen but handle this case anyway */
		if (	/* Check if the reply has anything unexpected */
			(msg.hdr.mcode.ack.code != ACKCODE_ALLOW_WRITE.code)
			|| (msg.hdr.vaddr != vaddr)
			|| (msg.hdr.client_pid != client_pid)
			|| (msg.hdr.pgd != pgd)
			|| (msg.hdr.payload_len != 0)
		) {
			printk(KERN_ERR "WARNING: Unexpected acknowledgement");
			continue;
		}

		break;

	}

	return (n_tries_remaining < 0) ? 0 : 1;

err:
	ksock_remove(ctx->conn_socks, conn_sock);
	ksock_socket_destroy(conn_sock);
	return -1;

}

/*
 * @brief Command a client to block reads on a page
 *
 * @param ctx Server context
 * @param conn_sock Socket with which the client connected
 * @param vaddr Virtual address of the page to unlock
 * @param client_pid PID of the process running on the
 * target machine
 * @param pgd Pointer to the PGD table of the page to
 * be unlocked
 *
 * @return 1 if the request was sent AND acknowledged,
 * -1 on error and 0 otherwise
 */
int comm_lock_read(struct comm_ctx *ctx, struct socket *conn_sock,
	unsigned long vaddr, pid_t client_pid, pgd_t *pgd) {

	int n_tries_remaining = 8;

	while ( n_tries_remaining --> 0 ) {

		int err_code;
		struct comm_msg msg = { .hdr = {
			.mcode = (comm_code_t)OPCODE_LOCK_READ,
			.vaddr = vaddr,
			.client_pid = client_pid,
			.pgd = pgd,
			.payload_len = 0,
		}};

		if ( comm_send(conn_sock, &msg) < 0 ) {
			printk(KERN_INFO "comm_lock_read: Lost connection "
				"with the client");
			goto err;
		}

		err_code = comm_timeout_recv(conn_sock, &msg,
			ctx->msec_timeout);
		if ( err_code < 0 ) {
			printk(KERN_INFO "comm_lock_read: Lost connection "
				"with the client");
			goto err;
		} else if ( err_code > 0 ) {
			printk(KERN_INFO "comm_lock_read: Client timed out");
			continue;
		}

		/* Should not happen but handle this case anyway */
		if (	/* Check if the reply has anything unexpected */
			(msg.hdr.mcode.ack.code != ACKCODE_LOCK_READ.code)
			|| (msg.hdr.vaddr != vaddr)
			|| (msg.hdr.client_pid != client_pid)
			|| (msg.hdr.pgd != pgd)
			|| (msg.hdr.payload_len != 0)
		) {
			printk(KERN_ERR "WARNING: Unexpected acknowledgement");
			continue;
		}

		break;

	}

	return (n_tries_remaining < 0) ? 0 : 1;

err:
	ksock_remove(ctx->conn_socks, conn_sock);
	ksock_socket_destroy(conn_sock);
	return -1;

}

/*
 * @brief Command a client to unblock reads on a page and
 * send the new page that was modified during the block
 *
 * @param ctx Server context
 * @param conn_sock Socket with which the client connected
 * @param vaddr Virtual address of the page to unlock
 * @param client_pid PID of the process running on the
 * target machine
 * @param pgd Pointer to the PGD table of the page to
 * be unlocked
 * @param pagedata Pointer to the modified page data
 *
 * @return 1 if the request was sent AND acknowledged,
 * -1 on error and 0 otherwise
 */
int comm_resume_read(struct comm_ctx *ctx, struct socket *conn_sock,
	unsigned long vaddr, pid_t client_pid, pgd_t *pgd, char *pagedata) {

	int n_tries_remaining = 8;
	struct comm_msg *msg;

	msg = (struct comm_msg*)kmalloc(
		sizeof(struct comm_msg) + PAGE_SIZE, GFP_KERNEL);
	if ( !msg ) {
		printk(KERN_ERR "comm_resume_read: Allocation failure");
		return -1;
	}

	msg->hdr.mcode = (comm_code_t)OPCODE_RESUME_READ;
	msg->hdr.vaddr = vaddr;
	msg->hdr.client_pid = client_pid;
	msg->hdr.pgd = pgd;
	msg->hdr.payload_len = PAGE_SIZE;
	memcpy(msg->data.payload, pagedata, PAGE_SIZE);

	while ( n_tries_remaining --> 0 ) {

		int err_code;

		if ( comm_send(conn_sock, msg) < 0 ) {
			printk(KERN_INFO "comm_resume_read: Lost connection "
				"with the client");
			goto err;
		}

		err_code = comm_timeout_recv(conn_sock, msg,
			ctx->msec_timeout);
		if ( err_code < 0 ) {
			printk(KERN_INFO "comm_resume_read: Lost connection "
				"with the client");
			goto err;
		} else if ( err_code > 0 ) {
			printk(KERN_INFO "comm_resume_read: Client timed out");
			continue;
		}

		/* Should not happen but handle this case anyway */
		if (	/* Check if the reply has anything unexpected */
			(msg->hdr.mcode.ack.code != ACKCODE_RESUME_READ.code)
			|| (msg->hdr.vaddr != vaddr)
			|| (msg->hdr.client_pid != client_pid)
			|| (msg->hdr.pgd != pgd)
			|| (msg->hdr.payload_len != 0)
		) {	/* Recreate message */
			msg->hdr.mcode = (comm_code_t)OPCODE_RESUME_READ;
			msg->hdr.vaddr = vaddr;
			msg->hdr.client_pid = client_pid;
			msg->hdr.pgd = pgd;
			msg->hdr.payload_len = PAGE_SIZE;
			memcpy(msg->data.payload, pagedata, PAGE_SIZE);
			printk(KERN_ERR "WARNING: Unexpected acknowledgement");
			continue;
		}

		break;

	}

	kfree(msg);
	return (n_tries_remaining < 0) ? 0 : 1;

err:
	kfree(msg);
	ksock_remove(ctx->conn_socks, conn_sock);
	ksock_socket_destroy(conn_sock);
	return -1;

}

void comm_exit(struct comm_ctx *ctx) {

	if ( !ctx )
		return;

	if ( ctx->srv_thread )
		kthread_stop(ctx->srv_thread);
	if ( ctx->acceptor_sock )
		ksock_socket_destroy(ctx->acceptor_sock);
	if ( ctx->conn_socks )
		ksock_set_destroy(ctx->conn_socks);

	kfree(ctx);

	return;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* COMM_C */



