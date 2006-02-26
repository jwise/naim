/* 
** Copyright 2006 Daniel Reed <n@ml.org>
*/
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#define FIRETALK

#include "firetalk-int.h"
#include "firetalk.h"

#define SOCK_CANARY	1234567890

fte_t	firetalk_sock_resolve4(const char *const host, struct in_addr *inet4_ip) {
	struct hostent *he;

	he = gethostbyname(host);
	if (he && he->h_addr_list) {
		memmove(&(inet4_ip->s_addr), he->h_addr_list[0], sizeof(inet4_ip->s_addr));
		return(FE_SUCCESS);
	}

	memset(&(inet4_ip->s_addr), 0, sizeof(inet4_ip->s_addr));

	return(FE_NOTFOUND);
}

struct sockaddr_in *firetalk_sock_remotehost4(firetalk_sock_t *sock) {
	assert(sock->canary == SOCK_CANARY);

	return(&(sock->remote_addr));
}

#ifdef _FC_USE_IPV6
fte_t	firetalk_sock_resolve6(const char *const host, struct in6_addr *inet6_ip) {
	struct addrinfo *addr = NULL;  // xxx generalize this so that we can use this with v6 and v4
	struct addrinfo hints = {0, PF_INET6, 0, 0, 0, NULL, NULL, NULL};

	if (getaddrinfo(host, NULL, &hints, &addr) == 0) {
		struct addrinfo *cur;

		for (cur = addr; cur != NULL; cur = cur->ai_next)
			if (cur->ai_family == PF_INET6) {
				memcpy(&(inet6_ip->s6_addr), ((struct sockaddr_in6 *)cur->ai_addr)->sin6_addr.s6_addr, 16);
				freeaddrinfo(addr);
				return(FE_SUCCESS);
			}
	}

	memset(&(inet6_ip->s6_addr), 0, sizeof(inet6_ip->s6_addr));

	if (addr != NULL)
		freeaddrinfo(addr);

	return(FE_NOTFOUND);
}

struct sockaddr_in6 *firetalk_sock_remotehost6(firetalk_sock_t *sock) {
	assert(sock->canary == SOCK_CANARY);

	return(&(sock->remote_addr6));
}
#endif

fte_t	firetalk_sock_connect_host(firetalk_sock_t *sock, const char *const host, const uint16_t port) {
	assert(sock->canary == SOCK_CANARY);

#ifdef _FC_USE_IPV6
	if (firetalk_sock_resolve6(host, &(sock->remote_addr6.sin6_addr)) == FE_SUCCESS) {
		sock->remote_addr6.sin6_port = htons(port);
		sock->remote_addr6.sin6_family = AF_INET6;
	} else
		memset(&(sock->remote_addr6), 0, sizeof(sock->remote_addr6));
#endif

	if (firetalk_sock_resolve4(host, &(sock->remote_addr.sin_addr)) == FE_SUCCESS) {
		sock->remote_addr.sin_port = htons(port);
		sock->remote_addr.sin_family = AF_INET;
	} else
		memset(&(sock->remote_addr), 0, sizeof(sock->remote_addr));

	return(firetalk_sock_connect(sock));
}

