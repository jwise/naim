/* firetalk.c - FireTalk protocol interface
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
static firetalk_connection_t *conn_head = NULL;
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
		firetalk_protocol_irc_pass,
		firetalk_protocol_slcp,
		firetalk_protocol_toc2;

	if (firetalk_register_protocol(&firetalk_protocol_irc) != FE_SUCCESS)
		abort();
	if (firetalk_register_protocol(&firetalk_protocol_irc_pass) != FE_SUCCESS)
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


void	*firetalk_connection_t_magic = &firetalk_connection_t_magic,
	*firetalk_connection_t_canary = &firetalk_connection_t_canary;

int	firetalk_connection_t_valid(const firetalk_connection_t *this) {
	if (this->magic != &firetalk_connection_t_magic)
		return(0);
	if (this->canary != &firetalk_connection_t_canary)
		return(0);
	return(1);
}

/* firetalk_find_by_toc searches the firetalk handle list for the toc handle passed, and returns the firetalk handle */
firetalk_connection_t *firetalk_find_conn(const struct firetalk_driver_connection_t *const c) {
	firetalk_connection_t *conn;

	for (conn = conn_head; conn != NULL; conn = conn->next)
		if (conn->handle == c) {
			assert(firetalk_connection_t_valid(conn));
			return(conn);
		}
	abort();
}

firetalk_connection_t *firetalk_find_clientstruct(struct firetalk_useragent_connection_t *clientstruct) {
	firetalk_connection_t *conn;

	for (conn = conn_head; conn != NULL; conn = conn->next)
		if (conn->clientstruct == clientstruct) {
			assert(firetalk_connection_t_valid(conn));
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

	assert(firetalk_connection_t_valid(conn));

	for (iter = conn->deny_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->nickname, nickname) == FE_SUCCESS)
			break; /* not an error, user is in buddy list */

	if (iter == NULL) {
		if ((iter = firetalk_deny_t_new()) == NULL)
			abort();
		iter->next = conn->deny_head;
		conn->deny_head = iter;
		STRREPLACE(iter->nickname, nickname);
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

fte_t	firetalk_user_visible(firetalk_connection_t *conn, const char *const nickname) {
	firetalk_room_t *iter;

	assert(firetalk_connection_t_valid(conn));

	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		firetalk_member_t *mem;

		for (mem = iter->member_head; mem != NULL; mem = mem->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, mem->nickname, nickname) == FE_SUCCESS)
				return(FE_SUCCESS);
	}
	return(FE_NOMATCH);
}

fte_t	firetalk_user_visible_but(firetalk_connection_t *conn, const char *const room, const char *const nickname) {
	firetalk_room_t *iter;

	assert(firetalk_connection_t_valid(conn));

	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		firetalk_member_t *mem;

		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->name, room) == FE_SUCCESS)
			continue;
		for (mem = iter->member_head; mem != NULL; mem = mem->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, mem->nickname, nickname) == FE_SUCCESS)
				return(FE_SUCCESS);
	}
	return(FE_NOMATCH);
}

fte_t	firetalk_chat_internal_add_room(firetalk_connection_t *conn, const char *const name) {
	firetalk_room_t *iter;

	assert(firetalk_connection_t_valid(conn));

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->name, name) == FE_SUCCESS)
			return(FE_DUPEROOM); /* not an error, we're already in room */

	if ((iter = firetalk_room_t_new()) == NULL)
		abort();
	iter->next = conn->room_head;
	conn->room_head = iter;
	STRREPLACE(iter->name, name);

	return(FE_SUCCESS);
}

fte_t	firetalk_chat_internal_add_member(firetalk_connection_t *conn, const char *const room, const char *const nickname) {
	firetalk_room_t *iter;
	firetalk_member_t *memberiter;

	assert(firetalk_connection_t_valid(conn));

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->name, room) == FE_SUCCESS)
			break;

	if (iter == NULL) /* we don't know about that room */
		return(FE_NOTFOUND);

	for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, memberiter->nickname, nickname) == FE_SUCCESS)
			return(FE_SUCCESS);

	if ((memberiter = firetalk_member_t_new()) == NULL)
		abort();
	memberiter->next = iter->member_head;
	iter->member_head = memberiter;
	STRREPLACE(memberiter->nickname, nickname);

	return(FE_SUCCESS);
}

static void firetalk_im_delete_buddy(firetalk_connection_t *conn, const char *const nickname) {
	firetalk_buddy_t *iter, *prev;

	for (prev = NULL, iter = conn->buddy_head; iter != NULL; prev = iter, iter = iter->next) {
		assert(iter->nickname != NULL);
		assert(iter->group != NULL);

		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, nickname, iter->nickname) == FE_SUCCESS)
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
	firetalk_buddy_t_delete(iter);
	iter = NULL;
}

firetalk_buddy_t *firetalk_im_find_buddy(firetalk_connection_t *conn, const char *const name) {
	firetalk_buddy_t *iter;

	for (iter = conn->buddy_head; iter != NULL; iter = iter->next) {
		assert(iter->nickname != NULL);
		assert(iter->group != NULL);

		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->nickname, name) == FE_SUCCESS)
			return(iter);
	}
	return(NULL);
}

fte_t	firetalk_im_remove_buddy(firetalk_connection_t *conn, const char *const name) {
	firetalk_buddy_t *iter;

	assert(firetalk_connection_t_valid(conn));

	if ((iter = firetalk_im_find_buddy(conn, name)) == NULL)
		return(FE_NOTFOUND);

	if (conn->connected != FCS_NOTCONNECTED) {
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

	assert(firetalk_connection_t_valid(conn));

	prev = NULL;
	for (iter = conn->deny_head; iter != NULL; iter = iter->next) {
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, nickname, iter->nickname) == FE_SUCCESS) {
			if (conn->callbacks[FC_IM_DENYREMOVED] != NULL)
				conn->callbacks[FC_IM_DENYREMOVED](conn, conn->clientstruct, iter->nickname);

			if (prev)
				prev->next = iter->next;
			else
				conn->deny_head = iter->next;
			firetalk_deny_t_delete(iter);
			return(FE_SUCCESS);
		}
		prev = iter;
	}

	return(FE_NOTFOUND);
}

