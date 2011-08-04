/* firetalk.h - FireTalk wrapper declarations
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
#ifndef _FIRETALK_H
#define _FIRETALK_H

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>

#ifndef __CYGWIN__
# define _FC_USE_IPV6
#endif

/* enums */
enum firetalk_callback {
	FC_CONNECTED,		/* ... */
	FC_CONNECTFAILED,	/* ..., int error, char *reason */
	FC_DOINIT,		/* ..., char *nickname */
	FC_ERROR,		/* ..., int error, char *roomoruser (room or user that error applies to, null if none) */
	FC_DISCONNECT,		/* ..., int error */
	FC_SETIDLE,		/* ..., long *idle */
	FC_EVILED,		/* ..., int newevil, char *eviler */
	FC_NEWNICK,		/* ..., char *nickname */
	FC_PASSCHANGED,		/* ... */
	FC_NEEDPASS,		/* ..., char *pass, int size */
	FC_PRESELECT,		/* ... */
	FC_POSTSELECT,		/* ... */
	FC_IM_IDLEINFO,		/* ..., char *nickname, long idletime */
	FC_IM_STATUSINFO,	/* ..., char *nickname, char *message */
	FC_IM_EVILINFO,		/* ..., char *nickname, long warnval */
	FC_IM_BUDDYADDED,	/* ..., char *nickname, char *group, char *friendly */
	FC_IM_BUDDYREMOVED,	/* ..., char *nickname */
	FC_IM_DENYADDED,	/* ..., char *nickname */
	FC_IM_DENYREMOVED,	/* ..., char *nickname */
	FC_IM_TYPINGINFO,	/* ..., char *nickname, int typing */
	FC_IM_CAPABILITIES,	/* ..., char *nickname, char *caps */
	FC_IM_GOTINFO,		/* ..., char *nickname, char *info, int warning, int online, int idle, int flags */
	FC_IM_USER_NICKCHANGED,	/* ..., char *oldnick, char *newnick */
	FC_IM_GETMESSAGE,	/* ..., char *sender, int automessage_flag, char *message */
	FC_IM_GETACTION,	/* ..., char *sender, int automessage_flag, char *message */
	FC_IM_BUDDYONLINE,	/* ..., char *nickname */
	FC_IM_BUDDYOFFLINE,	/* ..., char *nickname */
	FC_IM_BUDDYFLAGS,	/* ..., char *nickname, int flags */
	FC_IM_BUDDYAWAY,	/* ..., char *nickname */
	FC_IM_BUDDYUNAWAY,	/* ..., char *nickname */
	/* FC_IM_LISTBUDDY? */
	FC_CHAT_JOINED,		/* ..., char *room */
	FC_CHAT_SYNCHED,	/* ..., char *room */
	FC_CHAT_LEFT,		/* ..., char *room */
	FC_CHAT_KICKED,		/* ..., char *room, char *by, char *reason */
	FC_CHAT_GETMESSAGE,	/* ..., char *room, char *from, int automessage_flag, char *message */
	FC_CHAT_GETACTION,	/* ..., char *room, char *from, int automessage_flag, char *message */
	FC_CHAT_INVITED,	/* ..., char *room, char *from, char *message */
	FC_CHAT_MODESET,	/* ..., char *room, char *by, int mode, char *arg */
	FC_CHAT_MODEUNSET,	/* ..., char *room, char *by, int mode, char *arg */
	FC_CHAT_OPPED,		/* ..., char *room, char *by */
	FC_CHAT_DEOPPED,	/* ..., char *room, char *by */
	FC_CHAT_USER_JOINED,	/* ..., char *room, char *who */
	FC_CHAT_USER_LEFT,	/* ..., char *room, char *who, char *reason */
	FC_CHAT_GOTTOPIC,	/* ..., char *room, char *topic, char *author */
	FC_CHAT_USER_OPPED,	/* ..., char *room, char *who, char *by */
	FC_CHAT_USER_DEOPPED,	/* ..., char *room, char *who, char *by */
	FC_CHAT_USER_KICKED,	/* ..., char *room, char *who, char *by, char *reason */
	FC_CHAT_USER_NICKCHANGED, /* ..., char *room, char *oldnick, char *newnick */
	FC_CHAT_KEYCHANGED,	/* ..., char *room, char *what, char *by */
	FC_CHAT_LISTMEMBER,	/* ..., char *room, char *membername, int opped */
	FC_FILE_OFFER,		/* ..., void *filehandle, char *from, char *filename, long size */
	FC_FILE_START,		/* ..., void *filehandle, void *clientfilestruct */
	FC_FILE_PROGRESS,	/* ..., void *filehandle, void *clientfilestruct, long bytes, long size */
	FC_FILE_FINISH,		/* ..., void *filehandle, void *clientfilestruct, long size */
	FC_FILE_ERROR,		/* ..., void *filehandle, void *clientfilestruct, int error */
	FC_MAX,			/* tracking enum; don't hook this */
};

typedef enum {
#define ERROR_EXPANDO(x,s) FE_##x,
#include "firetalk-errors.h"
#undef ERROR_EXPANDO
} fte_t;


struct firetalk_connection_t;
struct firetalk_transfer_t;
struct firetalk_useragent_connection_t;
struct firetalk_useragent_transfer_t;

/* Firetalk functions */
int	firetalk_find_protocol(const char *strprotocol);
const char *firetalk_strprotocol(const int p);
const char *firetalk_strerror(const fte_t	e);
struct firetalk_connection_t *firetalk_create_conn(const int protocol, struct firetalk_useragent_connection_t *clientstruct);
void	firetalk_destroy_conn(struct firetalk_connection_t *conn);
int	firetalk_get_protocol(struct firetalk_connection_t *conn);

