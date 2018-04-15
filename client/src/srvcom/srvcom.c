


/*

	DESCRIPTION:
		Everything related to communication with the
		central server

*/



#ifndef SRVCOM_C
#define SRVCOM_C



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
#include "../srvcom/srvcom.h"
#include "../common/hga_defs.h"



#define TRIES_PER_REQUEST	64
#define DFT_TIMEOUT_MSECS	10

#define ISNUM(c) ('0' <= (c) && (c) <= '9')
#define TONUM(c) ((int)(c - '0'))



struct srvcom_msg_hdr {

	srvcom_code_t mcode;

	unsigned long vaddr;
	pid_t pid;
	pgd_t *pgd;

	int payload_len;

} __attribute__((packed));

struct srvcom_msg_data {

	char payload[0];

} __attribute__((packed));

struct srvcom_msg {

	struct srvcom_msg_hdr hdr;
	struct srvcom_msg_data data;

} __attribute__((packed));



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

/* Send a message on behalf of the listener thread */
static int srvcom_listener_inject(struct srvcom_ctx *ctx, struct srvcom_msg *msg) {

	int err_code;

	spin_lock(&ctx->listener_inject_lock);
	err_code = ksock_send(ctx->listener_sock, (char*)msg,
		sizeof(msg->hdr) + msg->hdr.payload_len);
	spin_unlock(&ctx->listener_inject_lock);

	return err_code;

}

static int srvcom_send(struct socket *sock, struct srvcom_msg *msg) {

	return ksock_send(sock, (char*)msg,
		sizeof(msg->hdr) + msg->hdr.payload_len);

}

static int __attribute__((unused)) srvcom_recv(struct socket *sock, struct srvcom_msg *msg) {

	/* Receive header */
	if ( ksock_recv(sock, (char*)&msg->hdr, sizeof(msg->hdr)) < 0 ) {
		printk(KERN_INFO "srvcom_recv: ksock_recv failed");
		return -1;
	}

	/* Receive payload */
	if ( ksock_recv(sock, (char*)&msg->data, msg->hdr.payload_len) < 0 ) {
		printk(KERN_INFO "srvcom_recv: ksock_recv failed");
		return -1;
	}

	return 0;

}

static int srvcom_timeout_recv(struct socket *sock, struct srvcom_msg *msg,
	unsigned long msec_timeout) {

	int err_code;

	/* Receive header */
	err_code = ksock_recv_timeout(sock, (char*)&msg->hdr,
		sizeof(msg->hdr), msec_timeout);
	if ( err_code < 0 ) {
		printk(KERN_INFO "srvcom_timeout_recv: ksock_recv failed");
		return -1;
	} else if ( err_code > 0 ) {
		printk(KERN_INFO "srvcom_timeout_recv: ksock_recv timed out");
		return 1;
	}

	/* Receive payload */
	err_code = ksock_recv(sock, (char*)&msg->data, msg->hdr.payload_len);

	return err_code;

}

/*
 * Listener thread
 *    Started in srvcom_run() after the context is initialized.
 *    The thread will listen for incoming data from the central
 *    server and call the designated handler registered in ctx
 *    with the received message, so if a message response is to
 *    be processed by a handler then send the message on behalf
 *    of this thread through srvcom_listener_inject() to direct
 *    the response to the listener socket.
 */