fte_t	firetalk_chat_internal_remove_room(firetalk_connection_t *conn, const char *const name) {
	firetalk_room_t *iter, *prev;

	assert(firetalk_connection_t_valid(conn));

	prev = NULL;
	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, name, iter->name) == FE_SUCCESS) {
			if (prev)
				prev->next = iter->next;
			else
				conn->room_head = iter->next;
			firetalk_room_t_delete(iter);
			return(FE_SUCCESS);
		}
		prev = iter;
	}

	return(FE_NOTFOUND);
}

fte_t	firetalk_chat_internal_remove_member(firetalk_connection_t *conn, const char *const room, const char *const nickname) {
	firetalk_room_t *iter;
	firetalk_member_t *memberiter, *memberprev;

	assert(firetalk_connection_t_valid(conn));

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->name, room) == FE_SUCCESS)
			break;

	if (iter == NULL) /* we don't know about that room */
		return(FE_NOTFOUND);

	memberprev = NULL;
	for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiter->next) {
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, memberiter->nickname,nickname) == FE_SUCCESS) {
			if (memberprev)
				memberprev->next = memberiter->next;
			else
				iter->member_head = memberiter->next;
			firetalk_member_t_delete(memberiter);
			return(FE_SUCCESS);
		}
		memberprev = memberiter;
	}

	return(FE_SUCCESS);
}

firetalk_room_t *firetalk_find_room(firetalk_connection_t *conn, const char *const room) {
	firetalk_room_t *roomiter;
	const char *normalroom;

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, roomiter->name, normalroom) == FE_SUCCESS)
			return(roomiter);

	firetalkerror = FE_NOTFOUND;
	return(NULL);
}

static firetalk_member_t *firetalk_find_member(firetalk_connection_t *conn, firetalk_room_t *r, const char *const name) {
	firetalk_member_t *memberiter;

	for (memberiter = r->member_head; memberiter != NULL; memberiter = memberiter->next)
		if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, memberiter->nickname, name) == FE_SUCCESS)
			return(memberiter);

	firetalkerror = FE_NOTFOUND;
	return(NULL);
}

void	firetalk_callback_needpass(struct firetalk_driver_connection_t *c, char *pass, const int size) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_NEEDPASS])
		conn->callbacks[FC_NEEDPASS](conn, conn->clientstruct, pass, size);
}

static const char *isonline_hack = NULL;

void	firetalk_callback_im_getmessage(struct firetalk_driver_connection_t *c, const char *const sender, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_deny_t *iter;

	if (strstr(message, "<a href=\"http://www.candidclicks.com/cgi-bin/enter.cgi?") != NULL) {
		firetalk_im_evil(conn, sender);
		return;
	}
	if (conn->callbacks[FC_IM_GETMESSAGE]) {
		for (iter = conn->deny_head; iter != NULL; iter = iter->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, sender, iter->nickname) == FE_SUCCESS)
				return;
		isonline_hack = sender;
		conn->callbacks[FC_IM_GETMESSAGE](conn, conn->clientstruct, sender, automessage, message);
		isonline_hack = NULL;
	}
}

void	firetalk_callback_im_getaction(struct firetalk_driver_connection_t *c, const char *const sender, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_deny_t *iter;

	if (conn->callbacks[FC_IM_GETACTION]) {
		for (iter = conn->deny_head; iter != NULL; iter = iter->next)
			if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, sender, iter->nickname) == FE_SUCCESS)
				return;
		isonline_hack = sender;
		conn->callbacks[FC_IM_GETACTION](conn, conn->clientstruct, sender, automessage, message);
		isonline_hack = NULL;
	}
}

void	firetalk_callback_im_buddyonline(struct firetalk_driver_connection_t *c, const char *const nickname, int online) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
				if (strcmp(buddyiter->nickname, nickname) != 0)
					STRREPLACE(buddyiter->nickname, nickname);
				if (conn->callbacks[FC_IM_BUDDYONLINE] != NULL)
					conn->callbacks[FC_IM_BUDDYONLINE](conn, conn->clientstruct, nickname);
			} else {
				buddyiter->away = buddyiter->typing = buddyiter->warnval = buddyiter->idletime = 0;
				if (conn->callbacks[FC_IM_BUDDYOFFLINE] != NULL)
					conn->callbacks[FC_IM_BUDDYOFFLINE](conn, conn->clientstruct, nickname);
			}
		}
}

void firetalk_callback_im_buddyflags(struct firetalk_driver_connection_t *c, const char *const nickname, const int flags) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_buddy_t *buddyiter;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->flags != flags) && (buddyiter->online == 1)) {
			buddyiter->flags = flags;
			if (conn->callbacks[FC_IM_BUDDYFLAGS] != NULL)
				conn->callbacks[FC_IM_BUDDYFLAGS](conn, conn->clientstruct, nickname, flags);
		}
}

void	firetalk_callback_im_buddyaway(struct firetalk_driver_connection_t *c, const char *const nickname, const int away) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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

	if ((iter = firetalk_buddy_t_new()) == NULL)
		abort();
	iter->next = conn->buddy_head;
	conn->buddy_head = iter;
	STRREPLACE(iter->nickname, name);
	STRREPLACE(iter->group, group);
	STRREPLACE(iter->friendly, friendly);
	if (conn->callbacks[FC_IM_BUDDYADDED] != NULL)
		conn->callbacks[FC_IM_BUDDYADDED](conn, conn->clientstruct, iter->nickname, iter->group, iter->friendly);
	return(iter);
}

