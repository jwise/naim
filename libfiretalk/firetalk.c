/* firetalk.c - FireTalk wrapper definitions
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
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "firetalk-int.h"
#include "firetalk.h"

/* Global variables */
fte_t	firetalkerror;
static firetalk_connection_t *handle_head = NULL;
static const firetalk_driver_t **firetalk_protocols = NULL;
static int FP_MAX = 0;

fte_t	firetalk_register_protocol(const firetalk_driver_t *const proto) {
	const firetalk_driver_t **ptr;

	if (proto == NULL)
		abort();

	ptr = realloc(firetalk_protocols, sizeof(*firetalk_protocols)*(FP_MAX+1));
	if (ptr == NULL)
		return(FE_UNKNOWN);
	firetalk_protocols = ptr;
	firetalk_protocols[FP_MAX++] = proto;
	return(FE_SUCCESS);
}

static void firetalk_register_default_protocols(void) {
	extern const firetalk_driver_t
		firetalk_protocol_irc,
		firetalk_protocol_slcp,
		firetalk_protocol_toc2;

	if (firetalk_register_protocol(&firetalk_protocol_irc) != FE_SUCCESS)
		abort();
	if (firetalk_register_protocol(&firetalk_protocol_slcp) != FE_SUCCESS)
		abort();
	if (firetalk_register_protocol(&firetalk_protocol_toc2) != FE_SUCCESS)
		abort();
}

int	firetalk_find_protocol(const char *strprotocol) {
	static int registered_defaults = 0;
	int	i;

	if (strprotocol == NULL)
		abort();

	for (i = 0; i < FP_MAX; i++)
		if (strcasecmp(strprotocol, firetalk_protocols[i]->strprotocol) == 0)
			return(i);
	if (!registered_defaults) {
		registered_defaults = 1;
		firetalk_register_default_protocols();
		for (i = 0; i < FP_MAX; i++)
			if (strcasecmp(strprotocol, firetalk_protocols[i]->strprotocol) == 0)
				return(i);
	}
	return(-1);
}


static int _connection_canary = 0;
#define CONNECTION_CANARY	(&_connection_canary)

#define DEBUG

#ifdef DEBUG
# define VERIFYCONN \
	do { \
		assert(handle_head != NULL); \
		if (firetalk_check_handle(conn) != FE_SUCCESS) \
			abort(); \
		assert(conn->canary == CONNECTION_CANARY); \
	} while(0)

static fte_t firetalk_check_handle(firetalk_connection_t *c) {
	firetalk_connection_t *iter;

	for (iter = handle_head; iter != NULL; iter = iter->next)
		if (iter == c)
			return(FE_SUCCESS);
	return(FE_BADHANDLE);
}
#else
# define VERIFYCONN \
	do { \
	} while(0)
#endif

/* firetalk_find_by_toc searches the firetalk handle list for the toc handle passed, and returns the firetalk handle */
firetalk_connection_t *firetalk_find_handle(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn;

	for (conn = handle_head; conn != NULL; conn = conn->next)
		if (conn->handle == c) {
			VERIFYCONN;
			return(conn);
		}
	abort();
}

firetalk_connection_t *firetalk_find_clientstruct(struct firetalk_useragent_connection_t *clientstruct) {
	firetalk_connection_t *conn;

	for (conn = handle_head; conn != NULL; conn = conn->next)
		if (conn->clientstruct == clientstruct) {
			VERIFYCONN;
			return(conn);
		}
	return(NULL);
}

static char **firetalk_parse_subcode_args(char *string) {
	static char *args[256];
	int	i, n;
	size_t	l;

	l = strlen(string);
	args[0] = string;
	n = 1;
	for (i = 0; ((size_t)i < l) && (n < 255); i++) {
		if (string[i] == ' ') {
			string[i++] = '\0';
			args[n++] = &string[i];
		}
	}
	args[n] = NULL;
	return(args);
}

firetalk_deny_t *firetalk_im_internal_add_deny(firetalk_connection_t *conn, const char *const nickname) {
	firetalk_deny_t *iter;

	VERIFYCONN;

	for (iter = conn->deny_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(iter->nickname, nickname) == FE_SUCCESS)
			break; /* not an error, user is in buddy list */

	if (iter == NULL) {
		iter = calloc(1, sizeof(*iter));
		if (iter == NULL)
			abort();
		iter->next = conn->deny_head;
		conn->deny_head = iter;
		iter->nickname = strdup(nickname);
		if (iter->nickname == NULL)
			abort();
	}

	firetalk_callback_im_buddyonline(conn->handle, nickname, 0);

	if (conn->callbacks[FC_IM_DENYADDED] != NULL)
		conn->callbacks[FC_IM_DENYADDED](conn, conn->clientstruct, iter->nickname);

	return(iter);
}

int	firetalk_internal_connect_host_addr(const char *const host, const uint16_t port, struct sockaddr_in *inet4
# ifdef _FC_USE_IPV6
		, struct sockaddr_in6 *inet6
# endif
) {
# ifdef _FC_USE_IPV6
	if (firetalk_sock_resolve6(host, &(inet6->sin6_addr)) == FE_SUCCESS) {
		inet6->sin6_port = htons(port);
		inet6->sin6_family = AF_INET6;
	} else
		inet6 = NULL;
# endif
	if (firetalk_sock_resolve4(host, &(inet4->sin_addr)) == FE_SUCCESS) {
		inet4->sin_port = htons(port);
		inet4->sin_family = AF_INET;
	} else
		inet4 = NULL;

	return(firetalk_internal_connect(inet4
# ifdef _FC_USE_IPV6
	   , inet6
# endif
	   ));
}

int	firetalk_internal_connect_host(const char *const host, const int port) {
	struct sockaddr_in myinet4;
# ifdef _FC_USE_IPV6
	struct sockaddr_in6 myinet6;
# endif

	return(firetalk_internal_connect_host_addr(host, port, &myinet4
# ifdef _FC_USE_IPV6
		, &myinet6
# endif
	));
}

int	firetalk_internal_connect(struct sockaddr_in *inet4_ip
# ifdef _FC_USE_IPV6
		, struct sockaddr_in6 *inet6_ip
# endif
		) {
	int	s, i;

# ifdef _FC_USE_IPV6
	if (inet6_ip && (inet6_ip->sin6_addr.s6_addr[0] || inet6_ip->sin6_addr.s6_addr[1]
		|| inet6_ip->sin6_addr.s6_addr[2] || inet6_ip->sin6_addr.s6_addr[3]
		|| inet6_ip->sin6_addr.s6_addr[4] || inet6_ip->sin6_addr.s6_addr[5]
		|| inet6_ip->sin6_addr.s6_addr[6] || inet6_ip->sin6_addr.s6_addr[7]
		|| inet6_ip->sin6_addr.s6_addr[8] || inet6_ip->sin6_addr.s6_addr[9]
		|| inet6_ip->sin6_addr.s6_addr[10] || inet6_ip->sin6_addr.s6_addr[11]
		|| inet6_ip->sin6_addr.s6_addr[12] || inet6_ip->sin6_addr.s6_addr[13]
		|| inet6_ip->sin6_addr.s6_addr[14] || inet6_ip->sin6_addr.s6_addr[15])) {
		h_errno = 0;
		s = socket(PF_INET6, SOCK_STREAM, 0);
		if ((s != -1) && (fcntl(s, F_SETFL, O_NONBLOCK) == 0)) {
			i = connect(s, (const struct sockaddr *)inet6_ip, sizeof(*inet6_ip));
			if ((i == 0) || (errno == EINPROGRESS))
				return(s);
		}
	}
# endif

	if (inet4_ip && inet4_ip->sin_addr.s_addr) {
		h_errno = 0;
		s = socket(PF_INET, SOCK_STREAM, 0);
		if ((s != -1) && (fcntl(s, F_SETFL, O_NONBLOCK) == 0)) {
			i = connect(s, (const struct sockaddr *)inet4_ip, sizeof(*inet4_ip));
			if ((i == 0) || (errno == EINPROGRESS))
				return(s);
		}
	}

	firetalkerror = FE_CONNECT;
	return(-1);
}

void	firetalk_internal_send_data(firetalk_connection_t *c, const char *const data, const int length) {
	if (firetalk_sock_send(&(c->sock), data, length) != FE_SUCCESS)
		/* disconnect client (we probably overran the queue, or the other end is gone) */
		firetalk_callback_disconnect(c->handle, FE_PACKET);
}

struct sockaddr_in *firetalk_callback_remotehost4(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	return(firetalk_sock_remotehost4(&(conn->sock)));
}

#ifdef _FC_USE_IPV6
struct sockaddr_in6 *firetalk_callback_remotehost6(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	return(firetalk_sock_remotehost6(&(conn->sock)));
}
#endif

firetalk_sock_state_t firetalk_internal_get_connectstate(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	return(conn->sock.state);
}

void	firetalk_internal_set_connectstate(struct firetalk_driver_connection_t *c, firetalk_sock_state_t fcs) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	conn->sock.state = fcs;
}

fte_t	firetalk_user_visible(firetalk_connection_t *conn, const char *const nickname) {
	firetalk_room_t *iter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		firetalk_member_t *mem;

		for (mem = iter->member_head; mem != NULL; mem = mem->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(mem->nickname, nickname) == FE_SUCCESS)
				return(FE_SUCCESS);
	}
	return(FE_NOMATCH);
}

fte_t	firetalk_user_visible_but(firetalk_connection_t *conn, const char *const room, const char *const nickname) {
	firetalk_room_t *iter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		firetalk_member_t *mem;

		if (firetalk_protocols[conn->protocol]->comparenicks(iter->name, room) == FE_SUCCESS)
			continue;
		for (mem = iter->member_head; mem != NULL; mem = mem->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(mem->nickname, nickname) == FE_SUCCESS)
				return(FE_SUCCESS);
	}
	return(FE_NOMATCH);
}

fte_t	firetalk_chat_internal_add_room(firetalk_connection_t *conn, const char *const name) {
	firetalk_room_t *iter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(iter->name, name) == FE_SUCCESS)
			return(FE_DUPEROOM); /* not an error, we're already in room */

	iter = calloc(1, sizeof(*iter));
	if (iter == NULL)
		abort();
	iter->next = conn->room_head;
	conn->room_head = iter;
	iter->name = strdup(name);
	if (iter->name == NULL)
		abort();

	return(FE_SUCCESS);
}

