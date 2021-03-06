


/*
 * DESCRIPTION:
 *    Wrapper to simplify kernel tcp socket IO
 */



#ifndef KSOCK_SOCKET_C
#define KSOCK_SOCKET_C



#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/sock.h>

#include "../ksock/ksock.h"



#define IO_FLAGS (MSG_DONTWAIT)



struct socket *ksock_socket_create(void) {

	struct socket *sock;

	if ( sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock) < 0 ) {
		printk(KERN_ERR "ksock_new: Failed to create socket");
		return NULL;
	}

	return sock;

}

int ksock_bind(struct socket *sock, struct sockaddr *addr, int addrlen) {

	if ( sock->ops->bind(sock, addr, addrlen) < 0 )
		return -1;

	return 0;

}

int ksock_connect(struct socket *sock, struct sockaddr *serv_addr, int addrlen) {

	int error_code;

	error_code = sock->ops->connect(sock,
		(struct sockaddr*)serv_addr, addrlen, O_RDWR);
	if ( error_code && (error_code != -EINPROGRESS) ) {
		printk(KERN_ERR "ksock_connect: Failed to connect");
		return -1;
	}

	return 0;

}

int ksock_send(struct socket *sock, char *buf, int len) {

	mm_segment_t oldmm;
	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = IO_FLAGS,
	};

	oldmm = get_fs();
	set_fs(KERNEL_DS);

	while ( len > 0 ) {

		int n_sent;
		struct kvec vec;

		vec.iov_len = len;
		vec.iov_base = buf;

		n_sent = kernel_sendmsg(sock, &msg, &vec, len, len);
		if ( n_sent == -ERESTARTSYS || n_sent == -EAGAIN )
			continue;
		if ( n_sent < 0 ) {
			set_fs(oldmm);
			return -1;
		}

		len -= n_sent;
		buf += n_sent;

	}

	set_fs(oldmm);

	return 0;

}

int ksock_recv(struct socket *sock, char *buf, int len) {

	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = IO_FLAGS,
	};

	while ( len > 0 ) {

		int n_recv;
		struct kvec vec;

		vec.iov_len = len;
		vec.iov_base = buf;

		n_recv = kernel_recvmsg(sock, &msg, &vec, len, len, IO_FLAGS);
		if ( n_recv == -ERESTARTSYS || n_recv == -EAGAIN )
			continue;
		if ( n_recv < 0 )
			return -1;

		len -= n_recv;
		buf += n_recv;

	}

	return 0;

}

int ksock_accept_ready(struct socket *listener_sock) {

	struct inet_connection_sock *icsock =
		inet_csk(listener_sock->sk);

	return reqsk_queue_empty(&icsock->icsk_accept_queue) ? 0 : 1;

}

int ksock_recv_ready(struct socket *conn_sock) {

	return skb_queue_empty(&conn_sock->sk->sk_receive_queue) ? 0 : 1;

}

/* Non-blocking */
struct socket *ksock_accept(struct socket *listener_sock,
	struct sockaddr *client_addr, int *addr_len) {

	int err_code;
	struct socket *conn_sock;
	struct inet_connection_sock *icsock;

	icsock = inet_csk(listener_sock->sk);
	if ( reqsk_queue_empty(&icsock->icsk_accept_queue) ) {
		/* No connection requests yet */
		return NULL;
	}

	err_code = sock_create(listener_sock->sk->sk_family, listener_sock->type,
		listener_sock->sk->sk_protocol, &conn_sock);
	if ( err_code < 0 || !conn_sock ) {
		printk(KERN_ERR "ksock_accept: Failed to create connection socket");
		return NULL;
	}

	conn_sock->type = listener_sock->type;
	conn_sock->ops = listener_sock->ops;

	err_code = listener_sock->ops->accept(listener_sock, conn_sock, O_NONBLOCK);
	if ( err_code < 0 ) {
		printk(KERN_ERR "ksock_accept: Failed to accept connection socket");
		return NULL;
	}

	err_code = conn_sock->ops->getname(conn_sock, client_addr, addr_len, 2);
	if ( err_code < 0 ) {
		printk(KERN_ERR "ksock_accept: Failed to get client name");
		return NULL;
	}

	return conn_sock;

}

int ksock_listen(struct socket *listener_sock, int conn_backlog) {

	int err_code =
		listener_sock->ops->listen(listener_sock, conn_backlog);

	if ( err_code < 0 ) {
		printk(KERN_ERR "ksock_listen: listen() failed");
		return -1;
	}

	return 0;

}

int ksock_recv_timeout(struct socket *sock, char *buf, int len,
	unsigned long timeout_msecs) {

	struct msghdr msg = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = IO_FLAGS,
	};
	DECLARE_WAIT_QUEUE_HEAD(wq);

	wait_event_timeout(wq, !skb_queue_empty(&sock->sk->sk_receive_queue),
		msecs_to_jiffies(timeout_msecs));

	if ( skb_queue_empty(&sock->sk->sk_receive_queue) )
		/* Timeout */
		return 1;

	while ( len > 0 ) {

		int n_recv;
		struct kvec vec;

		vec.iov_len = len;
		vec.iov_base = buf;

		n_recv = kernel_recvmsg(sock, &msg, &vec, len, len, IO_FLAGS);
		if ( n_recv == -ERESTARTSYS || n_recv == -EAGAIN )
			continue;
		if ( n_recv < 0 )
			return -1;

		len -= n_recv;
		buf += n_recv;

	}

	return 0;

}

void ksock_socket_destroy(struct socket *sock) {

	if ( sock != NULL )
		sock_release(sock);

	return;

}



MODULE_LICENSE("Dual BSD/GPL");



#endif /* KSOCK_SOCKET_C */