static void firetalk_im_replace_buddy(firetalk_connection_t *conn, firetalk_buddy_t *iter, const char *const name, const char *const group, const char *const friendly) {
	if (!((strcmp(iter->group, group) == 0) && (((iter->friendly == NULL) && (friendly == NULL)) || ((iter->friendly != NULL) && (friendly != NULL) && (strcmp(iter->friendly, friendly) == 0))))) {
		/* user is in buddy list somewhere other than where the clients wants it */
		if ((conn->connected != FCS_NOTCONNECTED) && iter->uploaded)
			firetalk_protocols[conn->protocol]->im_remove_buddy(conn->handle, iter->nickname, iter->group);
		STRREPLACE(iter->group, group);
		STRREPLACE(iter->friendly, friendly);
		if (conn->callbacks[FC_IM_BUDDYADDED] != NULL)
			conn->callbacks[FC_IM_BUDDYADDED](conn, conn->clientstruct, iter->nickname, iter->group, iter->friendly);
	}
}

void	firetalk_callback_buddyadded(struct firetalk_driver_connection_t *c, const char *const name, const char *const group, const char *const friendly) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_buddy_t *iter;

	if ((iter = firetalk_im_find_buddy(conn, name)) != NULL) {
		iter->uploaded = 0;
		firetalk_im_replace_buddy(conn, iter, name, group, friendly);
	} else
		iter = firetalk_im_insert_buddy(conn, name, group, friendly);
	iter->uploaded = 1;
}

void	firetalk_callback_buddyremoved(struct firetalk_driver_connection_t *c, const char *const name, const char *const group) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_buddy_t *iter;

	if (((iter = firetalk_im_find_buddy(conn, name)) != NULL) && ((group == NULL) || (strcmp(iter->group, group) == 0)))
		firetalk_im_delete_buddy(conn, name);
}

void	firetalk_callback_denyadded(struct firetalk_driver_connection_t *c, const char *const name) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_deny_t *iter;

	iter = firetalk_im_internal_add_deny(conn, name);
	iter->uploaded = 1;
}

void	firetalk_callback_denyremoved(struct firetalk_driver_connection_t *c, const char *const name) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	firetalk_im_internal_remove_deny(conn, name);
}

void	firetalk_callback_typing(struct firetalk_driver_connection_t *c, const char *const name, const int typing) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_buddy_t *buddyiter;

	if (((buddyiter = firetalk_im_find_buddy(conn, nickname)) == NULL) || (buddyiter->online == 0))
		return;
	if ((buddyiter->capabilities == NULL) || (strcmp(buddyiter->capabilities, caps) != 0)) {
		STRREPLACE(buddyiter->capabilities, caps);
		if (conn->callbacks[FC_IM_CAPABILITIES] != NULL)
			conn->callbacks[FC_IM_CAPABILITIES](conn, conn->clientstruct, nickname, caps);
	}
}

void	firetalk_callback_warninfo(struct firetalk_driver_connection_t *c, char const *const nickname, const long warnval) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_ERROR])
		conn->callbacks[FC_ERROR](conn, conn->clientstruct, error, roomoruser, description);
}

void	firetalk_callback_connectfailed(struct firetalk_driver_connection_t *c, const fte_t error, const char *const description) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->connected == FCS_NOTCONNECTED)
		return;
	conn->connected = FCS_NOTCONNECTED;
	
	if (conn->callbacks[FC_CONNECTFAILED])
		conn->callbacks[FC_CONNECTFAILED](conn, conn->clientstruct, error, description);
}

void	firetalk_callback_disconnect(struct firetalk_driver_connection_t *c, const fte_t error) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_buddy_t *buddyiter;
	firetalk_deny_t *denyiter;

	FREESTR(conn->username);

	for (buddyiter = conn->buddy_head; buddyiter != NULL; buddyiter = buddyiter->next) {
		FREESTR(buddyiter->capabilities);
		buddyiter->idletime = buddyiter->warnval = buddyiter->typing = buddyiter->online = buddyiter->away = buddyiter->uploaded = 0;
	}

	for (denyiter = conn->deny_head; denyiter != NULL; denyiter = denyiter->next)
		denyiter->uploaded = 0;

	firetalk_room_t_list_delete(conn->room_head);
	conn->room_head = NULL;

	firetalk_queue_t_dtor(&(conn->subcode_requests));
	firetalk_queue_t_dtor(&(conn->subcode_replies));

	if ((conn->connected == FCS_ACTIVE) && conn->callbacks[FC_DISCONNECT])
		conn->callbacks[FC_DISCONNECT](conn, conn->clientstruct, error);
	conn->connected = FCS_NOTCONNECTED;
}

void	firetalk_callback_gotinfo(struct firetalk_driver_connection_t *c, const char *const nickname, const char *const info, const int warning, const long online, const long idle, const int flags) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_IM_GOTINFO])
		conn->callbacks[FC_IM_GOTINFO](conn, conn->clientstruct, nickname, info, warning, online, idle, flags);
}

void	firetalk_callback_idleinfo(struct firetalk_driver_connection_t *c, char const *const nickname, const long idletime) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_buddy_t *buddyiter;

	if (!conn->callbacks[FC_IM_IDLEINFO])
		return;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->idletime != idletime) && (buddyiter->online == 1)) {
			buddyiter->idletime = idletime;
			conn->callbacks[FC_IM_IDLEINFO](conn, conn->clientstruct, nickname, idletime);
		}
}

void	firetalk_callback_statusinfo(struct firetalk_driver_connection_t *c, const char *const nickname, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_IM_STATUSINFO])
		conn->callbacks[FC_IM_STATUSINFO](conn, conn->clientstruct, nickname, message);
}

void	firetalk_callback_doinit(struct firetalk_driver_connection_t *c, const char *const nickname) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	conn->connected = FCS_WAITING_SIGNON;
	if (conn->callbacks[FC_DOINIT])
		conn->callbacks[FC_DOINIT](conn, conn->clientstruct, nickname);
}

void	firetalk_callback_setidle(struct firetalk_driver_connection_t *c, long *const idle) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_SETIDLE])
		conn->callbacks[FC_SETIDLE](conn, conn->clientstruct, idle);
}