fte_t	firetalk_chat_internal_add_member(firetalk_connection_t *conn, const char *const room, const char *const nickname) {
	firetalk_room_t *iter;
	firetalk_member_t *memberiter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(iter->name, room) == FE_SUCCESS)
			break;

	if (iter == NULL) /* we don't know about that room */
		return(FE_NOTFOUND);

	for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(memberiter->nickname, nickname) == FE_SUCCESS)
			return(FE_SUCCESS);

	memberiter = calloc(1, sizeof(*memberiter));
	if (memberiter == NULL)
		abort();
	memberiter->next = iter->member_head;
	iter->member_head = memberiter;
	memberiter->nickname = strdup(nickname);
	if (memberiter->nickname == NULL)
		abort();

	return(FE_SUCCESS);
}

static void firetalk_im_delete_buddy(firetalk_connection_t *conn, const char *const nickname) {
	firetalk_buddy_t *iter, *prev;

	for (prev = NULL, iter = conn->buddy_head; iter != NULL; prev = iter, iter = iter->next) {
		assert(iter->nickname != NULL);
		assert(iter->group != NULL);

		if (firetalk_protocols[conn->protocol]->comparenicks(nickname, iter->nickname) == FE_SUCCESS)
			break;
	}
	if (iter == NULL)
		return;

	if (conn->callbacks[FC_IM_BUDDYREMOVED] != NULL)
		conn->callbacks[FC_IM_BUDDYREMOVED](conn, conn->clientstruct, iter->nickname);

	if (prev != NULL)
		prev->next = iter->next;
	else
		conn->buddy_head = iter->next;
	free(iter->nickname);
	iter->nickname = NULL;
	free(iter->group);
	iter->group = NULL;
	if (iter->friendly != NULL) {
		free(iter->friendly);
		iter->friendly = NULL;
	}
	if (iter->capabilities != NULL) {
		free(iter->capabilities);
		iter->capabilities = NULL;
	}
	free(iter);
	iter = NULL;
}

static firetalk_buddy_t *firetalk_im_find_buddy(firetalk_connection_t *conn, const char *const name) {
	firetalk_buddy_t *iter;

	for (iter = conn->buddy_head; iter != NULL; iter = iter->next) {
		assert(iter->nickname != NULL);
		assert(iter->group != NULL);

		if (firetalk_protocols[conn->protocol]->comparenicks(iter->nickname, name) == FE_SUCCESS)
			return(iter);
	}
	return(NULL);
}

fte_t	firetalk_im_remove_buddy(firetalk_connection_t *conn, const char *const name) {
	firetalk_buddy_t *iter;

	VERIFYCONN;

	if ((iter = firetalk_im_find_buddy(conn, name)) == NULL)
		return(FE_NOTFOUND);

	if (conn->sock.state != FCS_NOTCONNECTED) {
		fte_t	ret;

		ret = firetalk_protocols[conn->protocol]->im_remove_buddy(conn->handle, iter->nickname, iter->group);
		if (ret != FE_SUCCESS)
			return(ret);
	}

	firetalk_im_delete_buddy(conn, name);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_internal_remove_deny(firetalk_connection_t *conn, const char *const nickname) {
	firetalk_deny_t *iter, *prev;

	VERIFYCONN;

	prev = NULL;
	for (iter = conn->deny_head; iter != NULL; iter = iter->next) {
		if (firetalk_protocols[conn->protocol]->comparenicks(nickname, iter->nickname) == FE_SUCCESS) {
			if (conn->callbacks[FC_IM_DENYREMOVED] != NULL)
				conn->callbacks[FC_IM_DENYREMOVED](conn, conn->clientstruct, iter->nickname);

			if (prev)
				prev->next = iter->next;
			else
				conn->deny_head = iter->next;
			free(iter->nickname);
			iter->nickname = NULL;
			free(iter);

			return(FE_SUCCESS);
		}
		prev = iter;
	}

	return(FE_NOTFOUND);
}

fte_t	firetalk_chat_internal_remove_room(firetalk_connection_t *conn, const char *const name) {
	firetalk_room_t *iter, *prev;
	firetalk_member_t *memberiter, *membernext;

	VERIFYCONN;

	prev = NULL;
	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		if (firetalk_protocols[conn->protocol]->comparenicks(name, iter->name) == FE_SUCCESS) {
			for (memberiter = iter->member_head; memberiter != NULL; memberiter = membernext) {
				membernext = memberiter->next;
				free(memberiter->nickname);
				memberiter->nickname = NULL;
				free(memberiter);
			}
			iter->member_head = NULL;
			if (prev)
				prev->next = iter->next;
			else
				conn->room_head = iter->next;
			if (iter->name) {
				free(iter->name);
				iter->name = NULL;
			}
			free(iter);
			return(FE_SUCCESS);
		}
		prev = iter;
	}

	return(FE_NOTFOUND);
}

fte_t	firetalk_chat_internal_remove_member(firetalk_connection_t *conn, const char *const room, const char *const nickname) {
	firetalk_room_t *iter;
	firetalk_member_t *memberiter, *memberprev;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(iter->name, room) == FE_SUCCESS)
			break;

	if (iter == NULL) /* we don't know about that room */
		return(FE_NOTFOUND);

	memberprev = NULL;
	for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiter->next) {
		if (firetalk_protocols[conn->protocol]->comparenicks(memberiter->nickname,nickname) == FE_SUCCESS) {
			if (memberprev)
				memberprev->next = memberiter->next;
			else
				iter->member_head = memberiter->next;
			if (memberiter->nickname) {
				free(memberiter->nickname);
				memberiter->nickname = NULL;
			}
			free(memberiter);
			return(FE_SUCCESS);
		}
		memberprev = memberiter;
	}

	return(FE_SUCCESS);
}

firetalk_room_t *firetalk_find_room(firetalk_connection_t *c, const char *const room) {
	firetalk_room_t *roomiter;
	const char *normalroom;

	normalroom = firetalk_protocols[c->protocol]->room_normalize(room);
	for (roomiter = c->room_head; roomiter != NULL; roomiter = roomiter->next)
		if (firetalk_protocols[c->protocol]->comparenicks(roomiter->name, normalroom) == FE_SUCCESS)
			return(roomiter);

	firetalkerror = FE_NOTFOUND;
	return(NULL);
}

static firetalk_member_t *firetalk_find_member(firetalk_connection_t *c, firetalk_room_t *r, const char *const name) {
	firetalk_member_t *memberiter;

	for (memberiter = r->member_head; memberiter != NULL; memberiter = memberiter->next)
		if (firetalk_protocols[c->protocol]->comparenicks(memberiter->nickname, name) == FE_SUCCESS)
			return(memberiter);

	firetalkerror = FE_NOTFOUND;
	return(NULL);
}

void	firetalk_callback_needpass(struct firetalk_driver_connection_t *c, char *pass, const int size) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_NEEDPASS])
		conn->callbacks[FC_NEEDPASS](conn, conn->clientstruct, pass, size);
}

static const char *isonline_hack = NULL;

void	firetalk_callback_im_getmessage(struct firetalk_driver_connection_t *c, const char *const sender, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_deny_t *iter;

	if (strstr(message, "<a href=\"http://www.candidclicks.com/cgi-bin/enter.cgi?") != NULL) {
		firetalk_im_evil(conn, sender);
		return;
	}
	if (conn->callbacks[FC_IM_GETMESSAGE]) {
		for (iter = conn->deny_head; iter != NULL; iter = iter->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(sender, iter->nickname) == FE_SUCCESS)
				return;
		isonline_hack = sender;
		conn->callbacks[FC_IM_GETMESSAGE](conn, conn->clientstruct, sender, automessage, message);
		isonline_hack = NULL;
	}
}

void	firetalk_callback_im_getaction(struct firetalk_driver_connection_t *c, const char *const sender, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_deny_t *iter;

	if (conn->callbacks[FC_IM_GETACTION]) {
		for (iter = conn->deny_head; iter != NULL; iter = iter->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(sender, iter->nickname) == FE_SUCCESS)
				return;
		isonline_hack = sender;
		conn->callbacks[FC_IM_GETACTION](conn, conn->clientstruct, sender, automessage, message);
		isonline_hack = NULL;
	}
}

void	firetalk_callback_im_buddyonline(struct firetalk_driver_connection_t *c, const char *const nickname, int online) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;

	online = online?1:0;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if (buddyiter->online != online) {
			buddyiter->online = online;

			if (online != 0) {
				assert(buddyiter->away == 0);
				assert(buddyiter->typing == 0);
				assert(buddyiter->warnval == 0);
				assert(buddyiter->idletime == 0);
				if (strcmp(buddyiter->nickname, nickname) != 0) {
					free(buddyiter->nickname);
					buddyiter->nickname = strdup(nickname);
					if (buddyiter->nickname == NULL)
						abort();
				}
				if (conn->callbacks[FC_IM_BUDDYONLINE] != NULL)
					conn->callbacks[FC_IM_BUDDYONLINE](conn, conn->clientstruct, nickname);
			} else {
				buddyiter->away = 0;
				buddyiter->typing = 0;
				buddyiter->warnval = 0;
				buddyiter->idletime = 0;
				if (conn->callbacks[FC_IM_BUDDYOFFLINE] != NULL)
					conn->callbacks[FC_IM_BUDDYOFFLINE](conn, conn->clientstruct, nickname);
			}
		}
}

void	firetalk_callback_im_buddyaway(struct firetalk_driver_connection_t *c, const char *const nickname, const int away) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->away != away) && (buddyiter->online == 1)) {
			buddyiter->away = away;
			if ((away == 1) && (conn->callbacks[FC_IM_BUDDYAWAY] != NULL))
				conn->callbacks[FC_IM_BUDDYAWAY](conn, conn->clientstruct, nickname);
			else if ((away == 0) && (conn->callbacks[FC_IM_BUDDYUNAWAY] != NULL))
				conn->callbacks[FC_IM_BUDDYUNAWAY](conn, conn->clientstruct, nickname);
		}
}