static int srvcom_listener_thread(void *thrdata) {

	int err_code;
	struct srvcom_msg *msg;
	struct srvcom_ctx *ctx =
		(struct srvcom_ctx*)thrdata;

	allow_signal(SIGKILL|SIGTERM);

	msg = (struct srvcom_msg*)kmalloc(
		sizeof(struct srvcom_msg) + PAGE_SIZE, GFP_KERNEL);
	if ( !msg ) {
		printk(KERN_INFO "srvcom_listener_thread: Allocation failure");
		return -1;
	}

	if ( !(ctx->listener_sock = ksock_socket_create()) ) {
		printk(KERN_INFO "srvcom_listener_thread: Failed to create socket");
		kfree(msg);
		return -1;
	}

	err_code = ksock_connect(ctx->listener_sock,
		(struct sockaddr*)&ctx->serv_addr, sizeof(ctx->serv_addr));
	if ( err_code < 0 ) {
		printk(KERN_INFO "srvcom_listener_thread: Failed to connect to server");
		ksock_socket_destroy(ctx->listener_sock);
		kfree(msg);
		return -1;
	}

	while ( !kthread_should_stop() ) {

		int err_code;
		void *handler_cb_data;
		struct srvcom_msg ack;
		srvcom_ackcode_t ack_code;
		srvcom_handler_t msg_handler;

		unsigned mcode;
		unsigned long msg_vaddr;
		pid_t msg_pid;
		pgd_t *msg_pgd;
		char *msg_page;

		/* Receive message */
		err_code = srvcom_timeout_recv(ctx->listener_sock, msg, ctx->msec_timeout);
		if ( err_code < 0 ) {
			printk(KERN_INFO "srvcom_listener_thread: Lost connection");
			ksock_socket_destroy(ctx->listener_sock);
			kfree(msg);
			return -1;
		} else if ( err_code > 0 ) {
			printk(KERN_INFO "srvcom_listener_thread: Timed out");
			continue;
		}

		/* Get appropriate handler */
		mcode = (unsigned)(msg->hdr.mcode.op.code);
		msg_handler = ctx->handlers[mcode];
		handler_cb_data = ctx->handler_cb_data[mcode];

		/* If permission was granted then reset the try count */
		if ( msg->hdr.mcode.op.code == OPCODE_ALLOW_WRITE.code )
			ctx->write_try_count = 0;

		if ( !msg_handler )
			continue;

		/* Run handler and get response code */
		msg_vaddr = msg->hdr.vaddr;
		msg_page = msg->data.payload;
		msg_pid = msg->hdr.pid;
		msg_pgd = msg->hdr.pgd;
		ack_code = msg_handler(ctx, msg_vaddr,
			msg_pid, msg_pgd, msg_page, handler_cb_data);

		if ( ack_code.code == ACKCODE_NO_RESPONSE.code )
			continue;

		/* Send off acknowledgement */
		ack.hdr.mcode = (srvcom_code_t)ack_code;
		ack.hdr.vaddr = msg_vaddr;
		ack.hdr.pid = msg_pid;
		ack.hdr.pgd = msg_pgd;
		ack.hdr.payload_len = 0;
		spin_lock(&ctx->listener_inject_lock);
		err_code = srvcom_send(ctx->listener_sock, &ack);
		spin_unlock(&ctx->listener_inject_lock);
		if ( err_code < 0 ) {
			printk(KERN_INFO "srvcom_listener_thread: Lost connection");
			ksock_socket_destroy(ctx->listener_sock);
			kfree(msg);
			return -1;
		}

	}

	kfree(msg);

	return 0;

}



struct srvcom_ctx *srvcom_ctx_new(void) {

	struct srvcom_ctx *ctx;

	if ( !(ctx = kmalloc(sizeof(struct srvcom_ctx), GFP_KERNEL)) ) {
		printk(KERN_ERR "srvcom: Context allocation failure");
		return NULL;
	}

	// Defaults
	ctx->listener_sock = NULL;
	memset(&(ctx->serv_addr), 0, sizeof(ctx->serv_addr));
	ctx->msec_timeout = DFT_TIMEOUT_MSECS;
	ctx->listener_thread = NULL;
	memset(ctx->handlers, 0, sizeof(ctx->handlers));
	memset(ctx->handler_cb_data, 0, sizeof(ctx->handler_cb_data));

	return ctx;

}

void srvcom_set_serv_addr(struct srvcom_ctx *ctx,
	const char *ip, int port) {

	memset(&(ctx->serv_addr), 0, sizeof(ctx->serv_addr));
	ctx->serv_addr.sin_family = PF_INET;
	ctx->serv_addr.sin_port = htons(port);
	ctx->serv_addr.sin_addr.s_addr = htonl(str2ip(ip));

	return;

}

void srvcom_set_timeout(struct srvcom_ctx *ctx, long msecs) {

	ctx->msec_timeout = msecs;

	return;

}

void srvcom_register_handler(struct srvcom_ctx *ctx,
	srvcom_opcode_t opcode, srvcom_handler_t handler, void *cb_data) {

	unsigned opcode_index = (unsigned)opcode.code;

	ctx->handlers[opcode_index] = handler;
	ctx->handler_cb_data[opcode_index] = cb_data;

	return;

}

int srvcom_run(struct srvcom_ctx *ctx) {

	spin_lock_init(&ctx->listener_inject_lock);
	spin_unlock(&ctx->listener_inject_lock);

	ctx->write_try_count = 0;

	ctx->listener_thread =
		kthread_run(srvcom_listener_thread, ctx, "Listener thread");
	if ( !ctx->listener_thread )
		return -1;

	return 0;

}

/*
 * Request the server for permission to write to a page
 * containing address addr
 */
int srvcom_request_write(struct srvcom_ctx *ctx, unsigned long addr,
	pid_t pid, pgd_t *pgd) {

	int try_count;
	struct srvcom_msg msg;

	/*
	 * Many unnecessary page faults for the same page will
	 * generate requests before the handler for ALLOW_WRITE
	 * is triggered. Limit these requests to once per every
	 * TRIES_PER_REQUEST tries.
	 */
	try_count = ctx->write_try_count;
	if ( ++(ctx->write_try_count) >= TRIES_PER_REQUEST )
		ctx->write_try_count = 0;
	if ( try_count > 0 )
		return 0;

	msg.hdr.mcode = (srvcom_code_t)OPCODE_REQUEST_WRITE;
	msg.hdr.vaddr = addr;
	msg.hdr.pid = pid;
	msg.hdr.pgd = pgd;
	msg.hdr.payload_len = 0;

	if ( srvcom_listener_inject(ctx, &msg) < 0 ) {
		printk(KERN_INFO "srvcom_request_write: Injection failure");
		return -1;
	}

	return 0;

}