void	firetalk_callback_eviled(struct firetalk_driver_connection_t *c, const int newevil, const char *const eviler) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_EVILED])
		conn->callbacks[FC_EVILED](conn, conn->clientstruct, newevil, eviler);
}

void	firetalk_callback_newnick(struct firetalk_driver_connection_t *c, const char *const nickname) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_NEWNICK])
		conn->callbacks[FC_NEWNICK](conn, conn->clientstruct, nickname);
}

void	firetalk_callback_passchanged(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_PASSCHANGED])
		conn->callbacks[FC_PASSCHANGED](conn, conn->clientstruct);
}

void	firetalk_callback_user_nickchanged(struct firetalk_driver_connection_t *c, const char *const oldnick, const char *const newnick) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
			if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, memberiter->nickname, oldnick) == FE_SUCCESS) {
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
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (firetalk_chat_internal_add_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_JOINED])
		conn->callbacks[FC_CHAT_JOINED](conn, conn->clientstruct, room);
}

void	firetalk_callback_chat_left(struct firetalk_driver_connection_t *c, const char *const room) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (firetalk_chat_internal_remove_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_LEFT])
		conn->callbacks[FC_CHAT_LEFT](conn, conn->clientstruct, room);
}

void	firetalk_callback_chat_kicked(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (firetalk_chat_internal_remove_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_KICKED])
		conn->callbacks[FC_CHAT_KICKED](conn, conn->clientstruct, room, by, reason);
}

void	firetalk_callback_chat_getmessage(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_CHAT_GETMESSAGE])
		conn->callbacks[FC_CHAT_GETMESSAGE](conn, conn->clientstruct, room, from, automessage, message);
}

void	firetalk_callback_chat_getaction(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const int automessage, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_CHAT_GETACTION])
		conn->callbacks[FC_CHAT_GETACTION](conn, conn->clientstruct, room, from, automessage, message);
}

void	firetalk_callback_chat_invited(struct firetalk_driver_connection_t *c, const char *const room, const char *const from, const char *const message) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_CHAT_INVITED])
		conn->callbacks[FC_CHAT_INVITED](conn, conn->clientstruct, room, from, message);
}

void	firetalk_callback_chat_user_joined(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const extra) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_room_t *iter;

	iter = firetalk_find_room(conn, room);
	if (iter == NULL)
		return;

	if (who == NULL) {
		if (conn->callbacks[FC_CHAT_SYNCHED])
			conn->callbacks[FC_CHAT_SYNCHED](conn, conn->clientstruct, room);
	} else {
		if (firetalk_chat_internal_add_member(conn, room, who) != FE_SUCCESS)
			return;
		if (conn->callbacks[FC_CHAT_USER_JOINED])
			conn->callbacks[FC_CHAT_USER_JOINED](conn, conn->clientstruct, room, who, extra);
	}
}

void	firetalk_callback_chat_user_left(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (firetalk_chat_internal_remove_member(conn, room, who) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_USER_LEFT])
		conn->callbacks[FC_CHAT_USER_LEFT](conn, conn->clientstruct, room, who, reason);
}

void	firetalk_callback_chat_user_quit(struct firetalk_driver_connection_t *c, const char *const who, const char *const reason) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_room_t *roomiter;
	firetalk_member_t *memberiter, *membernext;
	
	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		for (memberiter = roomiter->member_head; memberiter != NULL; memberiter = membernext) {
			membernext = memberiter->next;
			if (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, memberiter->nickname, who) == FE_SUCCESS)
				firetalk_callback_chat_user_left(c, roomiter->name, who, reason);
		}
}

void	firetalk_callback_chat_gottopic(struct firetalk_driver_connection_t *c, const char *const room, const char *const topic, const char *const author) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_room_t *r;

	r = firetalk_find_room(conn, room);
	if (r != NULL)
		if (conn->callbacks[FC_CHAT_GOTTOPIC])
			conn->callbacks[FC_CHAT_GOTTOPIC](conn, conn->clientstruct, room, topic, author);
}

void	firetalk_callback_chat_modeset(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, const char *const mode, const char *const arg) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_CHAT_MODESET])
		conn->callbacks[FC_CHAT_MODESET](conn, conn->clientstruct, room, by, mode, arg);
}

void	firetalk_callback_chat_modeunset(struct firetalk_driver_connection_t *c, const char *const room, const char *const by, const char *const mode, const char *const arg) {
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_CHAT_MODEUNSET])
		conn->callbacks[FC_CHAT_MODEUNSET](conn, conn->clientstruct, room, by, mode, arg);
}

void	firetalk_callback_chat_user_opped(struct firetalk_driver_connection_t *c, const char *const room, const char *const who, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (conn->callbacks[FC_CHAT_KEYCHANGED])
		conn->callbacks[FC_CHAT_KEYCHANGED](conn, conn->clientstruct, room, what, by);
}

void	firetalk_callback_chat_opped(struct firetalk_driver_connection_t *c, const char *const room, const char *const by) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);

	if (firetalk_chat_internal_remove_member(conn, room, who) != FE_SUCCESS)
		return;
	if (conn->callbacks[FC_CHAT_USER_KICKED])
		conn->callbacks[FC_CHAT_USER_KICKED](conn, conn->clientstruct, room, who, by, reason);
}

const char *firetalk_subcode_get_request_reply(struct firetalk_driver_connection_t *c, const char *const command) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_subcode_callback_t *iter;

	for (iter = conn->subcode_request_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0)
			if (iter->staticresp != NULL)
				return(iter->staticresp);
	return(NULL);
}

void	firetalk_callback_subcode_request(struct firetalk_driver_connection_t *c, const char *const from, const char *const command, char *args) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_subcode_callback_t *iter;
	firetalk_sock_state_t connectedsave = conn->connected; /* nasty hack: some IRC servers send CTCP VERSION requests during signon, before 001, and demand a reply; idiots */

	conn->connected = FCS_ACTIVE;

	for (iter = conn->subcode_request_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0) {
			if (iter->staticresp != NULL)
				firetalk_subcode_send_reply(conn, from, command, iter->staticresp);
			else {
				isonline_hack = from;
				iter->callback(conn, conn->clientstruct, from, command, args);
				isonline_hack = NULL;
			}

			conn->connected = connectedsave;

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

	conn->connected = connectedsave;
}