fte_t	firetalk_sock_connect(firetalk_sock_t *sock) {
	struct sockaddr_in *inet4_ip = &(sock->remote_addr);
#ifdef _FC_USE_IPV6
	struct sockaddr_in6 *inet6_ip = &(sock->remote_addr6);
#endif
	int	i;

	assert(sock->canary == SOCK_CANARY);

	if (sock->fd != -1) {
		close(sock->fd);
		sock->fd = -1;
		sock->state = FCS_NOTCONNECTED;
	}
	assert(sock->state == FCS_NOTCONNECTED);
	sock->bufferpos = 0;

#ifdef _FC_USE_IPV6
	if (inet6_ip && (inet6_ip->sin6_addr.s6_addr[0] || inet6_ip->sin6_addr.s6_addr[1]
		|| inet6_ip->sin6_addr.s6_addr[2] || inet6_ip->sin6_addr.s6_addr[3]
		|| inet6_ip->sin6_addr.s6_addr[4] || inet6_ip->sin6_addr.s6_addr[5]
		|| inet6_ip->sin6_addr.s6_addr[6] || inet6_ip->sin6_addr.s6_addr[7]
		|| inet6_ip->sin6_addr.s6_addr[8] || inet6_ip->sin6_addr.s6_addr[9]
		|| inet6_ip->sin6_addr.s6_addr[10] || inet6_ip->sin6_addr.s6_addr[11]
		|| inet6_ip->sin6_addr.s6_addr[12] || inet6_ip->sin6_addr.s6_addr[13]
		|| inet6_ip->sin6_addr.s6_addr[14] || inet6_ip->sin6_addr.s6_addr[15])) {
		h_errno = 0;
		sock->fd = socket(PF_INET6, SOCK_STREAM, 0);
		if ((sock->fd != -1) && (fcntl(sock->fd, F_SETFL, O_NONBLOCK) == 0)) {
			errno = 0;
			i = connect(sock->fd, (const struct sockaddr *)inet6_ip, sizeof(struct sockaddr_in6));
			if ((i == 0) || (errno == EINPROGRESS)) {
				sock->state = FCS_WAITING_SYNACK;
				return(FE_SUCCESS);
			}
		}
	}
#endif

	if (inet4_ip && inet4_ip->sin_addr.s_addr) {
		h_errno = 0;
		sock->fd = socket(PF_INET, SOCK_STREAM, 0);
		if ((sock->fd != -1) && (fcntl(sock->fd, F_SETFL, O_NONBLOCK) == 0)) {
			errno = 0;
			i = connect(sock->fd, (const struct sockaddr *)inet4_ip, sizeof(struct sockaddr_in));
			if ((i == 0) || (errno == EINPROGRESS)) {
				sock->state = FCS_WAITING_SYNACK;
				return(FE_SUCCESS);
			}
		}
	}

	assert(sock->fd == -1);
	assert(sock->state == FCS_NOTCONNECTED);
	return(FE_CONNECT);
}

fte_t	firetalk_sock_send(firetalk_sock_t *sock, const char *const buffer, const int bufferlen) {
	assert(sock->canary == SOCK_CANARY);
	assert(sock->state != FCS_NOTCONNECTED);

	if (sock->state == FCS_WAITING_SYNACK)
		return(FE_SUCCESS);

	if (send(sock->fd, buffer, bufferlen, MSG_DONTWAIT|MSG_NOSIGNAL) != bufferlen)
		return(FE_PACKET);

	return(FE_SUCCESS);
}

void	firetalk_sock_preselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except, int *n) {
	assert(sock->canary == SOCK_CANARY);
	assert((sock->fd == -1) || (sock->state != FCS_NOTCONNECTED));

	if (sock->fd == -1)
		return;

	if (sock->fd >= *n)
		*n = sock->fd + 1;
	FD_SET(sock->fd, my_except);
	if (sock->state == FCS_WAITING_SYNACK)
		FD_SET(sock->fd, my_write);
	else
		FD_SET(sock->fd, my_read);
}

void	firetalk_sock_init(firetalk_sock_t *sock) {
	memset(sock, 0, sizeof(*sock));
	sock->canary = SOCK_CANARY;
	sock->fd = -1;
	sock->state = FCS_NOTCONNECTED;
	sock->bufferpos = 0;
}

void	firetalk_sock_close(firetalk_sock_t *sock) {
	assert(sock->canary == SOCK_CANARY);
	assert((sock->fd == -1) || (sock->state != FCS_NOTCONNECTED));

	close(sock->fd);
	firetalk_sock_init(sock);
}