static firetalk_buddy_t *firetalk_im_insert_buddy(firetalk_connection_t *conn, const char *const name, const char *const group, const char *const friendly) {
	firetalk_buddy_t *iter;

	iter = calloc(1, sizeof(*iter));
	if (iter == NULL)
		abort();
	iter->next = conn->buddy_head;
	conn->buddy_head = iter;
	iter->nickname = strdup(name);
	if (iter->nickname == NULL)
		abort();
	iter->group = strdup(group);
	if (iter->group == NULL)
		abort();
	if (friendly == NULL)
		iter->friendly = NULL;
	else {
		iter->friendly = strdup(friendly);
		if (iter->friendly == NULL)
			abort();
	}
	if (conn->callbacks[FC_IM_BUDDYADDED] != NULL)
		conn->callbacks[FC_IM_BUDDYADDED](conn, conn->clientstruct, iter->nickname, iter->group, iter->friendly);
	return(iter);
}

static void firetalk_im_replace_buddy(firetalk_connection_t *conn, firetalk_buddy_t *iter, const char *const name, const char *const group, const char *const friendly) {
	if (!((strcmp(iter->group, group) == 0) && (((iter->friendly == NULL) && (friendly == NULL)) || ((iter->friendly != NULL) && (friendly != NULL) && (strcmp(iter->friendly, friendly) == 0))))) {
		/* user is in buddy list somewhere other than where the clients wants it */
		if ((conn->sock.state != FCS_NOTCONNECTED) && iter->uploaded)
			firetalk_protocols[conn->protocol]->im_remove_buddy(conn->handle, iter->nickname, iter->group);
		free(iter->group);
		iter->group = strdup(group);
		if (iter->group == NULL)
			abort();
		free(iter->friendly);
		if (friendly == NULL)
			iter->friendly = NULL;
		else {
			iter->friendly = strdup(friendly);
			if (iter->friendly == NULL)
				abort();
		}
		if (conn->callbacks[FC_IM_BUDDYADDED] != NULL)
			conn->callbacks[FC_IM_BUDDYADDED](conn, conn->clientstruct, iter->nickname, iter->group, iter->friendly);
	}
}

void	firetalk_callback_buddyadded(struct firetalk_driver_connection_t *c, const char *const name, const char *const group, const char *const friendly) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *iter;

	if ((iter = firetalk_im_find_buddy(conn, name)) != NULL)
		firetalk_im_replace_buddy(conn, iter, name, group, friendly);
	else
		iter = firetalk_im_insert_buddy(conn, name, group, friendly);
	iter->uploaded = 1;
}

void	firetalk_callback_buddyremoved(struct firetalk_driver_connection_t *c, const char *const name, const char *const group) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *iter;

	if (((iter = firetalk_im_find_buddy(conn, name)) != NULL) && ((group == NULL) || (strcmp(iter->group, group) == 0)))
		firetalk_im_delete_buddy(conn, name);
}

void	firetalk_callback_denyadded(struct firetalk_driver_connection_t *c, const char *const name) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_deny_t *iter;

	iter = firetalk_im_internal_add_deny(conn, name);
	iter->uploaded = 1;
}

void	firetalk_callback_denyremoved(struct firetalk_driver_connection_t *c, const char *const name) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	firetalk_im_internal_remove_deny(conn, name);
}

void	firetalk_callback_typing(struct firetalk_driver_connection_t *c, const char *const name, const int typing) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;

	assert(conn->username != NULL);
	assert(name != NULL);
	assert(typing >= 0);

	if (((buddyiter = firetalk_im_find_buddy(conn, name)) == NULL) || (buddyiter->online == 0))
		return;
	if (buddyiter->typing != typing) {
		buddyiter->typing = typing;
		if (conn->callbacks[FC_IM_TYPINGINFO] != NULL)
			conn->callbacks[FC_IM_TYPINGINFO](conn, conn->clientstruct, buddyiter->nickname, typing);
	}
}

void	firetalk_callback_capabilities(struct firetalk_driver_connection_t *c, char const *const nickname, const char *const caps) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;

	if (((buddyiter = firetalk_im_find_buddy(conn, nickname)) == NULL) || (buddyiter->online == 0))
		return;
	if ((buddyiter->capabilities == NULL) || (strcmp(buddyiter->capabilities, caps) != 0)) {
		free(buddyiter->capabilities);
		buddyiter->capabilities = strdup(caps);
		if (conn->callbacks[FC_IM_CAPABILITIES] != NULL)
			conn->callbacks[FC_IM_CAPABILITIES](conn, conn->clientstruct, nickname, caps);
	}
}

void	firetalk_callback_warninfo(struct firetalk_driver_connection_t *c, char const *const nickname, const long warnval) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;

	if (((buddyiter = firetalk_im_find_buddy(conn, nickname)) == NULL) || (buddyiter->online == 0))
		return;
	if (buddyiter->warnval != warnval) {
		buddyiter->warnval = warnval;
		if (conn->callbacks[FC_IM_EVILINFO] != NULL)
			conn->callbacks[FC_IM_EVILINFO](conn, conn->clientstruct, nickname, warnval);
	}
}

void	firetalk_callback_error(struct firetalk_driver_connection_t *c, const fte_t error, const char *const roomoruser, const char *const description) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_ERROR])
		conn->callbacks[FC_ERROR](conn, conn->clientstruct, error, roomoruser, description);
}

void	firetalk_callback_connectfailed(struct firetalk_driver_connection_t *c, const fte_t error, const char *const description) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->sock.state == FCS_NOTCONNECTED)
		return;

	firetalk_sock_close(&(conn->sock));
	if (conn->callbacks[FC_CONNECTFAILED])
		conn->callbacks[FC_CONNECTFAILED](conn, conn->clientstruct, error, description);
}

void	firetalk_callback_disconnect(struct firetalk_driver_connection_t *c, const fte_t error) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;
	firetalk_deny_t *denyiter;
	firetalk_room_t *roomiter, *roomnext;

	if (conn->sock.state != FCS_NOTCONNECTED)
		firetalk_sock_close(&(conn->sock));

	if (conn->username != NULL) {
		free(conn->username);
		conn->username = NULL;
	}

	for (buddyiter = conn->buddy_head; buddyiter != NULL; buddyiter = buddyiter->next) {
		if (buddyiter->capabilities != NULL) {
			free(buddyiter->capabilities);
			buddyiter->capabilities = NULL;
		}
		buddyiter->idletime = buddyiter->warnval = buddyiter->typing = buddyiter->online = buddyiter->away = buddyiter->uploaded = 0;
	}

	for (denyiter = conn->deny_head; denyiter != NULL; denyiter = denyiter->next)
		denyiter->uploaded = 0;

	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomnext) {
		firetalk_member_t *memberiter, *membernext;

		roomnext = roomiter->next;
		roomiter->next = NULL;
		for (memberiter = roomiter->member_head; memberiter != NULL; memberiter = membernext) {
			membernext = memberiter->next;
			memberiter->next = NULL;
			free(memberiter->nickname);
			memberiter->nickname = NULL;
			free(memberiter);
		}
		roomiter->member_head = NULL;
		free(roomiter->name);
		roomiter->name = NULL;
		free(roomiter);
	}
	conn->room_head = NULL;

	if (conn->callbacks[FC_DISCONNECT])
		conn->callbacks[FC_DISCONNECT](conn, conn->clientstruct, error);
}

void	firetalk_callback_gotinfo(struct firetalk_driver_connection_t *c, const char *const nickname, const char *const info, const int warning, const long online, const long idle, const int flags) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_IM_GOTINFO])
		conn->callbacks[FC_IM_GOTINFO](conn, conn->clientstruct, nickname, info, warning, online, idle, flags);
}

void	firetalk_callback_idleinfo(struct firetalk_driver_connection_t *c, char const *const nickname, const long idletime) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;

	if (!conn->callbacks[FC_IM_IDLEINFO])
		return;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->idletime != idletime) && (buddyiter->online == 1)) {
			buddyiter->idletime = idletime;
			conn->callbacks[FC_IM_IDLEINFO](conn, conn->clientstruct, nickname, idletime);
		}
}

void	firetalk_callback_doinit(struct firetalk_driver_connection_t *c, const char *const nickname) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_DOINIT])
		conn->callbacks[FC_DOINIT](conn, conn->clientstruct, nickname);
}

void	firetalk_callback_setidle(struct firetalk_driver_connection_t *c, long *const idle) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_SETIDLE])
		conn->callbacks[FC_SETIDLE](conn, conn->clientstruct, idle);
}

void	firetalk_callback_eviled(struct firetalk_driver_connection_t *c, const int newevil, const char *const eviler) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_EVILED])
		conn->callbacks[FC_EVILED](conn, conn->clientstruct, newevil, eviler);
}

void	firetalk_callback_newnick(struct firetalk_driver_connection_t *c, const char *const nickname) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_NEWNICK])
		conn->callbacks[FC_NEWNICK](conn, conn->clientstruct, nickname);
}

void	firetalk_callback_passchanged(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_PASSCHANGED])
		conn->callbacks[FC_PASSCHANGED](conn, conn->clientstruct);
}

void	firetalk_callback_user_nickchanged(struct firetalk_driver_connection_t *c, const char *const oldnick, const char *const newnick) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_buddy_t *buddyiter;
	firetalk_room_t *roomiter;
	firetalk_member_t *memberiter;
	char *tempstr;

	if ((buddyiter = firetalk_im_find_buddy(conn, oldnick)) != NULL)
		if (strcmp(buddyiter->nickname, newnick) != 0) {
			tempstr = buddyiter->nickname;
			buddyiter->nickname = strdup(newnick);
			if (buddyiter->nickname == NULL)
				abort();
			if (conn->callbacks[FC_IM_USER_NICKCHANGED])
				conn->callbacks[FC_IM_USER_NICKCHANGED](conn, conn->clientstruct, tempstr, newnick);
			free(tempstr);
		}

	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		for (memberiter = roomiter->member_head; memberiter != NULL; memberiter = memberiter->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(memberiter->nickname, oldnick) == FE_SUCCESS) {
				if (strcmp(memberiter->nickname, newnick) != 0) {
					tempstr = memberiter->nickname;
					memberiter->nickname = strdup(newnick);
					if (memberiter->nickname == NULL)
						abort();
					if (conn->callbacks[FC_CHAT_USER_NICKCHANGED])
						conn->callbacks[FC_CHAT_USER_NICKCHANGED](conn, conn->clientstruct, roomiter->name, tempstr, newnick);
					free(tempstr);
				}
				break;
			}
}

