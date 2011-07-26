/* Copyright 2006 Daniel Reed <n@ml.org>
*/
#include <ctype.h>
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

void	*firetalk_sock_t_magic = &firetalk_sock_t_magic,
	*firetalk_sock_t_canary = &firetalk_sock_t_canary,
	*firetalk_buffer_t_magic = &firetalk_buffer_t_magic,
	*firetalk_buffer_t_canary = &firetalk_buffer_t_canary;

fte_t	firetalk_sock_resolve4(const char *const host, struct in_addr *inet4_ip) {
	struct hostent *he;

	he = gethostbyname(host);
	if ((he != NULL) && (he->h_addr_list != NULL)) {
		memmove(&(inet4_ip->s_addr), he->h_addr_list[0], sizeof(inet4_ip->s_addr));
		return(FE_SUCCESS);
	}

	memset(&(inet4_ip->s_addr), 0, sizeof(inet4_ip->s_addr));

	return(FE_NOTFOUND);
}

struct sockaddr_in *firetalk_sock_remotehost4(firetalk_sock_t *sock) {
	assert(firetalk_sock_t_valid(sock));

	return(&(sock->remote_addr));
}

struct sockaddr_in *firetalk_sock_localhost4(firetalk_sock_t *sock) {
	assert(firetalk_sock_t_valid(sock));

	return(&(sock->local_addr));
}

#ifdef _FC_USE_IPV6
fte_t	firetalk_sock_resolve6(const char *const host, struct in6_addr *inet6_ip) {
	struct addrinfo *addr = NULL;  // xxx generalize this so that we can use this with v6 and v4
	struct addrinfo hints = { 0, PF_INET6, 0, 0, 0, NULL, NULL, NULL };

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
	assert(firetalk_sock_t_valid(sock));

	return(&(sock->remote_addr6));
}

struct sockaddr_in6 *firetalk_sock_localhost6(firetalk_sock_t *sock) {
	assert(firetalk_sock_t_valid(sock));

	return(&(sock->local_addr6));
}
#endif

fte_t	firetalk_sock_connect(firetalk_sock_t *sock) {
	struct sockaddr_in *inet4_ip = &(sock->remote_addr);
#ifdef _FC_USE_IPV6
	struct sockaddr_in6 *inet6_ip = &(sock->remote_addr6);
#endif

	assert(firetalk_sock_t_valid(sock));

	if (sock->fd != -1) {
		close(sock->fd);
		sock->fd = -1;
		sock->state = FCS_NOTCONNECTED;
	}
	assert(sock->state == FCS_NOTCONNECTED);
//	sock->bufferpos = 0;

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
			if ((connect(sock->fd, (const struct sockaddr *)inet6_ip, sizeof(*inet6_ip)) == 0) || (errno == EINPROGRESS)) {
				unsigned int l = sizeof(sock->local_addr6);

				getsockname(sock->fd, (struct sockaddr *)&sock->local_addr6, &l);
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
			if ((connect(sock->fd, (const struct sockaddr *)inet4_ip, sizeof(*inet4_ip)) == 0) || (errno == EINPROGRESS)) {
				unsigned int l = sizeof(sock->local_addr);

				getsockname(sock->fd, (struct sockaddr *)&sock->local_addr, &l);
				sock->state = FCS_WAITING_SYNACK;
				return(FE_SUCCESS);
			}
		}
	}

	assert(sock->fd == -1);
	assert(sock->state == FCS_NOTCONNECTED);
	return(FE_CONNECT);
}

