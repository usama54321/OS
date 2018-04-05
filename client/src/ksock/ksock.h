


/*

	DESCRIPTION:
		Wrapper to simplify kernel tcp socket IO

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



struct socket;



int ksock_create(struct socket **sock);
int ksock_connect(struct socket *sock, struct sockaddr_in *serv_addr);
int ksock_send(struct socket *sock, char *buf, int len);
int ksock_recv(struct socket *sock, char *buf, int len);
int ksock_recv_timeout(struct socket *sock, char *buf, int len,
	unsigned long timeout_msecs);
void ksock_destroy(struct socket *sock);



MODULE_LICENSE("Dual BSD/GPL");



#endif /* KSOCK_H */