void	firetalk_callback_chat_joined(struct firetalk_driver_connection_t *c, const char *const room) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (firetalk_chat_internal_add_room(conn, room) != FE_SUCCESS)
		return;
}

void	firetalk_callback_chat_left(struct firetalk_driver_connection_t *c, const char *const room) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (firetalk_chat_internal_remove_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_LEFT])
		conn->callbacks[FC_CHAT_LEFT](conn, conn->clientstruct, room);
}

void	firetalk_callback_chat_kicked(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (firetalk_chat_internal_remove_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_KICKED])
		conn->callbacks[FC_CHAT_KICKED](conn, conn->clientstruct, room, by, reason);
}

void	firetalk_callback_chat_getmessage(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_CHAT_GETMESSAGE])
		conn->callbacks[FC_CHAT_GETMESSAGE](conn, conn->clientstruct, room, from, automessage, message);
}

void	firetalk_callback_chat_getaction(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_CHAT_GETACTION])
		conn->callbacks[FC_CHAT_GETACTION](conn, conn->clientstruct, room, from, automessage, message);
}

void	firetalk_callback_chat_invited(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_CHAT_INVITED])
		conn->callbacks[FC_CHAT_INVITED](conn, conn->clientstruct, room, from, message);
}

void	firetalk_callback_chat_user_joined(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const extra) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *iter;

	iter = firetalk_find_room(conn, room);
	if (iter == NULL)
		return;

	if (who == NULL) {
		if (iter->sentjoin == 0) {
			iter->sentjoin = 1;
			if (conn->callbacks[FC_CHAT_JOINED])
				conn->callbacks[FC_CHAT_JOINED](conn, conn->clientstruct, room);
		}
	} else {
		if (firetalk_chat_internal_add_member(conn, room, who) != FE_SUCCESS)
			return;
		if (iter->sentjoin == 1)
			if (conn->callbacks[FC_CHAT_USER_JOINED])
				conn->callbacks[FC_CHAT_USER_JOINED](conn, conn->clientstruct, room, who, extra);
	}
}

void	firetalk_callback_chat_user_left(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (firetalk_chat_internal_remove_member(conn, room, who) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_USER_LEFT])
		conn->callbacks[FC_CHAT_USER_LEFT](conn, conn->clientstruct, room, who, reason);
}

void	firetalk_callback_chat_user_quit(struct firetalk_driver_connection_t *c, const char *const who, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *roomiter;
	firetalk_member_t *memberiter, *membernext;
	
	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		for (memberiter = roomiter->member_head; memberiter != NULL; memberiter = membernext) {
			membernext = memberiter->next;
			if (firetalk_protocols[conn->protocol]->comparenicks(memberiter->nickname, who) == FE_SUCCESS)
				firetalk_callback_chat_user_left(c, roomiter->name, who, reason);
		}
}

void	firetalk_callback_chat_gottopic(struct firetalk_driver_connection_t *c, const char *const room, const char *const topic, const char *const author) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *r;

	r = firetalk_find_room(conn, room);
	if (r != NULL)
		if (conn->callbacks[FC_CHAT_GOTTOPIC])
			conn->callbacks[FC_CHAT_GOTTOPIC](conn, conn->clientstruct, room, topic, author);
}

#ifdef RAWIRCMODES
void	firetalk_callback_chat_modechanged(struct firetalk_driver_connection_t *c, const char *const room, const char *const mode, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_CHAT_MODECHANGED])
		conn->callbacks[FC_CHAT_MODECHANGED](conn, conn->clientstruct, room, mode, by);
}
#endif

void	firetalk_callback_chat_user_opped(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *r;
	firetalk_member_t *m;

	r = firetalk_find_room(conn, room);
	if (r == NULL)
		return;
	m = firetalk_find_member(conn, r, who);
	if (m == NULL)
		return;
	if (m->admin == 0) {
		m->admin = 1;
		if (conn->callbacks[FC_CHAT_USER_OPPED])
			conn->callbacks[FC_CHAT_USER_OPPED](conn, conn->clientstruct, room, who, by);
	}
}

void	firetalk_callback_chat_user_deopped(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *r;
	firetalk_member_t *m;

	r = firetalk_find_room(conn, room);
	if (r == NULL)
		return;
	m = firetalk_find_member(conn, r, who);
	if (m == NULL)
		return;
	if (m->admin == 1) {
		m->admin = 0;
		if (conn->callbacks[FC_CHAT_USER_DEOPPED])
			conn->callbacks[FC_CHAT_USER_DEOPPED](conn, conn->clientstruct, room, who, by);
	}
}

void	firetalk_callback_chat_keychanged(struct firetalk_driver_connection_t *c, const char *const room, const char *const what, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (conn->callbacks[FC_CHAT_KEYCHANGED])
		conn->callbacks[FC_CHAT_KEYCHANGED](conn, conn->clientstruct, room, what, by);
}

void	firetalk_callback_chat_opped(struct firetalk_driver_connection_t *c, const char *const room, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *r;

	r = firetalk_find_room(conn,room);
	if (r == NULL)
		return;
	if (r->admin == 0)
		r->admin = 1;
	else
		return;
	if (conn->callbacks[FC_CHAT_OPPED])
		conn->callbacks[FC_CHAT_OPPED](conn, conn->clientstruct, room, by);
}

void	firetalk_callback_chat_deopped(struct firetalk_driver_connection_t *c, const char *const room, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_room_t *r;

	r = firetalk_find_room(conn,room);
	if (r == NULL)
		return;
	if (r->admin == 1)
		r->admin = 0;
	else
		return;
	if (conn->callbacks[FC_CHAT_DEOPPED])
		conn->callbacks[FC_CHAT_DEOPPED](conn, conn->clientstruct, room, by);
}

void	firetalk_callback_chat_user_kicked(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_handle(c);

	if (firetalk_chat_internal_remove_member(conn, room, who) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_USER_KICKED])
		conn->callbacks[FC_CHAT_USER_KICKED](conn, conn->clientstruct, room, who, by, reason);
}

const char *firetalk_subcode_get_request_reply(struct firetalk_driver_connection_t *c, const char *const command) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_subcode_callback_t *iter;

	for (iter = conn->subcode_request_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0)
			if (iter->staticresp != NULL)
				return(iter->staticresp);
	return(NULL);
}

void	firetalk_callback_subcode_request(struct firetalk_driver_connection_t *c, const char *const from, const char *const command, char *args) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_subcode_callback_t *iter;
	firetalk_sock_state_t connectedsave = conn->sock.state; /* nasty hack: some IRC servers send CTCP VERSION requests during signon, before 001, and demand a reply; idiots */

	conn->sock.state = FCS_ACTIVE;

	for (iter = conn->subcode_request_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0) {
			if (iter->staticresp != NULL)
				firetalk_subcode_send_reply(conn, from, command, iter->staticresp);
			else {
				isonline_hack = from;
				iter->callback(conn, conn->clientstruct, from, command, args);
				isonline_hack = NULL;
			}

			conn->sock.state = connectedsave;

			return;
		}

	if (strcmp(command, "ACTION") == 0)
		/* we don't support chatroom subcodes, so we're just going to assume that this is a private ACTION and let the protocol code handle the other case */
		firetalk_callback_im_getaction(c, from, 0, args);
	else if (strcmp(command, "VERSION") == 0)
		firetalk_subcode_send_reply(conn, from, "VERSION", PACKAGE_NAME ":" PACKAGE_VERSION ":unknown");
	else if (strcmp(command, "CLIENTINFO") == 0)
		firetalk_subcode_send_reply(conn, from, "CLIENTINFO", "CLIENTINFO PING SOURCE TIME VERSION");
	else if (strcmp(command, "PING") == 0) {
		if (args != NULL)
			firetalk_subcode_send_reply(conn, from, "PING", args);
		else
			firetalk_subcode_send_reply(conn, from, "PING", "");
	} else if (strcmp(command, "TIME") == 0) {
		time_t	temptime;
		char	tempbuf[64];
		size_t	s;

		time(&temptime);
		strncpy(tempbuf, ctime(&temptime), sizeof(tempbuf)-1);
		tempbuf[sizeof(tempbuf)-1] = 0;
		s = strlen(tempbuf);
		if (s > 0)
			tempbuf[s-1] = '\0';
		firetalk_subcode_send_reply(conn, from, "TIME", tempbuf);
	} else if ((strcmp(command,"DCC") == 0) && (args != NULL) && (strncasecmp(args, "SEND ", 5) == 0)) {
		/* DCC send */
		struct in_addr addr;
		uint32_t ip;
		long	size = -1;
		uint16_t port;
		char	**myargs;
#ifdef _FC_USE_IPV6
		struct in6_addr addr6;
		struct in6_addr *sendaddr6 = NULL;
#endif
		myargs = firetalk_parse_subcode_args(&args[5]);
		if ((myargs[0] != NULL) && (myargs[1] != NULL) && (myargs[2] != NULL)) {
			/* valid dcc send */
			if (myargs[3] != NULL) {
				size = atol(myargs[3]);
#ifdef _FC_USE_IPV6
				if (myargs[4] != NULL) {
					/* ipv6-enabled dcc */
					inet_pton(AF_INET6,myargs[4],&addr6);
					sendaddr6 = &addr6;
				}
#endif
			}
			sscanf(myargs[1], "%u", &ip);
			ip = htonl(ip);
			memcpy(&addr.s_addr, &ip, 4);
			port = (uint16_t)atoi(myargs[2]);
			firetalk_callback_file_offer(c, from, myargs[0], size, inet_ntoa(addr), NULL, port, FF_TYPE_DCC);
		}
	} else if (conn->subcode_request_default != NULL)
		conn->subcode_request_default->callback(conn, conn->clientstruct, from, command, args);

	conn->sock.state = connectedsave;
}