void	firetalk_callback_subcode_reply(struct firetalk_driver_connection_t *c, const char *const from, const char *const command, const char *const args) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
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
	firetalk_connection_t *conn = firetalk_find_conn(c);
	firetalk_transfer_t *iter;

	if ((iter = firetalk_transfer_t_new()) == NULL)
		abort();
	iter->next = conn->file_head;
	conn->file_head = iter;
	STRREPLACE(iter->who, from);
	STRREPLACE(iter->filename, filename);
	iter->size = size;
	iter->state = FF_STATE_WAITLOCAL;
	iter->direction = FF_DIRECTION_RECEIVING;
	iter->port = htons(port);
	iter->type = type;
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
		conn->callbacks[FC_FILE_OFFER](conn, conn->clientstruct, iter, iter->who, iter->filename, iter->size);
}

void	firetalk_handle_receive(firetalk_connection_t *conn, firetalk_transfer_t *filestruct) {
	static char buffer[4096];
	ssize_t	s;

	while ((s = recv(filestruct->sock.fd, buffer, sizeof(buffer), MSG_DONTWAIT)) == 4096) {
		if (write(filestruct->filefd, buffer, sizeof(buffer)) != sizeof(buffer)) {
			if (conn->callbacks[FC_FILE_ERROR])
				conn->callbacks[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
		filestruct->bytes += sizeof(buffer);
	}
	if (s != -1) {
		if (write(filestruct->filefd, buffer, (size_t)s) != s) {
			if (conn->callbacks[FC_FILE_ERROR])
				conn->callbacks[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
		filestruct->bytes += s;
	}
	if (filestruct->type == FF_TYPE_DCC) {
		uint32_t netbytes = htonl((uint32_t)filestruct->bytes);

		if (firetalk_sock_send(&(filestruct->sock), &netbytes, sizeof(netbytes)) != FE_SUCCESS) {
			if (conn->callbacks[FC_FILE_ERROR])
				conn->callbacks[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
	}
	if (conn->callbacks[FC_FILE_PROGRESS])
		conn->callbacks[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
	if (filestruct->bytes == filestruct->size) {
		if (conn->callbacks[FC_FILE_FINISH])
			conn->callbacks[FC_FILE_FINISH](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->size);
		firetalk_file_cancel(conn, filestruct);
	}
}

void	firetalk_handle_send(firetalk_connection_t *conn, firetalk_transfer_t *filestruct) {
	static char buffer[4096];
	ssize_t	s;

	while ((s = read(filestruct->filefd, buffer, sizeof(buffer))) > 0) {
		if (firetalk_sock_send(&(filestruct->sock), buffer, s) != FE_SUCCESS) {
			if (conn->callbacks[FC_FILE_ERROR])
				conn->callbacks[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
		filestruct->bytes += s;
		if (conn->callbacks[FC_FILE_PROGRESS])
			conn->callbacks[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
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
	if (conn->callbacks[FC_FILE_PROGRESS])
		conn->callbacks[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
	if (filestruct->bytes == filestruct->size)
		if (conn->callbacks[FC_FILE_FINISH])
			conn->callbacks[FC_FILE_FINISH](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes);
	firetalk_file_cancel(conn, filestruct);
}

/* External function definitions */

const char *firetalk_strprotocol(const int p) {
	if ((p >= 0) && (p < FP_MAX))
		return(firetalk_protocols[p]->strprotocol);
	return(NULL);
}

const char *firetalk_strerror(const fte_t e) {
	switch (e) {
#define ERROR_EXPANDO(x, s) \
		case FE_##x: \
			return(s);
#include "firetalk-errors.h"
#undef ERROR_EXPANDO
		default:
			return("Invalid error number");
	}
}

firetalk_connection_t *firetalk_create_conn(const int protocol, struct firetalk_useragent_connection_t *clientstruct) {
	firetalk_connection_t *conn;

	if ((protocol < 0) || (protocol >= FP_MAX)) {
		firetalkerror = FE_BADPROTO;
		return(NULL);
	}
	if ((conn = firetalk_connection_t_new()) == NULL)
		abort();
	conn->next = conn_head;
	conn_head = conn;
	conn->clientstruct = clientstruct;
	conn->protocol = protocol;
	conn->handle = firetalk_protocols[protocol]->create_conn(firetalk_protocols[protocol]->cookie);
	conn->connected = FCS_NOTCONNECTED;
	return(conn);
}

void	firetalk_destroy_conn(firetalk_connection_t *conn) {
	assert(firetalk_connection_t_valid(conn));

	assert(conn->deleted == 0);
	assert(conn->handle != NULL);
	memset(conn->callbacks, 0, sizeof(conn->callbacks));

	firetalk_protocols[conn->protocol]->destroy_conn(conn->handle);
	conn->handle = NULL;
	conn->deleted = 1;
}

fte_t	firetalk_disconnect(firetalk_connection_t *conn) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);
	
	conn->connected = FCS_NOTCONNECTED;

	return(firetalk_protocols[conn->protocol]->disconnect(conn->handle));
}

fte_t	firetalk_signon(firetalk_connection_t *conn, const char *server, uint16_t port, const char *const username) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_NOTCONNECTED)
		firetalk_disconnect(conn);
	conn->connected = FCS_WAITING_SIGNON;

	STRREPLACE(conn->username, username);

	if (server == NULL)
		server = firetalk_protocols[conn->protocol]->default_server;

	if (port == 0)
		port = firetalk_protocols[conn->protocol]->default_port;

	return(firetalk_protocols[conn->protocol]->connect(conn->handle, server, port, username));
}

void	firetalk_callback_connected(struct firetalk_driver_connection_t *c) {
	firetalk_connection_t *conn = firetalk_find_conn(c);
#ifdef FT_OLD_CONN_FD
	struct sockaddr_in *localaddr = firetalk_sock_localhost4(&(conn->sock));
	conn->localip = htonl(localaddr->sin_addr.s_addr);
#else
#	warning XXX: Need conn->localip
#endif

	conn->connected = FCS_ACTIVE;
	
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

	file->sock.state = FCS_ACTIVE;
	file->state = FF_STATE_TRANSFERRING;

	if (conn->callbacks[FC_FILE_START])
		conn->callbacks[FC_FILE_START](conn, conn->clientstruct, file, file->clientfilestruct);
	return(FE_SUCCESS);
}

int	firetalk_get_protocol(firetalk_connection_t *conn) {
	assert(firetalk_connection_t_valid(conn));

	return(conn->protocol);
}

fte_t	firetalk_register_callback(firetalk_connection_t *conn, const int type, void (*function)(firetalk_connection_t *, struct firetalk_useragent_connection_t *, ...)) {
	assert(firetalk_connection_t_valid(conn));

	if ((type < 0) || (type >= FC_MAX))
		return(FE_CALLBACKNUM);
	conn->callbacks[type] = function;
	return(FE_SUCCESS);
}

fte_t	firetalk_im_add_buddy(firetalk_connection_t *conn, const char *const name, const char *const group, const char *const friendly) {
	firetalk_buddy_t *iter;

	assert(firetalk_connection_t_valid(conn));

	if ((iter = firetalk_im_find_buddy(conn, name)) != NULL)
		firetalk_im_replace_buddy(conn, iter, name, group, friendly);
	else
		iter = firetalk_im_insert_buddy(conn, name, group, friendly);

        if (conn->connected != FCS_NOTCONNECTED) {
		fte_t	ret;

		ret = firetalk_protocols[conn->protocol]->im_add_buddy(conn->handle, iter->nickname, iter->group, iter->friendly);
		if (ret != FE_SUCCESS)
			return(ret);
		iter->uploaded = 1;
	}

	if ((isonline_hack != NULL) && (firetalk_protocols[conn->protocol]->comparenicks(conn->handle, iter->nickname, isonline_hack) == FE_SUCCESS))
		firetalk_callback_im_buddyonline(conn->handle, iter->nickname, 1);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_add_deny(firetalk_connection_t *conn, const char *const nickname) {
	assert(firetalk_connection_t_valid(conn));

	if (firetalk_im_internal_add_deny(conn, nickname) == NULL)
		return(FE_UNKNOWN);

	if (conn->connected == FCS_ACTIVE)
		return(firetalk_protocols[conn->protocol]->im_add_deny(conn->handle, nickname));
	return(FE_SUCCESS);
}

fte_t	firetalk_im_remove_deny(firetalk_connection_t *conn, const char *const nickname) {
	fte_t	ret;

	assert(firetalk_connection_t_valid(conn));

	ret = firetalk_im_internal_remove_deny(conn,nickname);
	if (ret != FE_SUCCESS)
		return(ret);

	if (conn->connected == FCS_ACTIVE)
		return(firetalk_protocols[conn->protocol]->im_remove_deny(conn->handle, nickname));
	return(FE_SUCCESS);
}

fte_t	firetalk_im_send_message(firetalk_connection_t *conn, const char *const dest, const char *const message, const int auto_flag) {
	fte_t	e;

	assert(firetalk_connection_t_valid(conn));

	if ((conn->connected != FCS_ACTIVE) && (strcasecmp(dest, ":RAW") != 0))
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

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
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
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->get_info(conn->handle, nickname));
}

fte_t	firetalk_set_info(firetalk_connection_t *conn, const char *const info) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_info(conn->handle, info));
}

fte_t	firetalk_chat_listmembers(firetalk_connection_t *conn, const char *const roomname) {
	firetalk_room_t *room;
	firetalk_member_t *memberiter;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
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
	return(firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room));
}

fte_t	firetalk_set_away(firetalk_connection_t *conn, const char *const message, const int auto_flag) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_away(conn->handle, message, auto_flag));
}

fte_t	firetalk_set_nickname(firetalk_connection_t *conn, const char *const nickname) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_nickname(conn->handle, nickname));
}

fte_t	firetalk_set_password(firetalk_connection_t *conn, const char *const oldpass, const char *const newpass) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_password(conn->handle, oldpass, newpass));
}

