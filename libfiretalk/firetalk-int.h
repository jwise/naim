/*
firetalk-int.h - FireTalk wrapper declarations
Copyright (C) 2000 Ian Gulliver

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef HAVE_INET_ATON
# define inet_aton(x,y)	inet_pton(AF_INET,x,y)
#endif

#define DEBUG

typedef struct s_firetalk_handle *firetalk_t;
#define _HAVE_FIRETALK_T

#ifndef _HAVE_CLIENT_T
#define _HAVE_CLIENT_T
typedef void *client_t;
#endif

#include "firetalk.h"

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif


typedef void (*ptrtofnct)(firetalk_t, void *, ...);

typedef struct {
	char	**keys;
	void	**data;
	int	count;
} firetalk_queue_t;

struct s_firetalk_buddy {
	struct s_firetalk_buddy *next;
	char	*nickname,
		*group,
		*friendly;
	long	idletime,
		warnval;
	unsigned char
		online:1,
		away:1;
	int	typing;
	char	*capabilities;
};

struct s_firetalk_deny {
	struct s_firetalk_deny *next;
	char *nickname;
};

struct s_firetalk_member {
	struct s_firetalk_member *next;
	char *nickname;
	unsigned char
		admin:1;
};

struct s_firetalk_room {
	struct s_firetalk_room *next;
	struct s_firetalk_member *member_head;
	char *name;
	unsigned char
		admin:1,
		sentjoin:1;
};

struct s_firetalk_file {
	struct s_firetalk_file *next;
	char *who;
	char *filename;
	struct in_addr inet_ip;
#ifdef _FC_USE_IPV6
	struct in6_addr inet6_ip;
	int tryinet6;
#endif
	uint16_t port;
	long size;
	long bytes;
	uint32_t acked;
#define FF_STATE_WAITLOCAL	0
#define FF_STATE_WAITREMOTE	1
#define FF_STATE_WAITSYNACK	2
#define FF_STATE_TRANSFERRING	3
	int state;
#define FF_DIRECTION_SENDING	0
#define FF_DIRECTION_RECEIVING	1
	int direction;
#define FF_TYPE_DCC		0
#define FF_TYPE_RAW		1
	int type;
	int sockfd;
	int filefd;
	void *clientfilestruct;
};

struct s_firetalk_subcode_callback {
	struct s_firetalk_subcode_callback *next;
	char *command, *staticresp;
	ptrtofnct callback;
};

typedef struct {
	const char *strprotocol;
	int	mustfork;
	fte_t	(*periodic)(firetalk_t conn);
	fte_t	(*preselect)(firetalk_t conn, client_t, fd_set *read, fd_set *write, fd_set *except, int *n);
	fte_t	(*postselect)(firetalk_t conn, client_t, fd_set *read, fd_set *write, fd_set *except);
	fte_t	(*comparenicks)(firetalk_t conn, const char *const, const char *const);
	fte_t	(*isprintable)(firetalk_t conn, const int);
	fte_t	(*disconnect)(firetalk_t conn, client_t);
	fte_t	(*connect)(firetalk_t conn, client_t, const char *server, uint16_t port, const char *const username);
	fte_t	(*sendpass)(firetalk_t conn, client_t, const char *const password);
	fte_t	(*save_config)(firetalk_t conn, client_t);
	fte_t	(*get_info)(firetalk_t conn, client_t, const char *const);
	fte_t	(*set_info)(firetalk_t conn, client_t, const char *const);
	fte_t	(*set_away)(firetalk_t conn, client_t, const char *const, const int);
	fte_t	(*set_nickname)(firetalk_t conn, client_t, const char *const);
	fte_t	(*set_password)(firetalk_t conn, client_t, const char *const, const char *const);
	fte_t	(*set_privacy)(firetalk_t conn, client_t, const char *const);
	fte_t	(*im_add_buddy)(firetalk_t conn, client_t, const char *const, const char *const, const char *const);
	fte_t	(*im_remove_buddy)(firetalk_t conn, client_t, const char *const, const char *const);
	fte_t	(*im_add_deny)(firetalk_t conn, client_t, const char *const);
	fte_t	(*im_remove_deny)(firetalk_t conn, client_t, const char *const);
	fte_t	(*im_upload_buddies)(firetalk_t conn, client_t);
	fte_t	(*im_upload_denies)(firetalk_t conn, client_t);
	fte_t	(*im_send_message)(firetalk_t conn, client_t, const char *const, const char *const, const int);
	fte_t	(*im_send_action)(firetalk_t conn, client_t, const char *const, const char *const, const int);
	fte_t	(*im_evil)(firetalk_t conn, client_t, const char *const);
	fte_t	(*chat_join)(firetalk_t conn, client_t, const char *const);
	fte_t	(*chat_part)(firetalk_t conn, client_t, const char *const);
	fte_t	(*chat_invite)(firetalk_t conn, client_t, const char *const, const char *const, const char *const);
	fte_t	(*chat_set_topic)(firetalk_t conn, client_t, const char *const, const char *const);
	fte_t	(*chat_op)(firetalk_t conn, client_t, const char *const, const char *const);
	fte_t	(*chat_deop)(firetalk_t conn, client_t, const char *const, const char *const);
	fte_t	(*chat_kick)(firetalk_t conn, client_t, const char *const, const char *const, const char *const);
	fte_t	(*chat_send_message)(firetalk_t conn, client_t, const char *const, const char *const, const int);
	fte_t	(*chat_send_action)(firetalk_t conn, client_t, const char *const, const char *const, const int);
	char	*(*subcode_encode)(firetalk_t conn, client_t, const char *const, const char *const);
	const char *(*room_normalize)(firetalk_t conn, const char *const);
	client_t (*create_handle)(firetalk_t conn);
	void	(*destroy_handle)(firetalk_t conn, client_t);
} firetalk_PD_t;

enum {
	FTRPC_FUNC_REPLY = 0,
	FTRPC_FUNC_COMPARENICKS,
	FTRPC_FUNC_ISPRINTABLE,
	FTRPC_FUNC_DISCONNECT,
	FTRPC_FUNC_CONNECT,
	FTRPC_FUNC_SENDPASS,
	FTRPC_FUNC_SAVE_CONFIG,
	FTRPC_FUNC_GET_INFO,
	FTRPC_FUNC_SET_INFO,
	FTRPC_FUNC_SET_AWAY,
	FTRPC_FUNC_SET_NICKNAME,
	FTRPC_FUNC_SET_PASSWORD,
	FTRPC_FUNC_SET_PRIVACY,
	FTRPC_FUNC_IM_ADD_BUDDY,
	FTRPC_FUNC_IM_REMOVE_BUDDY,
	FTRPC_FUNC_IM_ADD_DENY,
	FTRPC_FUNC_IM_REMOVE_DENY,
	FTRPC_FUNC_IM_UPLOAD_BUDDIES,
	FTRPC_FUNC_IM_UPLOAD_DENIES,
	FTRPC_FUNC_IM_SEND_MESSAGE,
	FTRPC_FUNC_IM_SEND_ACTION,
	FTRPC_FUNC_IM_EVIL,
	FTRPC_FUNC_CHAT_JOIN,
	FTRPC_FUNC_CHAT_PART,
	FTRPC_FUNC_CHAT_INVITE,
	FTRPC_FUNC_CHAT_SET_TOPIC,
	FTRPC_FUNC_CHAT_OP,
	FTRPC_FUNC_CHAT_DEOP,
	FTRPC_FUNC_CHAT_KICK,
	FTRPC_FUNC_CHAT_SEND_MESSAGE,
	FTRPC_FUNC_CHAT_SEND_ACTION,
	FTRPC_FUNC_SUBCODE_ENCODE,
	FTRPC_FUNC_ROOM_NORMALIZE,
	FTRPC_FUNC_DESTROY_HANDLE,
};

typedef struct {
	void	(*im_getmessage)(firetalk_t conn, client_t c, const char *const sender, const int automessage, const char *const message);
	void	(*im_getaction)(firetalk_t conn, client_t c, const char *const sender, const int automessage, const char *const message);
	void	(*im_buddyonline)(firetalk_t conn, client_t c, const char *const nickname, const int online);
	void	(*im_buddyaway)(firetalk_t conn, client_t c, const char *const nickname, const int away);
	void	(*buddyadded)(firetalk_t conn, client_t c, const char *const name, const char *const group, const char *const friendly);
	void	(*buddyremoved)(firetalk_t conn, client_t c, const char *const name, const char *const group);
	void	(*typing)(firetalk_t conn, client_t c, const char *const name, const int typing);
	void	(*capabilities)(firetalk_t conn, client_t c, char const *const nickname, const char *const caps);
	void	(*warninfo)(firetalk_t conn, client_t c, char const *const nickname, const long warnval);
	void	(*error)(firetalk_t conn, client_t c, const int error, const char *const roomoruser, const char *const description);
	void	(*connectfailed)(firetalk_t conn, client_t c, const int error, const char *const description);
	void	(*connected)(firetalk_t conn, client_t c);
	void	(*disconnect)(firetalk_t conn, client_t c, const int error);
	void	(*gotinfo)(firetalk_t conn, client_t c, const char *const nickname, const char *const info, const int warning, const long online, const long idle, const int flags);
	void	(*idleinfo)(firetalk_t conn, client_t c, char const *const nickname, const long idletime);
	void	(*doinit)(firetalk_t conn, client_t c, char const *const nickname);
	void	(*setidle)(firetalk_t conn, client_t c, long *const idle);
	void	(*eviled)(firetalk_t conn, client_t c, const int newevil, const char *const eviler);
	void	(*newnick)(firetalk_t conn, client_t c, const char *const nickname);
	void	(*passchanged)(firetalk_t conn, client_t c);
	void	(*user_nickchanged)(firetalk_t conn, client_t c, const char *const oldnick, const char *const newnick);
	void	(*chat_joined)(firetalk_t conn, client_t c, const char *const room);
	void	(*chat_left)(firetalk_t conn, client_t c, const char *const room);
	void	(*chat_kicked)(firetalk_t conn, client_t c, const char *const room, const char *const by, const char *const reason);
	void	(*chat_getmessage)(firetalk_t conn, client_t c, const char *const room, const char *const from, const int automessage, const char *const message);
	void	(*chat_getaction)(firetalk_t conn, client_t c, const char *const room, const char *const from, const int automessage, const char *const message);
	void	(*chat_invited)(firetalk_t conn, client_t c, const char *const room, const char *const from, const char *const message);
	void	(*chat_user_joined)(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const extra);
	void	(*chat_user_left)(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const reason);
	void	(*chat_user_quit)(firetalk_t conn, client_t c, const char *const who, const char *const reason);
	void	(*chat_gottopic)(firetalk_t conn, client_t c, const char *const room, const char *const topic, const char *const author);
#ifdef RAWIRCMODES
	void	(*chat_modechanged)(firetalk_t conn, client_t c, const char *const room, const char *const mode, const char *const by);
#endif
	void	(*chat_user_opped)(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by);
	void	(*chat_user_deopped)(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by);
	void	(*chat_keychanged)(firetalk_t conn, client_t c, const char *const room, const char *const what, const char *const by);
	void	(*chat_opped)(firetalk_t conn, client_t c, const char *const room, const char *const by);
	void	(*chat_deopped)(firetalk_t conn, client_t c, const char *const room, const char *const by);
	void	(*chat_user_kicked)(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by, const char *const reason);
	void	(*subcode_request)(firetalk_t conn, client_t c, const char *const from, const char *const command, char *args);
	void	(*subcode_reply)(firetalk_t conn, client_t c, const char *const from, const char *const command, const char *const args);
	void	(*file_offer)(firetalk_t conn, client_t c, const char *const from, const char *const filename, const long size, const char *const ipstring, const char *const ip6string, const uint16_t port, const int type);
	void	(*needpass)(firetalk_t conn, client_t c);
} firetalk_PI_t;

enum {
	FTRPC_CALLBACK_REPLY = 0,
	FTRPC_CALLBACK_IM_GETMESSAGE,
	FTRPC_CALLBACK_IM_GETACTION,
	FTRPC_CALLBACK_IM_BUDDYONLINE,
	FTRPC_CALLBACK_IM_BUDDYAWAY,
	FTRPC_CALLBACK_BUDDYADDED,
	FTRPC_CALLBACK_BUDDYREMOVED,
	FTRPC_CALLBACK_TYPING,
	FTRPC_CALLBACK_CAPABILITIES,
	FTRPC_CALLBACK_WARNINFO,
	FTRPC_CALLBACK_ERROR,
	FTRPC_CALLBACK_CONNECTFAILED,
	FTRPC_CALLBACK_CONNECTED,
	FTRPC_CALLBACK_DISCONNECT,
	FTRPC_CALLBACK_GOTINFO,
	FTRPC_CALLBACK_IDLEINFO,
	FTRPC_CALLBACK_DOINIT,
	FTRPC_CALLBACK_SETIDLE,
	FTRPC_CALLBACK_EVILED,
	FTRPC_CALLBACK_NEWNICK,
	FTRPC_CALLBACK_PASSCHANGED,
	FTRPC_CALLBACK_USER_NICKCHANGED,
	FTRPC_CALLBACK_CHAT_JOINED,
	FTRPC_CALLBACK_CHAT_LEFT,
	FTRPC_CALLBACK_CHAT_KICKED,
	FTRPC_CALLBACK_CHAT_GETMESSAGE,
	FTRPC_CALLBACK_CHAT_GETACTION,
	FTRPC_CALLBACK_CHAT_INVITED,
	FTRPC_CALLBACK_CHAT_USER_JOINED,
	FTRPC_CALLBACK_CHAT_USER_LEFT,
	FTRPC_CALLBACK_CHAT_USER_QUIT,
	FTRPC_CALLBACK_CHAT_GOTTOPIC,
#ifdef RAWIRCMODES
	FTRPC_CALLBACK_CHAT_MODECHANGED,
#endif
	FTRPC_CALLBACK_CHAT_USER_OPPED,
	FTRPC_CALLBACK_CHAT_USER_DEOPPED,
	FTRPC_CALLBACK_CHAT_KEYCHANGED,
	FTRPC_CALLBACK_CHAT_OPPED,
	FTRPC_CALLBACK_CHAT_DEOPPED,
	FTRPC_CALLBACK_CHAT_USER_KICKED,
	FTRPC_CALLBACK_SUBCODE_REQUEST,
	FTRPC_CALLBACK_SUBCODE_REPLY,
	FTRPC_CALLBACK_FILE_OFFER,
	FTRPC_CALLBACK_NEEDPASS,
};

typedef enum {
	FCS_NOTCONNECTED,
	FCS_WAITING_SYNACK,
	FCS_SEND_SIGNON,
	FCS_WAITING_PASSWORD,
	FCS_GOT_PASSWORD,
	FCS_WAITING_SIGNON,
	FCS_ACTIVE
} firetalk_sock_state_t;

typedef struct {
	unsigned long canary;
	int	fd;
	firetalk_sock_state_t state;
	struct sockaddr_in remote_addr;
	struct in_addr local_addr;
#ifdef _FC_USE_IPV6
	struct sockaddr_in6 remote_addr6;
	struct in6_addr local_addr6;
#endif
	int	readdata;
	unsigned short bufferpos;
	unsigned char buffer[1024*8*2];
} firetalk_sock_t;

struct s_firetalk_handle {
	void	*handle, *clientstruct;
	int	connected;
	firetalk_sock_t rpcpeer;
	unsigned long localip;
	ptrtofnct UA[FC_MAX];
	const firetalk_PD_t *PD;
	const firetalk_PI_t *PI;
	struct s_firetalk_handle *next, *prev;
	struct s_firetalk_buddy *buddy_head;
	struct s_firetalk_deny *deny_head;
	struct s_firetalk_room *room_head;
	struct s_firetalk_file *file_head;
	struct s_firetalk_subcode_callback *subcode_request_head, *subcode_reply_head, *subcode_request_default, *subcode_reply_default;
	firetalk_queue_t subcode_requests, subcode_replies;
	unsigned char deleted:1;
};


fte_t	firetalk_register_protocol(const firetalk_PD_t *const proto);
const char *firetalk_subcode_get_request_reply(firetalk_t conn, client_t c, const char *const command);

void	firetalk_enqueue(firetalk_queue_t *queue, const char *const key, void *data);
const void *firetalk_queue_peek(firetalk_queue_t *queue, const char *const key);
void	*firetalk_dequeue(firetalk_queue_t *queue, const char *const key);
void	firetalk_queue_append(char *buf, int buflen, firetalk_queue_t *queue, const char *const key);

fte_t	firetalk_chat_internal_add_room(firetalk_t conn, const char *const name);
fte_t	firetalk_chat_internal_add_member(firetalk_t conn, const char *const room, const char *const nickname);
fte_t	firetalk_chat_internal_remove_room(firetalk_t conn, const char *const name);
fte_t	firetalk_chat_internal_remove_member(firetalk_t conn, const char *const room, const char *const nickname);

struct s_firetalk_room *firetalk_find_room(struct s_firetalk_handle *c, const char *const room);
fte_t	firetalk_user_visible(firetalk_t conn, const char *const nickname);
fte_t	firetalk_user_visible_but(firetalk_t conn, const char *const room, const char *const nickname);

void	firetalk_handle_send(struct s_firetalk_handle *c, struct s_firetalk_file *filestruct);
void	firetalk_handle_receive(struct s_firetalk_handle *c, struct s_firetalk_file *filestruct);

fte_t	firetalk_sock_connect_host(firetalk_sock_t *sock, const char *const host, const uint16_t port);
fte_t	firetalk_sock_connect(firetalk_sock_t *sock);
fte_t	firetalk_sock_resolve4(const char *const host, struct in_addr *inet4_ip);
struct sockaddr_in *firetalk_sock_remotehost4(firetalk_sock_t *sock);
#ifdef _FC_USE_IPV6
fte_t	firetalk_sock_resolve6(const char *const host, struct in6_addr *inet6_ip);
struct sockaddr_in6 *firetalk_sock_remotehost6(firetalk_sock_t *sock);
#endif

void	firetalk_sock_init(firetalk_sock_t *sock);
void	firetalk_sock_close(firetalk_sock_t *sock);
fte_t	firetalk_sock_send(firetalk_sock_t *sock, const char *const buffer, const int bufferlen);
void	firetalk_sock_preselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except, int *n);
fte_t	firetalk_sock_postselect(firetalk_sock_t *sock, fd_set *my_read, fd_set *my_write, fd_set *my_except);

const char *firetalk_debase64(const char *const str);
const char *firetalk_htmlentities(const char *str);

#endif