void	firetalk_callback_subcode_reply(struct firetalk_driver_connection_t *c, const char *const from, const char *const command, const char *const args) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_subcode_callback_t *iter;

	for (iter = conn->subcode_reply_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0) {
			isonline_hack = from;
			iter->callback(conn, conn->clientstruct, from, command, args);
			isonline_hack = NULL;
			return;
		}

	if (conn->subcode_reply_default != NULL)
		conn->subcode_reply_default->callback(conn, conn->clientstruct, from, command, args);
}

/* size may be -1 if unknown (0 is valid) */
void	firetalk_callback_file_offer(struct firetalk_driver_connection_t *c, const char *const from, const char *const filename, const long size, const char *const ipstring, const char *const ip6string, const uint16_t port, const int type) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	firetalk_transfer_t *iter;

	iter = calloc(1, sizeof(*iter));
	if (iter == NULL)
		abort();
	iter->next = conn->file_head;
	conn->file_head = iter;
	iter->who = strdup(from);
	if (iter->who == NULL)
		abort();
	iter->filename = strdup(filename);
	if (iter->filename == NULL)
		abort();
	iter->size = size;
	iter->state = FF_STATE_WAITLOCAL;
	iter->direction = FF_DIRECTION_RECEIVING;
	firetalk_sock_init(&(iter->sock));
	iter->filefd = -1;
	iter->port = htons(port);
	iter->type = type;
	iter->next = iter;
	if (inet_aton(ipstring, &iter->inet_ip) == 0) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return;
	}
#ifdef _FC_USE_IPV6
	if (ip6string)
		if (inet_pton(AF_INET6, ip6string, &iter->inet6_ip) != 0)
			iter->tryinet6 = 1;
#endif
	if (conn->callbacks[FC_FILE_OFFER])
		conn->callbacks[FC_FILE_OFFER](conn, conn->clientstruct, iter, from, filename, size);
}

void	firetalk_handle_receive(firetalk_connection_t *c, firetalk_transfer_t *filestruct) {
	static char buffer[4096];
	ssize_t	s;

	while ((s = recv(filestruct->sock.fd, buffer, 4096, MSG_DONTWAIT)) == 4096) {
		if (write(filestruct->filefd, buffer, 4096) != 4096) {
			if (c->callbacks[FC_FILE_ERROR])
				c->callbacks[FC_FILE_ERROR](c, c->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(c, filestruct);
			return;
		}
		filestruct->bytes += 4096;
	}
	if (s != -1) {
		if (write(filestruct->filefd, buffer, (size_t)s) != s) {
			if (c->callbacks[FC_FILE_ERROR])
				c->callbacks[FC_FILE_ERROR](c, c->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(c, filestruct);
			return;
		}
		filestruct->bytes += s;
	}
	if (filestruct->type == FF_TYPE_DCC) {
		uint32_t netbytes = htonl((uint32_t)filestruct->bytes);

		if (firetalk_sock_send(&(filestruct->sock), &netbytes, sizeof(netbytes)) != FE_SUCCESS) {
			if (c->callbacks[FC_FILE_ERROR])
				c->callbacks[FC_FILE_ERROR](c, c->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(c, filestruct);
			return;
		}
	}
	if (c->callbacks[FC_FILE_PROGRESS])
		c->callbacks[FC_FILE_PROGRESS](c, c->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
	if (filestruct->bytes == filestruct->size) {
		if (c->callbacks[FC_FILE_FINISH])
			c->callbacks[FC_FILE_FINISH](c, c->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->size);
		firetalk_file_cancel(c, filestruct);
	}
}

void	firetalk_handle_send(firetalk_connection_t *c, firetalk_transfer_t *filestruct) {
	static char buffer[4096];
	ssize_t	s;

	while ((s = read(filestruct->filefd, buffer, sizeof(buffer))) > 0) {
		if (firetalk_sock_send(&(filestruct->sock), buffer, s) != FE_SUCCESS) {
			if (c->callbacks[FC_FILE_ERROR])
				c->callbacks[FC_FILE_ERROR](c, c->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(c, filestruct);
			return;
		}
		filestruct->bytes += s;
		if (c->callbacks[FC_FILE_PROGRESS])
			c->callbacks[FC_FILE_PROGRESS](c, c->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
		if (filestruct->type == FF_TYPE_DCC) {
			uint32_t acked = 0;

			while (recv(filestruct->sock.fd, &acked, 4, MSG_DONTWAIT) == 4)
				filestruct->acked = ntohl(acked);
		}
	}
	if (filestruct->type == FF_TYPE_DCC) {
		while (filestruct->acked < (uint32_t)filestruct->bytes) {
			uint32_t acked = 0;

			if (recv(filestruct->sock.fd, &acked, 4, 0) == 4)
				filestruct->acked = ntohl(acked);
		}
	}
	if (c->callbacks[FC_FILE_PROGRESS])
		c->callbacks[FC_FILE_PROGRESS](c, c->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
	if (filestruct->bytes == filestruct->size)
		if (c->callbacks[FC_FILE_FINISH])
			c->callbacks[FC_FILE_FINISH](c, c->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes);
	firetalk_file_cancel(c, filestruct);
}

/* External function definitions */

const char *firetalk_strprotocol(const int p) {
	if ((p >= 0) && (p < FP_MAX))
		return(firetalk_protocols[p]->strprotocol);
	return(NULL);
}

const char *firetalk_strerror(const fte_t e) {
	switch (e) {
		case FE_SUCCESS:
			return("Success");
		case FE_CONNECT:
			return("Connection failed");
		case FE_NOMATCH:
			return("Usernames do not match");
		case FE_PACKET:
			return("Packet transfer error");
		case FE_RECONNECTING:
			return("Server wants us to reconnect elsewhere");
		case FE_BADUSERPASS:
			return("Invalid username or password");
		case FE_SEQUENCE:
			return("Invalid sequence number from server");
		case FE_FRAMETYPE:
			return("Invalid frame type from server");
		case FE_PACKETSIZE:
			return("Packet too long");
		case FE_SERVER:
			return("Server problem; try again later");
		case FE_UNKNOWN:
			return("Unknown error");
		case FE_BLOCKED:
			return("You are blocked");
		case FE_WEIRDPACKET:
			return("Unknown packet received from server");
		case FE_CALLBACKNUM:
			return("Invalid callback number");
		case FE_BADUSER:
			return("Invalid username");
		case FE_NOTFOUND:
			return("Username not found in list");
		case FE_DISCONNECT:
			return("Server disconnected");
		case FE_SOCKET:
			return("Unable to create socket");
		case FE_RESOLV:
			return("Unable to resolve hostname");
		case FE_VERSION:
			return("Wrong server version");
		case FE_USERUNAVAILABLE:
			return("User is currently unavailable");
		case FE_USERINFOUNAVAILABLE:
			return("User information is currently unavailable");
		case FE_TOOFAST:
			return("You are sending messages too fast; last message was dropped");
		case FE_ROOMUNAVAILABLE:
			return("Chat room is currently unavailable");
		case FE_INCOMINGERROR:
			return("Incoming message delivery failure");
		case FE_USERDISCONNECT:
			return("User disconnected");
		case FE_INVALIDFORMAT:
			return("Server response was formatted incorrectly");
		case FE_IDLEFAST:
			return("You have requested idle to be reset too fast");
		case FE_BADROOM:
			return("Invalid room name");
		case FE_BADMESSAGE:
			return("Invalid message (too long?)");
		case FE_MESSAGETRUNCATED:
			return("Message truncated");
		case FE_BADPROTO:
			return("Invalid protocol");
		case FE_NOTCONNECTED:
			return("Not connected");
		case FE_BADCONNECTION:
			return("Invalid connection number");
		case FE_NOPERMS:
			return("No permission to perform operation");
		case FE_NOCHANGEPASS:
			return("Unable to change password");
		case FE_DUPEROOM:
			return("Room already in list");
		case FE_IOERROR:
        		return("Input/output error");
		case FE_BADHANDLE:
        		return("Invalid handle");
		case FE_TIMEOUT:
			return("Operation timed out");
		default:
			return("Invalid error number");
	}
}

firetalk_connection_t *firetalk_create_handle(const int protocol, struct firetalk_useragent_connection_t *clientstruct) {
	firetalk_connection_t *c;

	if ((protocol < 0) || (protocol >= FP_MAX)) {
		firetalkerror = FE_BADPROTO;
		return(NULL);
	}
	c = calloc(1, sizeof(*c));
	if (c == NULL)
		abort();
	c->canary = CONNECTION_CANARY;
	c->next = handle_head;
	handle_head = c;
	c->clientstruct = clientstruct;
	c->protocol = protocol;
	firetalk_sock_init(&(c->sock));
	firetalk_buffer_init(&(c->buffer));
	firetalk_buffer_alloc(&(c->buffer), firetalk_protocols[protocol]->default_buffersize);
	c->handle = firetalk_protocols[protocol]->create_handle();
	return(c);
}

void	firetalk_destroy_handle(firetalk_connection_t *conn) {
	VERIFYCONN;

	assert(conn->deleted == 0);
	assert(conn->handle != NULL);
	memset(conn->callbacks, 0, sizeof(conn->callbacks));

	firetalk_protocols[conn->protocol]->destroy_handle(conn->handle);
	conn->handle = NULL;
	conn->deleted = 1;
}

fte_t	firetalk_disconnect(firetalk_connection_t *conn) {
	VERIFYCONN;

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->disconnect(conn->handle));
}

fte_t	firetalk_signon(firetalk_connection_t *conn, const char *server, uint16_t port, const char *const username) {
	VERIFYCONN;

	if (conn->sock.state != FCS_NOTCONNECTED) {
		firetalk_disconnect(conn);
		conn->sock.state = FCS_NOTCONNECTED;
	}

	free(conn->username);
	conn->username = strdup(username);
	if (conn->username == NULL)
		abort();
	conn->buffer.pos = 0;

	if (server == NULL)
		server = firetalk_protocols[conn->protocol]->default_server;

	if (port == 0)
		port = firetalk_protocols[conn->protocol]->default_port;

	return(firetalk_sock_connect_host(&(conn->sock), server, port));
}

void	firetalk_callback_connected(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_handle(c);
	struct sockaddr_in *localaddr = firetalk_sock_localhost4(&(conn->sock));

	conn->sock.state = FCS_ACTIVE;
	conn->localip = htonl(localaddr->sin_addr.s_addr);

	if (conn->callbacks[FC_CONNECTED])
		conn->callbacks[FC_CONNECTED](conn, conn->clientstruct);
}

fte_t	firetalk_handle_file_synack(firetalk_connection_t *conn, firetalk_transfer_t *file) {
	int	i;
	unsigned int o = sizeof(int);

	if (getsockopt(file->sock.fd, SOL_SOCKET, SO_ERROR, &i, &o)) {
		firetalk_file_cancel(conn, file);
		return(FE_SOCKET);
	}

	if (i != 0) {
		firetalk_file_cancel(conn, file);
		return(FE_CONNECT);
	}

	file->state = FF_STATE_TRANSFERRING;

	if (conn->callbacks[FC_FILE_START])
		conn->callbacks[FC_FILE_START](conn, conn->clientstruct, file, file->clientfilestruct);
	return(FE_SUCCESS);
}

int	firetalk_get_protocol(firetalk_connection_t *conn) {
	VERIFYCONN;

	return(conn->protocol);
}

fte_t	firetalk_register_callback(firetalk_connection_t *conn, const int type, void (*function)(firetalk_connection_t *, struct firetalk_useragent_connection_t *, ...)) {
	VERIFYCONN;

	if ((type < 0) || (type >= FC_MAX))
		return(FE_CALLBACKNUM);
	conn->callbacks[type] = function;
	return(FE_SUCCESS);
}

fte_t	firetalk_im_add_buddy(firetalk_connection_t *conn, const char *const name, const char *const group, const char *const friendly) {
	firetalk_buddy_t *iter;

	VERIFYCONN;

	if ((iter = firetalk_im_find_buddy(conn, name)) != NULL)
		firetalk_im_replace_buddy(conn, iter, name, group, friendly);
	else
		iter = firetalk_im_insert_buddy(conn, name, group, friendly);

        if (conn->sock.state != FCS_NOTCONNECTED) {
		fte_t	ret;

		ret = firetalk_protocols[conn->protocol]->im_add_buddy(conn->handle, iter->nickname, iter->group, iter->friendly);
		if (ret != FE_SUCCESS)
			return(ret);
	}

	if ((isonline_hack != NULL) && (firetalk_protocols[conn->protocol]->comparenicks(iter->nickname, isonline_hack) == FE_SUCCESS))
		firetalk_callback_im_buddyonline(conn->handle, iter->nickname, 1);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_add_deny(firetalk_connection_t *conn, const char *const nickname) {
	VERIFYCONN;

	if (firetalk_im_internal_add_deny(conn, nickname) == NULL)
		return(FE_UNKNOWN);

	if (conn->sock.state == FCS_ACTIVE)
		return(firetalk_protocols[conn->protocol]->im_add_deny(conn->handle, nickname));
	return(FE_SUCCESS);
}

fte_t	firetalk_im_remove_deny(firetalk_connection_t *conn, const char *const nickname) {
	fte_t	ret;

	VERIFYCONN;

	ret = firetalk_im_internal_remove_deny(conn,nickname);
	if (ret != FE_SUCCESS)
		return(ret);

	if (conn->sock.state == FCS_ACTIVE)
		return(firetalk_protocols[conn->protocol]->im_remove_deny(conn->handle, nickname));
	return(FE_SUCCESS);
}

fte_t	firetalk_im_send_message(firetalk_connection_t *conn, const char *const dest, const char *const message, const int auto_flag) {
	fte_t	e;

	VERIFYCONN;

	if ((conn->sock.state != FCS_ACTIVE) && (strcasecmp(dest, ":RAW") != 0))
		return(FE_NOTCONNECTED);

	e = firetalk_protocols[conn->protocol]->im_send_message(conn->handle, dest, message, auto_flag);
	if (e != FE_SUCCESS)
		return(e);

	e = firetalk_protocols[conn->protocol]->periodic(conn);
	if (e != FE_SUCCESS && e != FE_IDLEFAST)
		return(e);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_send_action(firetalk_connection_t *conn, const char *const dest, const char *const message, const int auto_flag) {
	fte_t	e;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	e = firetalk_protocols[conn->protocol]->im_send_action(conn->handle, dest, message, auto_flag);
	if (e != FE_SUCCESS)
		return(e);

	e = firetalk_protocols[conn->protocol]->periodic(conn);
	if (e != FE_SUCCESS && e != FE_IDLEFAST)
		return(e);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_get_info(firetalk_connection_t *conn, const char *const nickname) {
	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->get_info(conn->handle, nickname));
}

fte_t	firetalk_set_info(firetalk_connection_t *conn, const char *const info) {
	VERIFYCONN;

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_info(conn->handle, info));
}

fte_t	firetalk_chat_listmembers(firetalk_connection_t *conn, const char *const roomname) {
	firetalk_room_t *room;
	firetalk_member_t *memberiter;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	if (!conn->callbacks[FC_CHAT_LISTMEMBER])
		return(FE_SUCCESS);

	room = firetalk_find_room(conn, roomname);
	if (room == NULL)
		return(firetalkerror);

	for (memberiter = room->member_head; memberiter != NULL; memberiter = memberiter->next)
		conn->callbacks[FC_CHAT_LISTMEMBER](conn, conn->clientstruct, room->name, memberiter->nickname, memberiter->admin);

	return(FE_SUCCESS);
}

const char *firetalk_chat_normalize(firetalk_connection_t *conn, const char *const room) {
	return(firetalk_protocols[conn->protocol]->room_normalize(room));
}

fte_t	firetalk_set_away(firetalk_connection_t *conn, const char *const message, const int auto_flag) {
	VERIFYCONN;

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_away(conn->handle, message, auto_flag));
}

fte_t	firetalk_set_nickname(firetalk_connection_t *conn, const char *const nickname) {
	VERIFYCONN;

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_nickname(conn->handle, nickname));
}

fte_t	firetalk_set_password(firetalk_connection_t *conn, const char *const oldpass, const char *const newpass) {
	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_password(conn->handle, oldpass, newpass));
}

fte_t	firetalk_set_privacy(firetalk_connection_t *conn, const char *const mode) {
	VERIFYCONN;

	assert(mode != NULL);

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_privacy(conn->handle, mode));
}

fte_t	firetalk_im_evil(firetalk_connection_t *conn, const char *const who) {
	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->im_evil(conn->handle, who));
}

fte_t	firetalk_chat_join(firetalk_connection_t *conn, const char *const room) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_join(conn->handle, normalroom));
}

