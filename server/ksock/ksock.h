


/*
 * DESCRIPTION:
 *    Wrapper to simplify kernel tcp socket IO
 * NOTE:
 *    Accept will *NOT* block.
 */



#ifndef KSOCK_H
#define KSOCK_H



#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/sock.h>



#define KSOCK_MAX_SETSZ 16



struct socket;

struct ksock_set {
	int setsz;
	struct socket *sockset[KSOCK_MAX_SETSZ];
};



/* Sockets */
struct socket *ksock_socket_create(void);
int ksock_bind(struct socket *sock, struct sockaddr *addr, int addrlen);
int ksock_connect(struct socket *sock, struct sockaddr *serv_addr, int addrlen);
int ksock_send(struct socket *sock, char *buf, int len);
int ksock_recv(struct socket *sock, char *buf, int len);
int ksock_accept_ready(struct socket *listener_sock);
int ksock_recv_ready(struct socket *conn_sock);
struct socket *ksock_accept(struct socket *listener_sock,
	struct sockaddr *client_addr, int *addr_len);
int ksock_listen(struct socket *listener_sock, int conn_backlog);
int ksock_recv_timeout(struct socket *sock, char *buf, int len,
	unsigned long timeout_msecs);
void ksock_socket_destroy(struct socket *sock);
/* Select */
struct ksock_set *ksock_set_create(void);
void ksock_setcpy(struct ksock_set *dst, struct ksock_set *src);
int ksock_contains(struct ksock_set *set, struct socket *sock);
int ksock_insert(struct ksock_set *set, struct socket *sock);
void ksock_remove(struct ksock_set *set, struct socket *sock);
void ksock_clear(struct ksock_set *set);
int ksock_select(struct ksock_set *accept_set, struct ksock_set *recv_set,
	unsigned long timeout_msecs);
void ksock_set_destroy(struct ksock_set *set);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* KSOCK_H */