fte_t	firetalk_set_privacy(firetalk_connection_t *conn, const char *const mode) {
	assert(firetalk_connection_t_valid(conn));

	assert(mode != NULL);

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->set_privacy(conn->handle, mode));
}

fte_t	firetalk_im_evil(firetalk_connection_t *conn, const char *const who) {
	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(firetalk_protocols[conn->protocol]->im_evil(conn->handle, who));
}

fte_t	firetalk_chat_join(firetalk_connection_t *conn, const char *const room) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_join(conn->handle, normalroom));
}

fte_t	firetalk_chat_part(firetalk_connection_t *conn, const char *const room) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_part(conn->handle, normalroom));
}

fte_t	firetalk_chat_send_message(firetalk_connection_t *conn, const char *const room, const char *const message, const int auto_flag) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	if (*room == ':')
		normalroom = room;
	else
		normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_send_message(conn->handle, normalroom, message, auto_flag));
}

fte_t	firetalk_chat_send_action(firetalk_connection_t *conn, const char *const room, const char *const message, const int auto_flag) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_send_action(conn->handle, normalroom, message, auto_flag));
}

fte_t	firetalk_chat_invite(firetalk_connection_t *conn, const char *const room, const char *const who, const char *const message) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_invite(conn->handle, normalroom, who, message));
}

fte_t	firetalk_chat_set_topic(firetalk_connection_t *conn, const char *const room, const char *const topic) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_set_topic(conn->handle, normalroom, topic));
}

fte_t	firetalk_chat_op(firetalk_connection_t *conn, const char *const room, const char *const who) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_op(conn->handle, normalroom, who));
}

fte_t	firetalk_chat_deop(firetalk_connection_t *conn, const char *const room, const char *const who) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_deop(conn->handle, normalroom, who));
}