fte_t	firetalk_sock_connect_host(firetalk_sock_t *sock, const char *const host, const uint16_t port) {
	assert(firetalk_sock_t_valid(sock));

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

fte_t	firetalk_sock_send(firetalk_sock_t *sock, const void *const buffer, const int bufferlen) {
	assert(firetalk_sock_t_valid(sock));
	assert(sock->state != FCS_NOTCONNECTED);

	if (sock->state == FCS_WAITING_SYNACK)
		return(FE_SUCCESS);

	if (send(sock->fd, buffer, bufferlen, /*MSG_DONTWAIT|*/MSG_NOSIGNAL) != bufferlen) {
		firetalk_sock_close(sock);
		return(FE_PACKET);
	}

	return(FE_SUCCESS);
}

void	firetalk_sock_preselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except, int *n) {
	assert(firetalk_sock_t_valid(sock));

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

void	firetalk_sock_close(firetalk_sock_t *sock) {
	firetalk_sock_t_dtor(sock);
	firetalk_sock_t_ctor(sock);
}

static fte_t firetalk_sock_synack(firetalk_sock_t *sock) {
	int	i;
	unsigned int o = sizeof(i);

	assert(firetalk_sock_t_valid(sock));

	if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &i, &o)) {
		firetalk_sock_close(sock);
		return(FE_SOCKET);
	}

	if (i != 0) {
		firetalk_sock_close(sock);
		return(FE_CONNECT);
	}

	sock->state = FCS_SEND_SIGNON;

	return(FE_SUCCESS);
}

static fte_t firetalk_sock_read(firetalk_sock_t *sock, firetalk_buffer_t *buffer) {
	int	length;

	assert(firetalk_sock_t_valid(sock));
	assert(firetalk_buffer_t_valid(buffer));

	/* read data into handle buffer */
	length = recv(sock->fd, &(buffer->buffer[buffer->pos]), buffer->size - buffer->pos, MSG_DONTWAIT);

	if (length < 1) {
		firetalk_sock_close(sock);
		return(FE_DISCONNECT);
	}

	buffer->pos += length;
	buffer->readdata = 1;

	return(FE_SUCCESS);
}

fte_t	firetalk_sock_postselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except, firetalk_buffer_t *buffer) {
	assert(firetalk_sock_t_valid(sock));
	assert(firetalk_buffer_t_valid(buffer));

	buffer->readdata = 0;

	if (sock->fd == -1)
		return(FE_SUCCESS);

	if (FD_ISSET(sock->fd, my_except)) {
		firetalk_sock_close(sock);
		return(FE_SOCKET);
	} else if (FD_ISSET(sock->fd, my_read))
		return(firetalk_sock_read(sock, buffer));
	else if (FD_ISSET(sock->fd, my_write))
		return(firetalk_sock_synack(sock));
	return(FE_SUCCESS);
}

int	firetalk_sock_t_valid(const firetalk_sock_t *sock) {
	if (sock->magic != &firetalk_sock_t_magic)
		return(0);
	if (sock->canary != &firetalk_sock_t_canary)
		return(0);
	if ((sock->fd == -1) && (sock->state != FCS_NOTCONNECTED))
		return(0);
	if ((sock->fd != -1) && (sock->state == FCS_NOTCONNECTED))
		return(0);
	return(1);
}

fte_t	firetalk_buffer_alloc(firetalk_buffer_t *buffer, uint32_t size) {
	void	*ptr;

	assert(firetalk_buffer_t_valid(buffer));

	if ((ptr = realloc(buffer->buffer, size)) == NULL)
		abort();
	buffer->buffer = ptr;
	buffer->size = size;
	if (buffer->pos > size)
		buffer->pos = size;
	return(FE_SUCCESS);
}

int	firetalk_buffer_t_valid(const firetalk_buffer_t *buffer) {
	int	i, c;

	if (buffer->magic != &firetalk_buffer_t_magic)
		return(0);
	if (buffer->canary != &firetalk_buffer_t_canary)
		return(0);
	if ((buffer->buffer == NULL) && (buffer->size != 0))
		return(0);
	if (buffer->pos > buffer->size)
		return(0);
	for (i = 0; i < buffer->pos; i++)
		c = buffer->buffer[i] + 1;
	return(1);
}