fte_t	firetalk_chat_part(firetalk_connection_t *conn, const char *const room) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_part(conn->handle, normalroom));
}

fte_t	firetalk_chat_send_message(firetalk_connection_t *conn, const char *const room, const char *const message, const int auto_flag) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	if (*room == ':')
		normalroom = room;
	else
		normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_send_message(conn->handle, normalroom, message, auto_flag));
}

fte_t	firetalk_chat_send_action(firetalk_connection_t *conn, const char *const room, const char *const message, const int auto_flag) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_send_action(conn->handle, normalroom, message, auto_flag));
}

fte_t	firetalk_chat_invite(firetalk_connection_t *conn, const char *const room, const char *const who, const char *const message) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_invite(conn->handle, normalroom, who, message));
}

fte_t	firetalk_chat_set_topic(firetalk_connection_t *conn, const char *const room, const char *const topic) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_set_topic(conn->handle, normalroom, topic));
}

fte_t	firetalk_chat_op(firetalk_connection_t *conn, const char *const room, const char *const who) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_op(conn->handle, normalroom, who));
}

fte_t	firetalk_chat_deop(firetalk_connection_t *conn, const char *const room, const char *const who) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_deop(conn->handle, normalroom, who));
}

fte_t	firetalk_chat_kick(firetalk_connection_t *conn, const char *const room, const char *const who, const char *const reason) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_kick(conn->handle, normalroom, who, reason));
}