fte_t	firetalk_chat_kick(firetalk_connection_t *conn, const char *const room, const char *const who, const char *const reason) {
	const char *normalroom;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = firetalk_protocols[conn->protocol]->room_normalize(conn->handle, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(firetalk_protocols[conn->protocol]->chat_kick(conn->handle, normalroom, who, reason));
}

fte_t	firetalk_subcode_send_request(firetalk_connection_t *conn, const char *const to, const char *const command, const char *const args) {
	char *sc;

	assert(firetalk_connection_t_valid(conn));

	if (conn->connected != FCS_ACTIVE && (*to != ':'))
		return(FE_NOTCONNECTED);

	sc = firetalk_protocols[conn->protocol]->subcode_encode(conn->handle, command, args);
	if (!sc)
		return(FE_SUCCESS);

	firetalk_enqueue(&conn->subcode_requests, to, sc);
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_send_reply(firetalk_connection_t *conn, const char *const to, const char *const command, const char *const args) {
	char *sc;

	assert(firetalk_connection_t_valid(conn));

	if ((conn->connected != FCS_ACTIVE) && (*to != ':'))
		return(FE_NOTCONNECTED);
	
	sc = firetalk_protocols[conn->protocol]->subcode_encode(conn->handle, command, args);
	if (!sc)
		return(FE_SUCCESS);

	firetalk_enqueue(&conn->subcode_replies, to, sc);
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_request_callback(firetalk_connection_t *conn, const char *const command, void (*callback)(firetalk_connection_t *, struct firetalk_useragent_connection_t *, const char *const, const char *const, const char *const)) {
	assert(firetalk_connection_t_valid(conn));

	if (command == NULL) {
		if (conn->subcode_request_default != NULL)
			firetalk_subcode_callback_t_delete(conn->subcode_request_default);
		if ((conn->subcode_request_default = firetalk_subcode_callback_t_new()) == NULL)
			abort();
		conn->subcode_request_default->callback = (ptrtofnct)callback;
	} else {
		firetalk_subcode_callback_t *iter;

		if ((iter = firetalk_subcode_callback_t_new()) == NULL)
			abort();
		iter->next = conn->subcode_request_head;
		conn->subcode_request_head = iter;
		STRREPLACE(iter->command, command);
		iter->callback = (ptrtofnct)callback;
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_request_reply(firetalk_connection_t *conn, const char *const command, const char *const reply) {
	assert(firetalk_connection_t_valid(conn));

	if (command == NULL) {
		if (conn->subcode_request_default != NULL)
			firetalk_subcode_callback_t_delete(conn->subcode_request_default);
		if ((conn->subcode_request_default = firetalk_subcode_callback_t_new()) == NULL)
			abort();
		STRREPLACE(conn->subcode_request_default->staticresp, reply);
	} else {
		firetalk_subcode_callback_t *iter;

		if ((iter = firetalk_subcode_callback_t_new()) == NULL)
			abort();
		iter->next = conn->subcode_request_head;
		conn->subcode_request_head = iter;
		STRREPLACE(iter->command, command);
		STRREPLACE(iter->staticresp, reply);
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_reply_callback(firetalk_connection_t *conn, const char *const command, void (*callback)(firetalk_connection_t *, struct firetalk_useragent_connection_t *, const char *const, const char *const, const char *const)) {
	assert(firetalk_connection_t_valid(conn));

	if (command == NULL) {
		if (conn->subcode_reply_default)
			firetalk_subcode_callback_t_delete(conn->subcode_reply_default);
		if ((conn->subcode_reply_default = firetalk_subcode_callback_t_new()) == NULL)
			abort();
		conn->subcode_reply_default->callback = (ptrtofnct)callback;
	} else {
		firetalk_subcode_callback_t *iter;

		if ((iter = firetalk_subcode_callback_t_new()) == NULL)
			abort();
		iter->next = conn->subcode_reply_head;
		conn->subcode_reply_head = iter;
		STRREPLACE(iter->command, command);
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

	assert(firetalk_connection_t_valid(conn));

	if ((iter = firetalk_transfer_t_new()) == NULL)
		abort();
	iter->next = conn->file_head;
	conn->file_head = iter;
	STRREPLACE(iter->who, nickname);
	STRREPLACE(iter->filename, filename);
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

	assert(firetalk_connection_t_valid(conn));

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

	assert(firetalk_connection_t_valid(conn));

	prev = NULL;
	for (fileiter = conn->file_head; fileiter != NULL; fileiter = fileiter->next) {
		if (fileiter == filehandle) {
			if (prev != NULL)
				prev->next = fileiter->next;
			else
				conn->file_head = fileiter->next;

			firetalk_transfer_t_delete(fileiter);
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
	assert(firetalk_connection_t_valid(conn));

	if ((nick1 == NULL) || (nick2 == NULL))
		return(FE_NOMATCH);

	return(firetalk_protocols[conn->protocol]->comparenicks(conn->handle, nick1, nick2));
}

fte_t	firetalk_isprint(firetalk_connection_t *conn, const int c) {
	assert(firetalk_connection_t_valid(conn));

	return(firetalk_protocols[conn->protocol]->isprintable(conn->handle, c));
}

fte_t	firetalk_select(void) {
	return(firetalk_select_custom(0, NULL, NULL, NULL, NULL));
}

fte_t	firetalk_select_custom(int n, fd_set *fd_read, fd_set *fd_write, fd_set *fd_except, struct timeval *timeout) {
	fte_t	ret;
	fd_set *my_read, *my_write, *my_except;
	fd_set internal_read, internal_write, internal_except;
	struct timeval internal_timeout, *my_timeout;
	firetalk_connection_t *conn;

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

	if (my_timeout->tv_sec > 50)
		my_timeout->tv_sec = 50;

	/* internal preselect */
	for (conn = conn_head; conn != NULL; conn = conn->next) {
		firetalk_transfer_t *fileiter;

		if (conn->deleted)
			continue;

		for (fileiter = conn->file_head; fileiter != NULL; fileiter = fileiter->next) {
			if (fileiter->sock.fd >= 0)
				FD_SET(fileiter->sock.fd, my_except);
			switch (fileiter->state) {
			  case FF_STATE_TRANSFERRING:
				switch (fileiter->direction) {
				  case FF_DIRECTION_SENDING:
					assert(fileiter->sock.fd >= 0);
					if (fileiter->sock.fd >= n)
						n = fileiter->sock.fd + 1;
					FD_SET(fileiter->sock.fd, my_write);
					break;
				  case FF_DIRECTION_RECEIVING:
					assert(fileiter->sock.fd >= 0);
					if (fileiter->sock.fd >= n)
						n = fileiter->sock.fd + 1;
					FD_SET(fileiter->sock.fd, my_read);
					break;
				}
				break;
			  case FF_STATE_WAITREMOTE:
				assert(fileiter->sock.fd >= 0);
				if (fileiter->sock.fd >= n)
					n = fileiter->sock.fd + 1;
				FD_SET(fileiter->sock.fd, my_read);
				break;
			  case FF_STATE_WAITSYNACK:
				assert(fileiter->sock.fd >= 0);
				if (fileiter->sock.fd >= n)
					n = fileiter->sock.fd + 1;
				FD_SET(fileiter->sock.fd, my_write);
				break;
			}
		}

		if (conn->connected == FCS_NOTCONNECTED)
			continue;

		while (conn->subcode_requests.count > 0) {
			int	count = conn->subcode_requests.count;
			char	*key = strdup(conn->subcode_requests.keys[0]);

			firetalk_protocols[conn->protocol]->im_send_message(conn->handle, key, "", 0);
			free(key);
			assert(conn->subcode_requests.count < count);
		}

		while (conn->subcode_replies.count > 0) {
			int	count = conn->subcode_replies.count;
			char	*key = strdup(conn->subcode_replies.keys[0]);

			firetalk_protocols[conn->protocol]->im_send_message(conn->handle, key, "", 1);
			free(key);
			assert(conn->subcode_replies.count < count);
		}

		firetalk_protocols[conn->protocol]->periodic(conn);
	}

	/* per-protocol preselect, UI prepoll */
	for (conn = conn_head; conn != NULL; conn = conn->next) {
		if (conn->deleted)
			continue;
		firetalk_protocols[conn->protocol]->preselect(conn->handle, my_read, my_write, my_except, &n);
		if (conn->callbacks[FC_PRESELECT])
			conn->callbacks[FC_PRESELECT](conn, conn->clientstruct);
	}

	/* select */
	if (n > 0) {
		ret = select(n, my_read, my_write, my_except, my_timeout);
		if (ret == -1)
			return(FE_PACKET);
	}

	/* per-protocol postselect, UI postpoll */
	for (conn = conn_head; conn != NULL; conn = conn->next) {
		if (conn->deleted)
			continue;

		firetalk_protocols[conn->protocol]->postselect(conn->handle, my_read, my_write, my_except);
		if (conn->callbacks[FC_POSTSELECT])
			conn->callbacks[FC_POSTSELECT](conn, conn->clientstruct);
	}

	/* internal postpoll */
	for (conn = conn_head; conn != NULL; conn = conn->next) {
		firetalk_transfer_t *fileiter, *filenext;
		firetalk_sock_state_t state;
		fte_t	ret;

		if (conn->deleted)
			continue;

		for (fileiter = conn->file_head; fileiter != NULL; fileiter = filenext) {
			filenext = fileiter->next;
			if ((fileiter->sock.fd >= 0) && FD_ISSET(fileiter->sock.fd, my_except)) {
				if (conn->callbacks[FC_FILE_ERROR])
					conn->callbacks[FC_FILE_ERROR](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct, FE_IOERROR);
				firetalk_file_cancel(conn, fileiter);
				continue;
			}
			switch (fileiter->state) {
			  case FF_STATE_TRANSFERRING:
				assert(fileiter->sock.fd >= 0);
				if (FD_ISSET(fileiter->sock.fd, my_write))
					firetalk_handle_send(conn, fileiter);
				else if (FD_ISSET(fileiter->sock.fd, my_read))
					firetalk_handle_receive(conn, fileiter);
				break;
			  case FF_STATE_WAITREMOTE:
				assert(fileiter->sock.fd >= 0);
				if (FD_ISSET(fileiter->sock.fd, my_read)) {
					struct sockaddr_in addr;
					unsigned int l = sizeof(addr);
					int	s;

					s = accept(fileiter->sock.fd, (struct sockaddr *)&addr, &l);
					if (s == -1) {
						if (conn->callbacks[FC_FILE_ERROR])
							conn->callbacks[FC_FILE_ERROR](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct, FE_SOCKET);
						firetalk_file_cancel(conn, fileiter);
					} else {
						close(fileiter->sock.fd);
						fileiter->sock.fd = s;
						fileiter->sock.state = FCS_ACTIVE;
						fileiter->state = FF_STATE_TRANSFERRING;
						if (conn->callbacks[FC_FILE_START])
							conn->callbacks[FC_FILE_START](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct);
					}
				}
				break;
			  case FF_STATE_WAITSYNACK:
				assert(fileiter->sock.fd >= 0);
				if (FD_ISSET(fileiter->sock.fd, my_write))
					firetalk_handle_file_synack(conn, fileiter);
				break;
			}
		}
	}

	/* handle deleted connections */
	{
		firetalk_connection_t *connprev, *connnext;

		connprev = NULL;
		for (conn = conn_head; conn != NULL; conn = connnext) {
			connnext = conn->next;
			if (conn->deleted) {
				assert(conn->handle == NULL);

				if (connprev == NULL) {
					assert(conn == conn_head);
					conn_head = connnext;
				} else {
					assert(conn != conn_head);
					connprev->next = connnext;
				}

				firetalk_connection_t_delete(conn);
			} else
				connprev = conn;
		}
	}

	return(FE_SUCCESS);
}