static fte_t firetalk_sock_synack(firetalk_sock_t *sock) {
	int	i;
	unsigned int o = sizeof(int);

	assert(sock->canary == SOCK_CANARY);

	if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &i, &o)) {
		firetalk_sock_close(sock);
//		if (conn->UA[FC_CONNECTFAILED])
//			conn->UA[FC_CONNECTFAILED](conn, conn->clientstruct, FE_SOCKET, strerror(errno));
		return(FE_SOCKET);
	}

	if (i != 0) {
		firetalk_sock_close(sock);
//		if (conn->UA[FC_CONNECTFAILED])
//			conn->UA[FC_CONNECTFAILED](conn, conn->clientstruct, FE_CONNECT, strerror(i));
		return(FE_CONNECT);
	}

	sock->state = FCS_SEND_SIGNON;
//	i = conn->PD->signon(conn, conn->handle, conn->username);
//	if (i != FE_SUCCESS)
//		return(i);

	return(FE_SUCCESS);
}

static fte_t firetalk_sock_read(firetalk_sock_t *sock) {
	short	length;

	assert(sock->canary == SOCK_CANARY);

	/* read data into handle buffer */
	length = recv(sock->fd, &(sock->buffer[sock->bufferpos]), sizeof(sock->buffer) - sock->bufferpos, MSG_DONTWAIT);

	if (length < 1)
		return(FE_DISCONNECT);
	sock->bufferpos += length;
	sock->readdata = 1;

	return(FE_SUCCESS);
}

fte_t	firetalk_sock_postselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except) {
	assert(sock->canary == SOCK_CANARY);
	assert((sock->fd == -1) || (sock->state != FCS_NOTCONNECTED));

	if (sock->fd == -1)
		return(FE_SUCCESS);

	sock->readdata = 0;

	if (FD_ISSET(sock->fd, my_except)) {
		firetalk_sock_close(sock);
		return(FE_SOCKET);
//		conn->PD->disconnect(conn, conn->handle);
	} else if (FD_ISSET(sock->fd, my_read))
		return(firetalk_sock_read(sock));
	else if (FD_ISSET(sock->fd, my_write))
		return(firetalk_sock_synack(sock));
	return(FE_SUCCESS);
}

void	firetalk_enqueue(firetalk_queue_t *queue, const char *const key, void *data) {
	return;

	queue->count++;
	queue->keys = realloc(queue->keys, (queue->count)*sizeof(*(queue->keys)));
	queue->data = realloc(queue->data, (queue->count)*sizeof(*(queue->data)));
	queue->keys[queue->count-1] = strdup(key);
	if (queue->keys[queue->count-1] == NULL)
		abort();
	queue->data[queue->count-1] = data;
}

const void *firetalk_queue_peek(firetalk_queue_t *queue, const char *const key) {
	int	i;

	assert(queue != NULL);
	assert(key != NULL);

	for (i = 0; i < queue->count; i++)
		if (strcmp(queue->keys[i], key) == 0)
			return(queue->data[i]);
	return(NULL);
}

void	*firetalk_dequeue(firetalk_queue_t *queue, const char *const key) {
	int	i;

	assert(queue != NULL);
	assert(key != NULL);

	for (i = 0; i < queue->count; i++)
		if (strcmp(queue->keys[i], key) == 0) {
			void	*data = queue->data[i];

			memmove(queue->keys+i, queue->keys+i+1, (queue->count-i-1)*sizeof(*(queue->keys)));
			memmove(queue->data+i, queue->data+i+1, (queue->count-i-1)*sizeof(*(queue->data)));
			queue->count--;
			queue->keys = realloc(queue->keys, (queue->count)*sizeof(*(queue->keys)));
			queue->data = realloc(queue->data, (queue->count)*sizeof(*(queue->data)));
			return(data);
		}
	return(NULL);
}

void	firetalk_queue_append(char *buf, int buflen, firetalk_queue_t *queue, const char *const key) {
	const char *data;

	while ((data = firetalk_queue_peek(queue, key)) != NULL) {
		if (strlen(buf)+strlen(data) >= buflen-1)
			break;
		strcat(buf, data);
		free(firetalk_dequeue(queue, key));
	}
}