fte_t	firetalk_subcode_send_request(firetalk_connection_t *conn, const char *const to, const char *const command, const char *const args) {
	VERIFYCONN;

	if (conn->sock.state != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

//	return(firetalk_protocols[conn->protocol]->subcode_send_request(conn->handle, to, command, args));
	firetalk_enqueue(&conn->subcode_requests, to, firetalk_protocols[conn->protocol]->subcode_encode(conn->handle, command, args));
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_send_reply(firetalk_connection_t *conn, const char *const to, const char *const command, const char *const args) {
	VERIFYCONN;

	if ((conn->sock.state != FCS_ACTIVE) && (*to != ':'))
		return(FE_NOTCONNECTED);

//	return(firetalk_protocols[conn->protocol]->subcode_send_reply(conn->handle, to, command, args));
	firetalk_enqueue(&conn->subcode_replies, to, firetalk_protocols[conn->protocol]->subcode_encode(conn->handle, command, args));
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_request_callback(firetalk_connection_t *conn, const char *const command, void (*callback)(firetalk_connection_t *, struct firetalk_useragent_connection_t *, const char *const, const char *const, const char *const)) {
	VERIFYCONN;

	if (command == NULL) {
		if (conn->subcode_request_default != NULL) {
			if (conn->subcode_request_default->staticresp != NULL) {
				free(conn->subcode_request_default->staticresp);
				conn->subcode_request_default->staticresp = NULL;
			}
			free(conn->subcode_request_default);
		}
		conn->subcode_request_default = calloc(1, sizeof(*conn->subcode_request_default));
		if (conn->subcode_request_default == NULL)
			abort();
		conn->subcode_request_default->callback = (ptrtofnct)callback;
	} else {
		firetalk_subcode_callback_t *iter;

		iter = calloc(1, sizeof(*iter));
		if (iter == NULL)
			abort();
		iter->next = conn->subcode_request_head;
		conn->subcode_request_head = iter;
		iter->command = strdup(command);
		if (iter->command == NULL)
			abort();
		iter->callback = (ptrtofnct)callback;
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_request_reply(firetalk_connection_t *conn, const char *const command, const char *const reply) {
	VERIFYCONN;

	if (command == NULL) {
		if (conn->subcode_request_default != NULL) {
			if (conn->subcode_request_default->staticresp != NULL) {
				free(conn->subcode_request_default->staticresp);
				conn->subcode_request_default->staticresp = NULL;
			}
			free(conn->subcode_request_default);
		}
		conn->subcode_request_default = calloc(1, sizeof(*conn->subcode_request_default));
		if (conn->subcode_request_default == NULL)
			abort();
		conn->subcode_request_default->staticresp = strdup(reply);
		if (conn->subcode_request_default->staticresp == NULL)
			abort();
	} else {
		firetalk_subcode_callback_t *iter;

		iter = calloc(1, sizeof(*iter));
		if (iter == NULL)
			abort();
		iter->next = conn->subcode_request_head;
		conn->subcode_request_head = iter;
		iter->command = strdup(command);
		if (iter->command == NULL)
			abort();
		iter->staticresp = strdup(reply);
		if (iter->staticresp == NULL)
			abort();
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_reply_callback(firetalk_connection_t *conn, const char *const command, void (*callback)(firetalk_connection_t *, struct firetalk_useragent_connection_t *, const char *const, const char *const, const char *const)) {
	VERIFYCONN;

	if (command == NULL) {
		if (conn->subcode_reply_default)
			free(conn->subcode_reply_default);
		conn->subcode_reply_default = calloc(1, sizeof(*conn->subcode_reply_default));
		if (conn->subcode_reply_default == NULL)
			abort();
		conn->subcode_reply_default->callback = (ptrtofnct)callback;
	} else {
		firetalk_subcode_callback_t *iter;

		iter = calloc(1, sizeof(*iter));
		if (iter == NULL)
			abort();
		iter->next = conn->subcode_reply_head;
		conn->subcode_reply_head = iter;
		iter->command = strdup(command);
		if (iter->command == NULL)
			abort();
		iter->callback = (ptrtofnct)callback;
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_file_offer(firetalk_connection_t *conn, const char *const nickname, const char *const filename, struct firetalk_useragent_transfer_t *clientfilestruct) {
	firetalk_transfer_t *iter;
	struct stat s;
	struct sockaddr_in addr;
	char	args[256];
	unsigned int l;

	VERIFYCONN;

	iter = calloc(1, sizeof(*iter));
	if (iter == NULL)
		abort();
	iter->next = conn->file_head;
	conn->file_head = iter;
	iter->who = strdup(nickname);
	if (iter->who == NULL)
		abort();
	iter->filename = strdup(filename);
	if (iter->filename == NULL)
		abort();
	iter->clientfilestruct = clientfilestruct;

	iter->filefd = open(filename, O_RDONLY);
	if (iter->filefd == -1) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return(FE_IOERROR);
	}

	if (fstat(iter->filefd, &s) != 0) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return(FE_IOERROR);
	}

	iter->size = (long)s.st_size;

	iter->sock.fd = socket(PF_INET, SOCK_STREAM, 0);
	if (iter->sock.fd == -1) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return(FE_SOCKET);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(iter->sock.fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return(FE_SOCKET);
	}

	if (listen(iter->sock.fd, 1) != 0) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return(FE_SOCKET);
	}

	l = (unsigned int)sizeof(addr);
	if (getsockname(iter->sock.fd, (struct sockaddr *)&addr, &l) != 0) {
		assert(conn->file_head == iter);
		firetalk_file_cancel(conn, iter);
		assert(conn->file_head != iter);
		return(FE_SOCKET);
	}

	iter->state = FF_STATE_WAITREMOTE;
	iter->direction = FF_DIRECTION_SENDING;
	iter->port = ntohs(addr.sin_port);
	iter->type = FF_TYPE_DCC;
	snprintf(args, sizeof(args), "SEND %s %u %u %ld", iter->filename, conn->localip, iter->port, iter->size);
	return(firetalk_subcode_send_request(conn, nickname, "DCC", args));
}

fte_t	firetalk_file_accept(firetalk_connection_t *conn, firetalk_transfer_t *fileiter, struct firetalk_useragent_transfer_t *clientfilestruct, const char *const localfile) {
	struct sockaddr_in addr;

	VERIFYCONN;

	fileiter->clientfilestruct = clientfilestruct;

	fileiter->filefd = open(localfile, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
	if (fileiter->filefd == -1)
		return(FE_NOPERMS);

	addr.sin_family = AF_INET;
	addr.sin_port = fileiter->port;
	memcpy(&addr.sin_addr.s_addr, &fileiter->inet_ip, 4);
	fileiter->sock.fd = firetalk_internal_connect(&addr
#ifdef _FC_USE_IPV6
	, NULL
#endif
	);
	if (fileiter->sock.fd == -1) {
		firetalk_file_cancel(conn, fileiter);
		return(FE_SOCKET);
	}
	fileiter->state = FF_STATE_WAITSYNACK;
	return(FE_SUCCESS);
}

fte_t	firetalk_file_cancel(firetalk_connection_t *conn, firetalk_transfer_t *filehandle) {
	firetalk_transfer_t *fileiter, *prev;

	VERIFYCONN;

	prev = NULL;
	for (fileiter = conn->file_head; fileiter != NULL; fileiter = fileiter->next) {
		if (fileiter == filehandle) {
			if (prev != NULL)
				prev->next = fileiter->next;
			else
				conn->file_head = fileiter->next;
			if (fileiter->who) {
				free(fileiter->who);
				fileiter->who = NULL;
			}
			if (fileiter->filename) {
				free(fileiter->filename);
				fileiter->filename = NULL;
			}
			firetalk_sock_close(&(fileiter->sock));
			if (fileiter->filefd >= 0) {
				close(fileiter->filefd);
				fileiter->filefd = -1;
			}
			free(fileiter);
			return(FE_SUCCESS);
		}
		prev = fileiter;
	}
	return(FE_NOTFOUND);
}

fte_t	firetalk_file_refuse(firetalk_connection_t *conn, firetalk_transfer_t *filehandle) {
	return(firetalk_file_cancel(conn, filehandle));
}

fte_t	firetalk_compare_nicks(firetalk_connection_t *conn, const char *const nick1, const char *const nick2) {
	VERIFYCONN;

	if ((nick1 == NULL) || (nick2 == NULL))
		return(FE_NOMATCH);

	return(firetalk_protocols[conn->protocol]->comparenicks(nick1, nick2));
}

fte_t	firetalk_isprint(firetalk_connection_t *conn, const int c) {
	VERIFYCONN;

	return(firetalk_protocols[conn->protocol]->isprintable(c));
}

fte_t	firetalk_select(void) {
	return(firetalk_select_custom(0, NULL, NULL, NULL, NULL));
}

fte_t	firetalk_select_custom(int n, fd_set *fd_read, fd_set *fd_write, fd_set *fd_except, struct timeval *timeout) {
	fte_t	ret;
	fd_set *my_read, *my_write, *my_except;
	fd_set internal_read, internal_write, internal_except;
	struct timeval internal_timeout, *my_timeout;
	firetalk_connection_t *fchandle;

	my_read = fd_read;
	my_write = fd_write;
	my_except = fd_except;
	my_timeout = timeout;

	if (!my_read) {
		my_read = &internal_read;
		FD_ZERO(my_read);
	}

	if (!my_write) {
		my_write = &internal_write;
		FD_ZERO(my_write);
	}

	if (!my_except) {
		my_except = &internal_except;
		FD_ZERO(my_except);
	}

	if (!my_timeout) {
		my_timeout = &internal_timeout;
		my_timeout->tv_sec = 15;
		my_timeout->tv_usec = 0;
	}

	if (my_timeout->tv_sec > 15)
		my_timeout->tv_sec = 15;

	/* internal preselect */
	for (fchandle = handle_head; fchandle != NULL; fchandle = fchandle->next) {
		firetalk_transfer_t *fileiter;

		if (fchandle->deleted)
			continue;

		for (fileiter = fchandle->file_head; fileiter != NULL; fileiter = fileiter->next) {
			if (fileiter->state == FF_STATE_TRANSFERRING) {
				if (fileiter->sock.fd >= n)
					n = fileiter->sock.fd + 1;
				switch (fileiter->direction) {
				  case FF_DIRECTION_SENDING:
					assert(fileiter->sock.fd >= 0);
					FD_SET(fileiter->sock.fd, my_write);
					FD_SET(fileiter->sock.fd, my_except);
					break;
				  case FF_DIRECTION_RECEIVING:
					assert(fileiter->sock.fd >= 0);
					FD_SET(fileiter->sock.fd, my_read);
					FD_SET(fileiter->sock.fd, my_except);
					break;
				}
			} else if (fileiter->state == FF_STATE_WAITREMOTE) {
				assert(fileiter->sock.fd >= 0);
				if (fileiter->sock.fd >= n)
					n = fileiter->sock.fd + 1;
				FD_SET(fileiter->sock.fd, my_read);
				FD_SET(fileiter->sock.fd, my_except);
			} else if (fileiter->state == FF_STATE_WAITSYNACK) {
				assert(fileiter->sock.fd >= 0);
				if (fileiter->sock.fd >= n)
					n = fileiter->sock.fd + 1;
				FD_SET(fileiter->sock.fd, my_write);
				FD_SET(fileiter->sock.fd, my_except);
			}
		}

		if (fchandle->sock.state == FCS_NOTCONNECTED)
			continue;

		while (fchandle->subcode_requests.count > 0) {
			int	count = fchandle->subcode_requests.count;

			firetalk_protocols[fchandle->protocol]->im_send_message(fchandle->handle, fchandle->subcode_requests.keys[0], "", 0);
			assert(fchandle->subcode_requests.count < count);
		}

		while (fchandle->subcode_replies.count > 0) {
			int	count = fchandle->subcode_replies.count;

			firetalk_protocols[fchandle->protocol]->im_send_message(fchandle->handle, fchandle->subcode_replies.keys[0], "", 1);
			assert(fchandle->subcode_replies.count < count);
		}

		firetalk_protocols[fchandle->protocol]->periodic(fchandle);

		firetalk_sock_preselect(&(fchandle->sock), my_read, my_write, my_except, &n);
	}

	/* per-protocol preselect, UI prepoll */
	for (fchandle = handle_head; fchandle != NULL; fchandle = fchandle->next) {
		if (fchandle->deleted)
			continue;
		firetalk_protocols[fchandle->protocol]->preselect(fchandle->handle, my_read, my_write, my_except, &n);
		if (fchandle->callbacks[FC_PRESELECT])
			fchandle->callbacks[FC_PRESELECT](fchandle, fchandle->clientstruct);
	}

	/* select */
	if (n > 0) {
		ret = select(n, my_read, my_write, my_except, my_timeout);
		if (ret == -1)
			return(FE_PACKET);
	}

	/* per-protocol postselect, UI postpoll */
	for (fchandle = handle_head; fchandle != NULL; fchandle = fchandle->next) {
		if (fchandle->deleted)
			continue;

		firetalk_protocols[fchandle->protocol]->postselect(fchandle->handle, my_read, my_write, my_except);
		if (fchandle->callbacks[FC_POSTSELECT])
			fchandle->callbacks[FC_POSTSELECT](fchandle, fchandle->clientstruct);
	}

	/* internal postpoll */
	for (fchandle = handle_head; fchandle != NULL; fchandle = fchandle->next) {
		firetalk_transfer_t *fileiter, *filenext;
		firetalk_sock_state_t state;
		fte_t	ret;

		if (fchandle->deleted)
			continue;

		for (fileiter = fchandle->file_head; fileiter != NULL; fileiter = filenext) {
			filenext = fileiter->next;
			if (fileiter->state == FF_STATE_TRANSFERRING) {
				assert(fileiter->sock.fd >= 0);
				if (FD_ISSET(fileiter->sock.fd, my_write))
					firetalk_handle_send(fchandle, fileiter);
				if ((fileiter->sock.fd != -1) && FD_ISSET(fileiter->sock.fd, my_read))
					firetalk_handle_receive(fchandle, fileiter);
				if ((fileiter->sock.fd != -1) && FD_ISSET(fileiter->sock.fd, my_except)) {
					if (fchandle->callbacks[FC_FILE_ERROR])
						fchandle->callbacks[FC_FILE_ERROR](fchandle, fchandle->clientstruct, fileiter, fileiter->clientfilestruct, FE_IOERROR);
					firetalk_file_cancel(fchandle, fileiter);
				}
			} else if (fileiter->state == FF_STATE_WAITREMOTE) {
				assert(fileiter->sock.fd >= 0);
				if (FD_ISSET(fileiter->sock.fd, my_read)) {
					struct sockaddr_in addr;
					unsigned int l = sizeof(addr);
					int	s;

					s = accept(fileiter->sock.fd, (struct sockaddr *)&addr, &l);
					if (s == -1) {
						if (fchandle->callbacks[FC_FILE_ERROR])
							fchandle->callbacks[FC_FILE_ERROR](fchandle, fchandle->clientstruct, fileiter, fileiter->clientfilestruct, FE_SOCKET);
						firetalk_file_cancel(fchandle, fileiter);
					} else {
						close(fileiter->sock.fd);
						fileiter->sock.fd = s;
						fileiter->state = FF_STATE_TRANSFERRING;
						if (fchandle->callbacks[FC_FILE_START])
							fchandle->callbacks[FC_FILE_START](fchandle, fchandle->clientstruct, fileiter, fileiter->clientfilestruct);
					}
				} else if (FD_ISSET(fileiter->sock.fd, my_except)) {
					if (fchandle->callbacks[FC_FILE_ERROR])
						fchandle->callbacks[FC_FILE_ERROR](fchandle, fchandle->clientstruct, fileiter, fileiter->clientfilestruct, FE_IOERROR);
					firetalk_file_cancel(fchandle, fileiter);
				}
			} else if (fileiter->state == FF_STATE_WAITSYNACK) {
				assert(fileiter->sock.fd >= 0);
				if (FD_ISSET(fileiter->sock.fd, my_write))
					firetalk_handle_file_synack(fchandle, fileiter);
				if (FD_ISSET(fileiter->sock.fd, my_except))
					firetalk_file_cancel(fchandle, fileiter);
			}
		}

		errno = 0;
		state = fchandle->sock.state;
		if ((ret = firetalk_sock_postselect(&(fchandle->sock), my_read, my_write, my_except, &(fchandle->buffer))) != FE_SUCCESS) {
			assert(fchandle->sock.state == FCS_NOTCONNECTED);
			if (state == FCS_ACTIVE)
				firetalk_protocols[fchandle->protocol]->disconnected(fchandle->handle, FE_DISCONNECT);
			else {
				if (fchandle->callbacks[FC_CONNECTFAILED])
					fchandle->callbacks[FC_CONNECTFAILED](fchandle, fchandle->clientstruct, ret, strerror(errno));
			}
			continue;
		}

		if (fchandle->sock.state == FCS_SEND_SIGNON) {
			fchandle->sock.state = FCS_WAITING_SIGNON;
			firetalk_protocols[fchandle->protocol]->signon(fchandle->handle, fchandle->username);
		} else if (fchandle->buffer.readdata) {
			if (fchandle->sock.state == FCS_ACTIVE)
				firetalk_protocols[fchandle->protocol]->got_data(fchandle->handle, fchandle->buffer.buffer, &fchandle->buffer.pos);
			else
				firetalk_protocols[fchandle->protocol]->got_data_connecting(fchandle->handle, fchandle->buffer.buffer, &fchandle->buffer.pos);
			if (fchandle->buffer.pos == fchandle->buffer.size)
				firetalk_callback_disconnect(fchandle->handle, FE_PACKETSIZE);
		}
	}

	/* handle deleted connections */
	{
		firetalk_connection_t *fchandleprev, *fchandlenext;

		fchandleprev = NULL;
		for (fchandle = handle_head; fchandle != NULL; fchandle = fchandlenext) {
			fchandlenext = fchandle->next;
			if (fchandle->deleted) {
				assert(fchandle->handle == NULL);
				if (fchandle->buddy_head != NULL) {
					firetalk_buddy_t *iter, *iternext;

					for (iter = fchandle->buddy_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						iter->next = NULL;
						if (iter->nickname != NULL) {
							free(iter->nickname);
							iter->nickname = NULL;
						}
						if (iter->group != NULL) {
							free(iter->group);
							iter->group = NULL;
						}
						if (iter->friendly != NULL) {
							free(iter->friendly);
							iter->friendly = NULL;
						}
						if (iter->capabilities != NULL) {
							free(iter->capabilities);
							iter->capabilities = NULL;
						}
						free(iter);
					}
					fchandle->buddy_head = NULL;
				}
				if (fchandle->deny_head != NULL) {
					firetalk_deny_t *iter, *iternext;

					for (iter = fchandle->deny_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						iter->next = NULL;
						if (iter->nickname != NULL) {
							free(iter->nickname);
							iter->nickname = NULL;
						}
						free(iter);
					}
					fchandle->deny_head = NULL;
				}
				if (fchandle->room_head != NULL) {
					firetalk_room_t *iter, *iternext;

					for (iter = fchandle->room_head; iter != NULL; iter = iternext) {
						firetalk_member_t *memberiter, *memberiternext;

						for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiternext) {
							memberiternext = memberiter->next;
							memberiter->next = NULL;
							if (memberiter->nickname != NULL) {
								free(memberiter->nickname);
								memberiter->nickname = NULL;
							}
							free(memberiter);
						}
						iter->member_head = NULL;
						iternext = iter->next;
						if (iter->name != NULL) {
							free(iter->name);
							iter->name = NULL;
						}
						free(iter);
					}
					fchandle->room_head = NULL;
				}
				while (fchandle->file_head != NULL)
					firetalk_file_cancel(fchandle, fchandle->file_head);
				if (fchandle->subcode_request_head != NULL) {
					firetalk_subcode_callback_t *iter, *iternext;

					for (iter = fchandle->subcode_request_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						iter->next = NULL;
						if (iter->command != NULL) {
							free(iter->command);
							iter->command = NULL;
						}
						if (iter->staticresp != NULL) {
							free(iter->staticresp);
							iter->staticresp = NULL;
						}
						free(iter);
					}
					fchandle->subcode_request_head = NULL;
				}
				if (fchandle->subcode_request_default != NULL) {
					if (fchandle->subcode_request_default->command != NULL) {
						free(fchandle->subcode_request_default->command);
						fchandle->subcode_request_default->command = NULL;
					}
					if (fchandle->subcode_request_default->staticresp != NULL) {
						free(fchandle->subcode_request_default->staticresp);
						fchandle->subcode_request_default->staticresp = NULL;
					}
					free(fchandle->subcode_request_default);
					fchandle->subcode_request_default = NULL;
				}
				if (fchandle->subcode_reply_head != NULL) {
					firetalk_subcode_callback_t *iter, *iternext;

					for (iter = fchandle->subcode_reply_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						iter->next = NULL;
						free(iter->command);
						iter->command = NULL;
						free(iter);
					}
					fchandle->subcode_reply_head = NULL;
				}
				if (fchandle->subcode_reply_default != NULL) {
					if (fchandle->subcode_reply_default->command != NULL) {
						free(fchandle->subcode_reply_default->command);
						fchandle->subcode_reply_default->command = NULL;
					}
					free(fchandle->subcode_reply_default);
					fchandle->subcode_reply_default = NULL;
				}
				if (fchandle->username != NULL) {
					free(fchandle->username);
					fchandle->username = NULL;
				}
				firetalk_buffer_free(&(fchandle->buffer));
				if (fchandleprev == NULL) {
					assert(fchandle == handle_head);
					handle_head = fchandlenext;
				} else {
					assert(fchandle != handle_head);
					fchandleprev->next = fchandlenext;
				}

				fchandle->canary = NULL;
				free(fchandle);
				fchandle = NULL;
			} else
				fchandleprev = fchandle;
		}
	}

	return(FE_SUCCESS);
}