fte_t	firetalk_disconnect(struct firetalk_connection_t *conn);
fte_t	firetalk_signon(struct firetalk_connection_t *conn, const char *const server, const uint16_t port, const char *const username);
fte_t	firetalk_register_callback(struct firetalk_connection_t *conn, const int type, void (*function)(struct firetalk_connection_t *, struct firetalk_useragent_connection_t *, ...));
struct firetalk_connection_t *firetalk_find_clientstruct(struct firetalk_useragent_connection_t *clientstruct);

fte_t	firetalk_im_add_buddy(struct firetalk_connection_t *conn, const char *const name, const char *const group, const char *const friendly);
fte_t	firetalk_im_remove_buddy(struct firetalk_connection_t *conn, const char *const name);
fte_t	firetalk_im_add_deny(struct firetalk_connection_t *conn, const char *const name);
fte_t	firetalk_im_remove_deny(struct firetalk_connection_t *conn, const char *const name);
fte_t	firetalk_im_send_message(struct firetalk_connection_t *conn, const char *const dest, const char *const message, const int auto_flag);
fte_t	firetalk_im_send_action(struct firetalk_connection_t *conn, const char *const dest, const char *const message, const int auto_flag);
fte_t	firetalk_im_evil(struct firetalk_connection_t *c, const char *const who);
fte_t	firetalk_im_get_info(struct firetalk_connection_t *conn, const char *const nickname);

fte_t	firetalk_chat_join(struct firetalk_connection_t *conn, const char *const room);
fte_t	firetalk_chat_part(struct firetalk_connection_t *conn, const char *const room);
fte_t	firetalk_chat_send_message(struct firetalk_connection_t *conn, const char *const room, const char *const message, const int auto_flag);
fte_t	firetalk_chat_send_action(struct firetalk_connection_t *conn, const char *const room, const char *const message, const int auto_flag);
fte_t	firetalk_chat_invite(struct firetalk_connection_t *conn, const char *const room, const char *const who, const char *const message);
fte_t	firetalk_chat_set_topic(struct firetalk_connection_t *conn, const char *const room, const char *const topic);
fte_t	firetalk_chat_op(struct firetalk_connection_t *conn, const char *const room, const char *const who);
fte_t	firetalk_chat_deop(struct firetalk_connection_t *conn, const char *const room, const char *const who);
fte_t	firetalk_chat_kick(struct firetalk_connection_t *conn, const char *const room, const char *const who, const char *const reason);
fte_t	firetalk_chat_listmembers(struct firetalk_connection_t *conn, const char *const room);

fte_t	firetalk_subcode_send_reply(struct firetalk_connection_t *conn, const char *const to, const char *const command, const char *const args);
fte_t	firetalk_subcode_send_request(struct firetalk_connection_t *conn, const char *const to, const char *const command, const char *const args);

fte_t	firetalk_subcode_register_request_callback(struct firetalk_connection_t *conn, const char *const command, void (*callback)(struct firetalk_connection_t *, struct firetalk_useragent_connection_t *, const char *const, const char *const, const char *const));
fte_t	firetalk_subcode_register_request_reply(struct firetalk_connection_t *conn, const char *const command, const char *const reply);
fte_t	firetalk_subcode_register_reply_callback(struct firetalk_connection_t *conn, const char *const command, void (*callback)(struct firetalk_connection_t *, struct firetalk_useragent_connection_t *, const char *const, const char *const, const char *const));

fte_t	firetalk_file_offer(struct firetalk_connection_t *conn, const char *const nickname, const char *const filename, struct firetalk_useragent_transfer_t *clientfilestruct);
fte_t	firetalk_file_accept(struct firetalk_connection_t *conn, struct firetalk_transfer_t *filehandle, struct firetalk_useragent_transfer_t *clientfilestruct, const char *const localfile);
fte_t	firetalk_file_refuse(struct firetalk_connection_t *conn, struct firetalk_transfer_t *filehandle);
fte_t	firetalk_file_cancel(struct firetalk_connection_t *conn, struct firetalk_transfer_t *filehandle);

fte_t	firetalk_compare_nicks(struct firetalk_connection_t *conn, const char *const nick1, const char *const nick2);
fte_t	firetalk_isprint(struct firetalk_connection_t *conn, const int c);
fte_t	firetalk_set_info(struct firetalk_connection_t *conn, const char *const info);
fte_t	firetalk_set_away(struct firetalk_connection_t *c, const char *const message, const int auto_flag);
const char *firetalk_chat_normalize(struct firetalk_connection_t *conn, const char *const room);
fte_t	firetalk_set_nickname(struct firetalk_connection_t *conn, const char *const nickname);
fte_t	firetalk_set_password(struct firetalk_connection_t *conn, const char *const oldpass, const char *const newpass);
fte_t	firetalk_set_privacy(struct firetalk_connection_t *conn, const char *const mode);
fte_t	firetalk_select();
fte_t	firetalk_select_custom(int n, fd_set *fd_read, fd_set *fd_write, fd_set *fd_except, struct timeval *timeout);

extern fte_t firetalkerror;
int	firetalk_internal_connect_host(const char *const host, const int port);

typedef struct firetalk_md5_t {
	uint32_t d[4];
	unsigned int length;
	unsigned char buffer[64];
} firetalk_md5_t;

void	firetalk_md5_init(firetalk_md5_t *st);
void	firetalk_md5_update(firetalk_md5_t *st, const char *input, int inputlen);
unsigned char *firetalk_md5_final(firetalk_md5_t *st);

#define FF_SUBSTANDARD	0x0001
#define FF_NORMAL	0x0002
#define FF_ADMIN	0x0004
#define FF_MOBILE	0x0008

#ifndef MSG_DONTWAIT
# define MSG_DONTWAIT	0
#endif

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL	0
#endif

#endif