void	firetalk_enqueue(firetalk_queue_t *queue, const char *const key, void *data) {
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

			free(queue->keys[i]);
			queue->keys[i] = NULL;
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

#define STRNCMP(x,y)	(strncmp((x), (y), sizeof(y)-1))
char	*firetalk_htmlclean(const char *str) {
	static char buf[2048];
	int	i, b = 0;

	for (i = 0; (str[i] != 0) && (b < sizeof(buf)-1); i++)
		if (STRNCMP(str+i, "&gt;") == 0) {
			buf[b++] = '>';
			i += sizeof("&gt;")-2;
		} else if (STRNCMP(str+i, "&lt;") == 0) {
			buf[b++] = '<';
			i += sizeof("&lt;")-2;
		} else if (STRNCMP(str+i, "&quot;") == 0) {
			buf[b++] = '"';
			i += sizeof("&quot;")-2;
		} else if (STRNCMP(str+i, "&nbsp;") == 0) {
			buf[b++] = ' ';
			i += sizeof("&nbsp;")-2;
		} else if (STRNCMP(str+i, "&amp;") == 0) {
			buf[b++] = '&';
			i += sizeof("&amp;")-2;
		} else
			buf[b++] = str[i];
	buf[b] = 0;

	return(buf);
}
#undef STRNCMP

const char *firetalk_nhtmlentities(const char *str, int len) {
	static char buf[1024];
	int	i, b = 0;

	for (i = 0; (str[i] != 0) && (b < sizeof(buf)-6-1) && ((len < 0) || (i < len)); i++)
		switch (str[i]) {
		  case '<':
			buf[b++] = '&';
			buf[b++] = 'l';
			buf[b++] = 't';
			buf[b++] = ';';
			break;
		  case '>':
			buf[b++] = '&';
			buf[b++] = 'g';
			buf[b++] = 't';
			buf[b++] = ';';
			break;
		  case '&':
			buf[b++] = '&';
			buf[b++] = 'a';
			buf[b++] = 'm';
			buf[b++] = 'p';
			buf[b++] = ';';
			break;
		  case '"':
			buf[b++] = '&';
			buf[b++] = 'q';
			buf[b++] = 'u';
			buf[b++] = 'o';
			buf[b++] = 't';
			buf[b++] = ';';
			break;
		  case '\n':
			buf[b++] = '<';
			buf[b++] = 'b';
			buf[b++] = 'r';
			buf[b++] = '>';
			break;
		  default:
			buf[b++] = str[i];
			break;
		}
	buf[b] = 0;
	return(buf);
}

const char *firetalk_htmlentities(const char *str) {
	return(firetalk_nhtmlentities(str, -1));
}

static unsigned char firetalk_debase64_char(const char c) {
	if ((c >= 'A') && (c <= 'Z'))
		return((unsigned char)(c - 'A'));
	if ((c >= 'a') && (c <= 'z'))
		return((unsigned char)(26 + (c - 'a')));
	if ((c >= '0') && (c <= '9'))
		return((unsigned char)(52 + (c - '0')));
	if (c == '+')
		return((unsigned char)62);
	if (c == '/')
		return((unsigned char)63);
	return((unsigned char)0);
}

const char *firetalk_debase64(const char *const str) {
	static unsigned char out[256];
	int	s, o, len = strlen(str);

	for (o = s = 0; (s <= (len - 3)) && (o < (sizeof(out)-3)); s += 4, o += 3) {
		out[o]   = (firetalk_debase64_char(str[s])   << 2) | (firetalk_debase64_char(str[s+1]) >> 4);
		out[o+1] = (firetalk_debase64_char(str[s+1]) << 4) | (firetalk_debase64_char(str[s+2]) >> 2);
		out[o+2] = (firetalk_debase64_char(str[s+2]) << 6) |  firetalk_debase64_char(str[s+3]);
	}
	out[o] = 0;
	return((char *)out);
}

const char *firetalk_printable(const char *const str) {
	static unsigned char out[256];
	int	s, o;

	for (o = s = 0; (str[s] != 0) && (o < sizeof(out)-4); s++)
		if (isprint(str[s]) && (((s != 0) && isprint(str[s-1])) || ((str[s+1] != 0) && isprint(str[s+1]))))
			out[o++] = str[s];
		else {
			sprintf(out+o, "\\x%02X", str[s]);
			o += 4;
		}
	out[o] = 0;
	return(out);
}