/*
 * Send the modified page to the server before
 * releasing page access
 *
 * XXX: *** WARNING *** :XXX
 *    Without the delivery guarantee of TCP this
 *    is *NOT* safe to use. Find a workaround as
 *    soon as possible.
 */
int srvcom_commit_page(struct srvcom_ctx *ctx, unsigned long addr,
	pid_t pid, pgd_t *pgd, char *pagedata) {

	struct srvcom_msg *msg;

	msg = (struct srvcom_msg*)kmalloc(
		sizeof(struct srvcom_msg) + PAGE_SIZE, GFP_KERNEL);
	if ( !msg ) {
		printk(KERN_INFO "srvcom_commit_page: Allocation failure");
		return -1;
	}

	msg->hdr.mcode = (srvcom_code_t)OPCODE_COMMIT_PAGE;
	msg->hdr.vaddr = addr;
	msg->hdr.pid = pid;
	msg->hdr.pgd = pgd;
	msg->hdr.payload_len = PAGE_SIZE;
	memcpy(msg->data.payload, pagedata, PAGE_SIZE);

	if ( srvcom_listener_inject(ctx, msg) < 0 ) {
		printk(KERN_INFO "srvcom_commit_page: Injection failure");
		kfree(msg);
		return -1;
	}

	kfree(msg);

	return 0;

}

#if 0
/*
 * TODO:
 *    The following commit implementation blocked until
 *    the server would respond and hence required a new
 *    connection to be established, which can severely
 *    fuck up the server's design. Currently everything
 *    is working with TCP so we still have some degree
 *    of delivery guarantee. Before moving to datagram
 *    however, a workaround must be implemented. Better
 *    if the event handlers are designed to receive and
 *    handle acknowledgement codes so that they are able
 *    to continue delivery in a kthread and destroy the
 *    kthread on an ACK receipt.
 */
int srvcom_commit_page(struct srvcom_ctx *ctx, unsigned long addr,
	pid_t pid, pgd_t *pgd, char *pagedata) {

	struct socket *sock;
	struct srvcom_msg *msg;

	msg = (struct srvcom_msg*)kmalloc(
		sizeof(struct srvcom_msg) + PAGE_SIZE, GFP_KERNEL);
	if ( !msg ) {
		printk(KERN_INFO "srvcom_commit_page: Allocation failure");
		return -1;
	}

	if ( !(sock = ksock_socket_create()) ) {
		printk(KERN_INFO "srvcom_commit_page: Failed to create socket");
		kfree(msg);
		return -1;
	}

	if ( ksock_connect(sock, (struct sockaddr*)&ctx->serv_addr, sizeof(ctx->serv_addr)) < 0 ) {
		printk(KERN_INFO "srvcom_commit_page: Failed to connect to server");
		ksock_socket_destroy(sock);
		kfree(msg);
		return -1;
	}

	msg->hdr.mcode = (srvcom_code_t)OPCODE_COMMIT_PAGE;
	msg->hdr.vaddr = addr;
	msg->hdr.pid = pid;
	msg->hdr.pgd = pgd;
	msg->hdr.payload_len = PAGE_SIZE;
	memcpy(msg->data.payload, pagedata, PAGE_SIZE);

	while ( 1 ) {

		int err_code;

		if ( srvcom_send(sock, msg) < 0 ) {
			printk(KERN_INFO "srvcom_commit_page: Send error");
			ksock_socket_destroy(sock);
			kfree(msg);
			return -1;
		}

		err_code = srvcom_timeout_recv(sock, msg, ctx->msec_timeout);
		if ( err_code < 0 ) {
			printk(KERN_INFO "srvcom_commit_page: Recv error");
			ksock_socket_destroy(sock);
			kfree(msg);
			return -1;
		} else if ( err_code > 0 ) {
			printk(KERN_INFO "srvcom_commit_page: Recv timed out");
			continue;
		}

		/* Should not happen but handle this case anyway */
		if (	/* Check if the reply has anything unexpected */
			(msg->hdr.mcode.ack.code != ACKCODE_COMMIT_PAGE.code)
			|| (msg->hdr.vaddr != addr)
			|| (msg->hdr.pid != pid)
			|| (msg->hdr.pgd != pgd)
			|| (msg->hdr.payload_len != 0)
		) {
			/* Reset the message data */
			msg->hdr.mcode = (srvcom_code_t)OPCODE_COMMIT_PAGE;
			msg->hdr.vaddr = addr;
			msg->hdr.pid = pid;
			msg->hdr.pgd = pgd;
			msg->hdr.payload_len = PAGE_SIZE;
			memcpy(msg->data.payload, pagedata, PAGE_SIZE);
			continue;
		}

		break;

	}

	ksock_socket_destroy(sock);
	kfree(msg);

	return 0;

}
#endif

void srvcom_exit(struct srvcom_ctx *ctx) {

	if ( !ctx )
		return;

	if ( ctx->listener_sock )
		ksock_socket_destroy(ctx->listener_sock);
	if ( ctx->listener_thread )
		kthread_stop(ctx->listener_thread);

	kfree(ctx);

	return;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* SRVCOM_C */



