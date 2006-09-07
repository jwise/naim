/* firetalk-int.h - FireTalk wrapper declarations
** Copyright (C) 2000 Ian Gulliver
** Copyright 2002-2006 Daniel Reed <n@ml.org>
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of version 2 of the GNU General Public License as
** published by the Free Software Foundation.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _FIRETALK_INT_H
#define _FIRETALK_INT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef HAVE_INET_ATON
# define inet_aton(x,y)	inet_pton(AF_INET, (x), (y))
#endif

#include "firetalk.h"

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#define ZERO_CTOR(T) \
static inline void T ## _ctor(T *this) { \
	memset(this, 0, sizeof(*this)); \
}

#define TYPE_NEW(T) \
static inline T *T ## _new(void) { \
	T *this; \
	\
	if ((this = malloc(sizeof(*this))) == NULL) \
		return(NULL); \
	T ## _ctor(this); \
	return(this); \
}

#define TYPE_DELETE(T) \
static inline void T ## _delete(T *this) { \
	T ## _dtor(this); \
	free(this); \
}

#define LIST_DELETE(T) \
static inline void T ## _list_delete(T *head) { \
	if (head != NULL) { \
		T *next = head->next; \
		\
		T ## _delete(head); \
		T ## _list_delete(next); \
	} \
}

typedef void (*ptrtofnct)(struct firetalk_connection_t *, struct firetalk_useragent_connection_t *, ...);

typedef struct {
	char	**keys;
	void	**data;
	int	count;
} firetalk_queue_t;

ZERO_CTOR(firetalk_queue_t);
TYPE_NEW(firetalk_queue_t);
static inline void firetalk_queue_t_dtor(firetalk_queue_t *this) {
	int	i;

	assert(this != NULL);
	for (i = 0; i < this->count; i++) {
		free(this->keys[i]);
		this->keys[i] = NULL;
	}
	free(this->keys);
	free(this->data);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_queue_t);

typedef struct firetalk_buddy_t {
	struct firetalk_buddy_t *next;
	char	*nickname,
		*group,
		*friendly;
	long	idletime,
		warnval;
	int	typing;
	char	*capabilities;
	uint8_t	online:1,
		away:1,
		uploaded:1;
} firetalk_buddy_t;

ZERO_CTOR(firetalk_buddy_t);
TYPE_NEW(firetalk_buddy_t);
static inline void firetalk_buddy_t_dtor(firetalk_buddy_t *this) {
	assert(this != NULL);
	free(this->nickname);
	free(this->group);
	free(this->friendly);
	free(this->capabilities);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_buddy_t);
LIST_DELETE(firetalk_buddy_t);

typedef struct firetalk_deny_t {
	struct firetalk_deny_t *next;
	char	*nickname;
	uint8_t	uploaded:1;
} firetalk_deny_t;

ZERO_CTOR(firetalk_deny_t);
TYPE_NEW(firetalk_deny_t);
static inline void firetalk_deny_t_dtor(firetalk_deny_t *this) {
	assert(this != NULL);
	free(this->nickname);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_deny_t);
LIST_DELETE(firetalk_deny_t);

typedef struct firetalk_member_t {
	struct firetalk_member_t *next;
	char	*nickname;
	uint8_t	admin:1;
} firetalk_member_t;

ZERO_CTOR(firetalk_member_t);
TYPE_NEW(firetalk_member_t);
static inline void firetalk_member_t_dtor(firetalk_member_t *this) {
	assert(this != NULL);
	free(this->nickname);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_member_t);
LIST_DELETE(firetalk_member_t);

typedef struct firetalk_room_t {
	struct firetalk_room_t *next;
	firetalk_member_t *member_head;
	char	*name;
	uint8_t	admin:1;
} firetalk_room_t;

ZERO_CTOR(firetalk_room_t);
TYPE_NEW(firetalk_room_t);
static inline void firetalk_room_t_dtor(firetalk_room_t *this) {
	assert(this != NULL);
	free(this->name);
	firetalk_member_t_list_delete(this->member_head);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_room_t);
LIST_DELETE(firetalk_room_t);

typedef enum {
	FCS_NOTCONNECTED,
	FCS_WAITING_SYNACK,
	FCS_SEND_SIGNON,
	FCS_WAITING_PASSWORD,
	FCS_WAITING_SIGNON,
	FCS_ACTIVE,
} firetalk_sock_state_t;

typedef struct {
	void	*magic;
	int	fd;
	firetalk_sock_state_t state;
	struct sockaddr_in remote_addr,
		local_addr;
#ifdef _FC_USE_IPV6
	struct sockaddr_in6 remote_addr6,
		local_addr6;
#endif
	void	*canary;
} firetalk_sock_t;

static inline void firetalk_sock_t_ctor(firetalk_sock_t *this) {
	extern void *firetalk_sock_t_magic, *firetalk_sock_t_canary;

	memset(this, 0, sizeof(*this));
	this->magic = &firetalk_sock_t_magic;
	this->canary = &firetalk_sock_t_canary;
	this->fd = -1;
	this->state = FCS_NOTCONNECTED;
}
TYPE_NEW(firetalk_sock_t);
int	firetalk_sock_t_valid(const firetalk_sock_t *sock);
static inline void firetalk_sock_t_dtor(firetalk_sock_t *this) {
	assert(this != NULL);
	assert(firetalk_sock_t_valid(this));
	if (this->fd != -1)
		close(this->fd);
	memset(this, 0, sizeof(*this));
	this->fd = -1;
}
TYPE_DELETE(firetalk_sock_t);

typedef struct {
	void	*magic;
	uint16_t size, pos;
	uint8_t	*buffer,
		readdata:1;
	void	*canary;
} firetalk_buffer_t;

static inline void firetalk_buffer_t_ctor(firetalk_buffer_t *this) {
	extern void *firetalk_buffer_t_magic, *firetalk_buffer_t_canary;

	memset(this, 0, sizeof(*this));
	this->magic = &firetalk_buffer_t_magic;
	this->canary = &firetalk_buffer_t_canary;
}
TYPE_NEW(firetalk_buffer_t);
int	firetalk_buffer_t_valid(const firetalk_buffer_t *buffer);
static inline void firetalk_buffer_t_dtor(firetalk_buffer_t *this) {
	assert(this != NULL);
	assert(firetalk_buffer_t_valid(this));
	free(this->buffer);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_buffer_t);

typedef struct firetalk_transfer_t {
	struct firetalk_transfer_t *next;
	char	*who,
		*filename;
	struct in_addr inet_ip;
#ifdef _FC_USE_IPV6
	struct in6_addr inet6_ip;
	int	tryinet6;
#endif
	uint16_t port;
	long	size,
		bytes;
	uint32_t acked;
#define FF_STATE_WAITLOCAL	0
#define FF_STATE_WAITREMOTE	1
#define FF_STATE_WAITSYNACK	2
#define FF_STATE_TRANSFERRING	3
	int	state;
#define FF_DIRECTION_SENDING	0
#define FF_DIRECTION_RECEIVING	1
	int	direction;
#define FF_TYPE_DCC		0
#define FF_TYPE_RAW		1
	int	type,
//		sockfd,
		filefd;
	firetalk_sock_t sock;
	struct firetalk_useragent_transfer_t *clientfilestruct;
} firetalk_transfer_t;

static inline void firetalk_transfer_t_ctor(firetalk_transfer_t *this) {
	memset(this, 0, sizeof(*this));
	this->filefd = -1;
	firetalk_sock_t_ctor(&(this->sock));
}
TYPE_NEW(firetalk_transfer_t);
static inline void firetalk_transfer_t_dtor(firetalk_transfer_t *this) {
	assert(this != NULL);
	free(this->who);
	free(this->filename);
	if (this->filefd >= 0)
		close(this->filefd);
	firetalk_sock_t_dtor(&(this->sock));
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_transfer_t);
LIST_DELETE(firetalk_transfer_t);

typedef struct firetalk_subcode_callback_t {
	struct firetalk_subcode_callback_t *next;
	char	*command, *staticresp;
	ptrtofnct callback;
} firetalk_subcode_callback_t;

ZERO_CTOR(firetalk_subcode_callback_t);
TYPE_NEW(firetalk_subcode_callback_t);
static inline void firetalk_subcode_callback_t_dtor(firetalk_subcode_callback_t *this) {
	assert(this != NULL);
	free(this->command);
	free(this->staticresp);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_subcode_callback_t);
LIST_DELETE(firetalk_subcode_callback_t);

typedef struct firetalk_connection_t {
	void	*magic;
	struct firetalk_driver_connection_t *handle;
	struct firetalk_useragent_connection_t *clientstruct;
	firetalk_sock_t sock;
	firetalk_buffer_t buffer;
	uint32_t localip;
	int	protocol;
	char	*username;
	ptrtofnct callbacks[FC_MAX];
	struct firetalk_connection_t *next, *prev;
	firetalk_buddy_t *buddy_head;
	firetalk_deny_t *deny_head;
	firetalk_room_t *room_head;
	firetalk_transfer_t *file_head;
	firetalk_subcode_callback_t *subcode_request_head,
		*subcode_reply_head,
		*subcode_request_default,
		*subcode_reply_default;
	firetalk_queue_t subcode_requests,
		subcode_replies;
	uint8_t	deleted:1;
	void	*canary;
} firetalk_connection_t;

static inline void firetalk_connection_t_ctor(firetalk_connection_t *this) {
	extern void *firetalk_connection_t_magic, *firetalk_connection_t_canary;

	memset(this, 0, sizeof(*this));
	this->magic = &firetalk_connection_t_magic;
	this->canary = &firetalk_connection_t_canary;
	firetalk_sock_t_ctor(&(this->sock));
	firetalk_buffer_t_ctor(&(this->buffer));
	firetalk_queue_t_ctor(&(this->subcode_requests));
	firetalk_queue_t_ctor(&(this->subcode_replies));
}
TYPE_NEW(firetalk_connection_t);
int	firetalk_connection_t_valid(const firetalk_connection_t *this);
static inline void firetalk_connection_t_dtor(firetalk_connection_t *this) {
	assert(this != NULL);
	assert(firetalk_connection_t_valid(this));
	firetalk_sock_t_dtor(&(this->sock));
	firetalk_buffer_t_dtor(&(this->buffer));
	free(this->username);
	firetalk_buddy_t_list_delete(this->buddy_head);
	firetalk_deny_t_list_delete(this->deny_head);
	firetalk_room_t_list_delete(this->room_head);
	firetalk_transfer_t_list_delete(this->file_head);
	firetalk_subcode_callback_t_list_delete(this->subcode_request_head);
	firetalk_subcode_callback_t_list_delete(this->subcode_reply_head);
	firetalk_subcode_callback_t_delete(this->subcode_request_default);
	firetalk_subcode_callback_t_delete(this->subcode_reply_default);
	firetalk_queue_t_dtor(&(this->subcode_requests));
	firetalk_queue_t_dtor(&(this->subcode_replies));
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(firetalk_connection_t);



struct firetalk_driver_connection_t;

typedef struct {
	const char *const strprotocol;
	const char *const default_server;
	const uint16_t default_port;
	const uint16_t default_buffersize;
	fte_t	(*periodic)(firetalk_connection_t *const conn);
	fte_t	(*preselect)(struct firetalk_driver_connection_t *c, fd_set *read, fd_set *write, fd_set *except, int *n);
	fte_t	(*postselect)(struct firetalk_driver_connection_t *c, fd_set *read, fd_set *write, fd_set *except);
	fte_t	(*got_data)(struct firetalk_driver_connection_t *c, firetalk_buffer_t *buffer);
	fte_t	(*got_data_connecting)(struct firetalk_driver_connection_t *c, firetalk_buffer_t *buffer);
	fte_t	(*comparenicks)(const char *const, const char *const);
	fte_t	(*isprintable)(const int);
	fte_t	(*disconnect)(struct firetalk_driver_connection_t *c);
	fte_t	(*disconnected)(struct firetalk_driver_connection_t *c, const fte_t reason);
	fte_t	(*signon)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*get_info)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*set_info)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*set_away)(struct firetalk_driver_connection_t *c, const char *const, const int);
	fte_t	(*set_nickname)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*set_password)(struct firetalk_driver_connection_t *c, const char *const, const char *const);
	fte_t	(*set_privacy)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*im_add_buddy)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const char *const);
	fte_t	(*im_remove_buddy)(struct firetalk_driver_connection_t *c, const char *const, const char *const);
	fte_t	(*im_add_deny)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*im_remove_deny)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*im_send_message)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const int);
	fte_t	(*im_send_action)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const int);
	fte_t	(*im_evil)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*chat_join)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*chat_part)(struct firetalk_driver_connection_t *c, const char *const);
	fte_t	(*chat_invite)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const char *const);
	fte_t	(*chat_set_topic)(struct firetalk_driver_connection_t *c, const char *const, const char *const);
	fte_t	(*chat_op)(struct firetalk_driver_connection_t *c, const char *const, const char *const);
	fte_t	(*chat_deop)(struct firetalk_driver_connection_t *c, const char *const, const char *const);
	fte_t	(*chat_kick)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const char *const);
	fte_t	(*chat_send_message)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const int);
	fte_t	(*chat_send_action)(struct firetalk_driver_connection_t *c, const char *const, const char *const, const int);
	char	*(*subcode_encode)(struct firetalk_driver_connection_t *c, const char *const, const char *const);
	const char *(*room_normalize)(const char *const);
	struct firetalk_driver_connection_t *(*create_conn)(void);
	void	(*destroy_conn)(struct firetalk_driver_connection_t *c);
} firetalk_driver_t;



#define STRREPLACE(target, source) do { \
		if ((source) == NULL) { \
			free(target); \
			(target) = NULL; \
		} else { \
			assert((source) != (target)); \
			if (((target) = realloc((target), strlen(source)+1)) == NULL) \
				abort(); \
			strcpy((target), (source)); \
		} \
	} while (0)

#define FREESTR(x) do { \
		if ((x) != NULL) { \
			free(x); \
			(x) = NULL; \
		} \
	} while (0)



fte_t	firetalk_register_protocol(const firetalk_driver_t *const proto);

void	firetalk_callback_im_getmessage(struct firetalk_driver_connection_t *c, const char *const sender, const int automessage, const char *const message);
void	firetalk_callback_im_getaction(struct firetalk_driver_connection_t *c, const char *const sender, const int automessage, const char *const message);
void	firetalk_callback_im_buddyonline(struct firetalk_driver_connection_t *c, const char *const nickname, const int online);
void	firetalk_callback_im_buddyaway(struct firetalk_driver_connection_t *c, const char *const nickname, const int away);
void	firetalk_callback_buddyadded(struct firetalk_driver_connection_t *c, const char *const name, const char *const group, const char *const friendly);
void	firetalk_callback_buddyremoved(struct firetalk_driver_connection_t *c, const char *const name, const char *const group);
void	firetalk_callback_denyadded(struct firetalk_driver_connection_t *c, const char *const name);
void	firetalk_callback_denyremoved(struct firetalk_driver_connection_t *c, const char *const name);
void	firetalk_callback_typing(struct firetalk_driver_connection_t *c, const char *const name, const int typing);
void	firetalk_callback_capabilities(struct firetalk_driver_connection_t *c, char const *const nickname, const char *const caps);
void	firetalk_callback_warninfo(struct firetalk_driver_connection_t *c, char const *const nickname, const long warnval);
void	firetalk_callback_error(struct firetalk_driver_connection_t *c, const fte_t error, const char *const roomoruser, const char *const description);
void	firetalk_callback_connectfailed(struct firetalk_driver_connection_t *c, const fte_t error, const char *const description);
void	firetalk_callback_connected(struct firetalk_driver_connection_t *c);
void	firetalk_callback_disconnect(struct firetalk_driver_connection_t *c, const fte_t error);
void	firetalk_callback_gotinfo(struct firetalk_driver_connection_t *c, const char *const nickname, const char *const info, const int warning, const long online, const long idle, const int flags);
void	firetalk_callback_idleinfo(struct firetalk_driver_connection_t *c, char const *const nickname, const long idletime);
void	firetalk_callback_doinit(struct firetalk_driver_connection_t *c, char const *const nickname);
void	firetalk_callback_setidle(struct firetalk_driver_connection_t *c, long *const idle);
void	firetalk_callback_eviled(struct firetalk_driver_connection_t *c, const int newevil, const char *const eviler);
void	firetalk_callback_newnick(struct firetalk_driver_connection_t *c, const char *const nickname);
void	firetalk_callback_passchanged(struct firetalk_driver_connection_t *c);
void	firetalk_callback_user_nickchanged(struct firetalk_driver_connection_t *c, const char *const oldnick, const char *const newnick);
void	firetalk_callback_chat_joined(struct firetalk_driver_connection_t *c, const char *const room);
void	firetalk_callback_chat_left(struct firetalk_driver_connection_t *c, const char *const room);
void	firetalk_callback_chat_kicked(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, const char *const reason);
void	firetalk_callback_chat_getmessage(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const int automessage, const char *const message);
void	firetalk_callback_chat_getaction(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const int automessage, const char *const message);
void	firetalk_callback_chat_invited(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const char *const message);
void	firetalk_callback_chat_user_joined(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const extra);
void	firetalk_callback_chat_user_left(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const reason);
void	firetalk_callback_chat_user_quit(struct firetalk_driver_connection_t *c, const char *const who, const char *const reason);
void	firetalk_callback_chat_gottopic(struct firetalk_driver_connection_t *c, const char *const room, const char *const topic, const char *const author);
void	firetalk_callback_chat_modeset(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, int mode, const char *const arg);
void	firetalk_callback_chat_modeunset(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, int mode, const char *const arg);
void	firetalk_callback_chat_user_opped(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by);
void	firetalk_callback_chat_user_deopped(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by);
void	firetalk_callback_chat_keychanged(struct firetalk_driver_connection_t *c, const char *const room, const char *const what, const char *const by);
void	firetalk_callback_chat_opped(struct firetalk_driver_connection_t *c, const char *const room, const char *const by);
void	firetalk_callback_chat_deopped(struct firetalk_driver_connection_t *c, const char *const room, const char *const by);
void	firetalk_callback_chat_user_kicked(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by, const char *const reason);
const char *firetalk_subcode_get_request_reply(struct firetalk_driver_connection_t *c, const char *const command);
void	firetalk_callback_subcode_request(struct firetalk_driver_connection_t *c, const char *const from, const char *const command, char *args);
void	firetalk_callback_subcode_reply(struct firetalk_driver_connection_t *c, const char *const from, const char *const command, const char *const args);
void	firetalk_callback_file_offer(struct firetalk_driver_connection_t *c, const char *const from, const char *const filename, const long size, const char *const ipstring, const char *const ip6string, const uint16_t port, const int type);
void	firetalk_callback_needpass(struct firetalk_driver_connection_t *c, char *pass, const int size);

firetalk_buddy_t *firetalk_im_find_buddy(firetalk_connection_t *conn, const char *const name);

void	firetalk_enqueue(firetalk_queue_t *queue, const char *const key, void *data);
const void *firetalk_peek(firetalk_queue_t *queue, const char *const key);
void	*firetalk_dequeue(firetalk_queue_t *queue, const char *const key);
void	firetalk_queue_append(char *buf, int buflen, firetalk_queue_t *queue, const char *const key);

firetalk_connection_t *firetalk_find_conn(const struct firetalk_driver_connection_t *const c);

fte_t	firetalk_chat_internal_add_room(firetalk_connection_t *conn, const char *const name);
fte_t	firetalk_chat_internal_add_member(firetalk_connection_t *conn, const char *const room, const char *const nickname);
fte_t	firetalk_chat_internal_remove_room(firetalk_connection_t *conn, const char *const name);
fte_t	firetalk_chat_internal_remove_member(firetalk_connection_t *conn, const char *const room, const char *const nickname);

firetalk_room_t *firetalk_find_room(firetalk_connection_t *c, const char *const room);
fte_t	firetalk_user_visible(firetalk_connection_t *conn, const char *const nickname);
fte_t	firetalk_user_visible_but(firetalk_connection_t *conn, const char *const room, const char *const nickname);

void	firetalk_handle_send(firetalk_connection_t *c, firetalk_transfer_t *filestruct);
void	firetalk_handle_receive(firetalk_connection_t *c, firetalk_transfer_t *filestruct);

void	firetalk_internal_send_data(firetalk_connection_t *c, const char *const data, const int length);

int	firetalk_internal_connect_host(const char *const host, const int port);
int	firetalk_internal_connect(struct sockaddr_in *inet4_ip
#ifdef _FC_USE_IPV6
		, struct sockaddr_in6 *inet6_ip
#endif
		);
fte_t	firetalk_internal_resolve4(const char *const host, struct in_addr *inet4_ip);
struct sockaddr_in *firetalk_callback_remotehost4(struct firetalk_driver_connection_t *c);
#ifdef _FC_USE_IPV6
fte_t	firetalk_internal_resolve6(const char *const host, struct in6_addr *inet6_ip);
struct sockaddr_in6 *firetalk_callback_remotehost6(struct firetalk_driver_connection_t *c);
#endif
firetalk_sock_state_t firetalk_internal_get_connectstate(struct firetalk_driver_connection_t *c);
void	firetalk_internal_set_connectstate(struct firetalk_driver_connection_t *c, firetalk_sock_state_t fcs);

fte_t	firetalk_sock_connect_host(firetalk_sock_t *sock, const char *const host, const uint16_t port);
fte_t	firetalk_sock_connect(firetalk_sock_t *sock);
fte_t	firetalk_sock_resolve4(const char *const host, struct in_addr *inet4_ip);
struct sockaddr_in *firetalk_sock_remotehost4(firetalk_sock_t *sock);
struct sockaddr_in *firetalk_sock_localhost4(firetalk_sock_t *sock);
#ifdef _FC_USE_IPV6
fte_t	firetalk_sock_resolve6(const char *const host, struct in6_addr *inet6_ip);
struct sockaddr_in6 *firetalk_sock_remotehost6(firetalk_sock_t *sock);
struct sockaddr_in6 *firetalk_sock_localhost6(firetalk_sock_t *sock);
#endif

void	firetalk_sock_init(firetalk_sock_t *sock);
void	firetalk_sock_close(firetalk_sock_t *sock);
fte_t	firetalk_sock_send(firetalk_sock_t *sock, const void *const buffer, const int bufferlen);
void	firetalk_sock_preselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except, int *n);
fte_t	firetalk_sock_postselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except, firetalk_buffer_t *buffer);

fte_t	firetalk_buffer_alloc(firetalk_buffer_t *buffer, uint16_t size);

char	*firetalk_htmlclean(const char *str);
const char *firetalk_nhtmlentities(const char *str, int len);
const char *firetalk_htmlentities(const char *str);
const char *firetalk_debase64(const char *const str);
const char *firetalk_printable(const char *const str);

#endif
