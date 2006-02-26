/*
firetalk.c - FireTalk wrapper definitions
Copyright (C) 2000 Ian Gulliver
Copyright 2002-2006 Daniel Reed <n@ml.org>

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

typedef void (*ptrtotoc)(void *, ...);
typedef void (*sighandler_t)(int);

/* Global variables */
fte_t	firetalkerror = FE_SUCCESS;
static struct s_firetalk_handle *handle_head = NULL;

static const firetalk_PD_t **firetalk_protocols = NULL;
static int FP_MAX = 0;

fte_t	firetalk_register_protocol(const firetalk_PD_t *const proto) {
	const firetalk_PD_t **ptr;

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
	extern const firetalk_PD_t
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

/* Internal function definitions */

#ifndef NDEBUG
# define VERIFYCONN \
	do { \
		if (firetalk_check_handle(conn) != FE_SUCCESS) \
			abort(); \
	} while(0)

static fte_t firetalk_check_handle(firetalk_t c) {
	struct s_firetalk_handle *iter;

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

	for (o = s = 0; (s <= (len - 3)) && (o < (sizeof(out)-1)); s += 4, o += 3) {
		out[o]   = (firetalk_debase64_char(str[s])   << 2) | (firetalk_debase64_char(str[s+1]) >> 4);
		out[o+1] = (firetalk_debase64_char(str[s+1]) << 4) | (firetalk_debase64_char(str[s+2]) >> 2);
		out[o+2] = (firetalk_debase64_char(str[s+2]) << 6) |  firetalk_debase64_char(str[s+3]);
	}
	out[o] = 0;
	return((char *)out);
}

static void firetalk_callback_im_buddyonline(firetalk_t conn, client_t c, const char *const nickname, int online);

fte_t	firetalk_im_internal_add_deny(firetalk_t conn, const char *const nickname) {
	struct s_firetalk_deny *iter;

	VERIFYCONN;

	for (iter = conn->deny_head; iter != NULL; iter = iter->next)
		if (conn->PD->comparenicks(conn, iter->nickname, nickname) == FE_SUCCESS)
			return(FE_DUPEUSER); /* not an error, user is in buddy list */

	iter = conn->deny_head;
	conn->deny_head = calloc(1, sizeof(struct s_firetalk_deny));
	if (conn->deny_head == NULL)
		abort();
	conn->deny_head->next = iter;
	conn->deny_head->nickname = strdup(nickname);
	if (conn->deny_head->nickname == NULL)
		abort();

	firetalk_callback_im_buddyonline(conn, conn->handle, nickname, 0);

	return(FE_SUCCESS);
}

fte_t	firetalk_user_visible(firetalk_t conn, const char *const nickname) {
	struct s_firetalk_room *iter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		struct s_firetalk_member *mem;

		for (mem = iter->member_head; mem != NULL; mem = mem->next)
			if (conn->PD->comparenicks(conn, mem->nickname, nickname) == FE_SUCCESS)
				return(FE_SUCCESS);
	}
	return(FE_NOMATCH);
}

fte_t	firetalk_user_visible_but(firetalk_t conn, const char *const room, const char *const nickname) {
	struct s_firetalk_room *iter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		struct s_firetalk_member *mem;

		if (conn->PD->comparenicks(conn, iter->name, room) == FE_SUCCESS)
			continue;
		for (mem = iter->member_head; mem != NULL; mem = mem->next)
			if (conn->PD->comparenicks(conn, mem->nickname, nickname) == FE_SUCCESS)
				return(FE_SUCCESS);
	}
	return(FE_NOMATCH);
}

fte_t	firetalk_chat_internal_add_room(firetalk_t conn, const char *const name) {
	struct s_firetalk_room *iter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (conn->PD->comparenicks(conn, iter->name, name) == FE_SUCCESS)
			return(FE_DUPEROOM); /* not an error, we're already in room */

	iter = conn->room_head;
	conn->room_head = calloc(1, sizeof(struct s_firetalk_room));
	if (conn->room_head == NULL)
		abort();
	conn->room_head->next = iter;
	conn->room_head->name = strdup(name);
	if (conn->room_head->name == NULL)
		abort();

	return(FE_SUCCESS);
}

fte_t	firetalk_chat_internal_add_member(firetalk_t conn, const char *const room, const char *const nickname) {
	struct s_firetalk_room *iter;
	struct s_firetalk_member *memberiter;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (conn->PD->comparenicks(conn, iter->name, room) == FE_SUCCESS)
			break;

	if (iter == NULL) /* we don't know about that room */
		return(FE_NOTFOUND);

	for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiter->next)
		if (conn->PD->comparenicks(conn, memberiter->nickname, nickname) == FE_SUCCESS)
			return(FE_SUCCESS);

	memberiter = iter->member_head;
	iter->member_head = calloc(1, sizeof(struct s_firetalk_member));
	if (iter->member_head == NULL)
		abort();
	iter->member_head->next = memberiter;
	iter->member_head->nickname = strdup(nickname);
	if (iter->member_head->nickname == NULL)
		abort();

	return(FE_SUCCESS);
}

static void firetalk_im_delete_buddy(firetalk_t conn, const char *const nickname) {
	struct s_firetalk_buddy *iter, *prev;

	for (prev = NULL, iter = conn->buddy_head; iter != NULL; prev = iter, iter = iter->next) {
		assert(iter->nickname != NULL);
		assert(iter->group != NULL);

		if (conn->PD->comparenicks(conn, nickname, iter->nickname) == FE_SUCCESS)
			break;
	}
	if (iter == NULL)
		return;

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

static struct s_firetalk_buddy *firetalk_im_find_buddy(firetalk_t conn, const char *const name) {
	struct s_firetalk_buddy *iter;

	for (iter = conn->buddy_head; iter != NULL; iter = iter->next) {
		assert(iter->nickname != NULL);
		assert(iter->group != NULL);

		if (conn->PD->comparenicks(conn, iter->nickname, name) == FE_SUCCESS)
			return(iter);
	}
	return(NULL);
}

fte_t	firetalk_im_remove_buddy(firetalk_t conn, const char *const name) {
	struct s_firetalk_buddy *iter;

	VERIFYCONN;

	if ((iter = firetalk_im_find_buddy(conn, name)) == NULL)
		return(FE_NOTFOUND);

	if (conn->connected != FCS_NOTCONNECTED) {
		int	ret;

		ret = conn->PD->im_remove_buddy(conn, conn->handle, iter->nickname, iter->group);
		if (ret != FE_SUCCESS)
			return(ret);
	}

	firetalk_im_delete_buddy(conn, name);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_internal_remove_deny(firetalk_t conn, const char *const nickname) {
	struct s_firetalk_deny *iter;
	struct s_firetalk_deny *prev;

	VERIFYCONN;

	prev = NULL;
	for (iter = conn->deny_head; iter != NULL; iter = iter->next) {
		if (conn->PD->comparenicks(conn, nickname, iter->nickname) == FE_SUCCESS) {
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

fte_t	firetalk_chat_internal_remove_room(firetalk_t conn, const char *const name) {
	struct s_firetalk_room *iter;
	struct s_firetalk_room *prev;
	struct s_firetalk_member *memberiter;
	struct s_firetalk_member *membernext;

	VERIFYCONN;

	prev = NULL;
	for (iter = conn->room_head; iter != NULL; iter = iter->next) {
		if (conn->PD->comparenicks(conn, name, iter->name) == FE_SUCCESS) {
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

fte_t	firetalk_chat_internal_remove_member(firetalk_t conn, const char *const room, const char *const nickname) {
	struct s_firetalk_room *iter;
	struct s_firetalk_member *memberiter;
	struct s_firetalk_member *memberprev;

	VERIFYCONN;

	for (iter = conn->room_head; iter != NULL; iter = iter->next)
		if (conn->PD->comparenicks(conn, iter->name, room) == FE_SUCCESS)
			break;

	if (iter == NULL) /* we don't know about that room */
		return(FE_NOTFOUND);

	memberprev = NULL;
	for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiter->next) {
		if (conn->PD->comparenicks(conn, memberiter->nickname, nickname) == FE_SUCCESS) {
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

struct s_firetalk_room *firetalk_find_room(firetalk_t conn, const char *const room) {
	struct s_firetalk_room *roomiter;
	const char *normalroom;

	normalroom = conn->PD->room_normalize(conn, room);
	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		if (conn->PD->comparenicks(conn, roomiter->name, normalroom) == FE_SUCCESS)
			return(roomiter);

	firetalkerror = FE_NOTFOUND;
	return(NULL);
}

static struct s_firetalk_member *firetalk_find_member(firetalk_t conn, struct s_firetalk_room *r, const char *const name) {
	struct s_firetalk_member *memberiter;

	for (memberiter = r->member_head; memberiter != NULL; memberiter = memberiter->next)
		if (conn->PD->comparenicks(conn, memberiter->nickname, name) == FE_SUCCESS)
			return(memberiter);

	firetalkerror = FE_NOTFOUND;
	return(NULL);
}

static void firetalk_callback_needpass(firetalk_t conn, client_t c, char *pass, const int size) {
	if (conn->UA[FC_NEEDPASS])
		conn->UA[FC_NEEDPASS](conn, conn->clientstruct, pass, size);
}

static const char *isonline_hack = NULL;

static void firetalk_callback_im_getmessage(firetalk_t conn, client_t c, const char *const sender, const int automessage, const char *const message) {
	struct s_firetalk_deny *iter;

	if (strstr(message, "<a href=\"http://www.candidclicks.com/cgi-bin/enter.cgi?") != NULL) {
		firetalk_im_evil(conn, sender);
		return;
	}
	if (conn->UA[FC_IM_GETMESSAGE]) {
		for (iter = conn->deny_head; iter != NULL; iter = iter->next)
			if (conn->PD->comparenicks(conn, sender, iter->nickname) == FE_SUCCESS)
				return;
		isonline_hack = sender;
		conn->UA[FC_IM_GETMESSAGE](conn, conn->clientstruct, sender, automessage, message);
		isonline_hack = NULL;
	}
}

static void firetalk_callback_im_getaction(firetalk_t conn, client_t c, const char *const sender, const int automessage, const char *const message) {
	struct s_firetalk_deny *iter;

	if (conn->UA[FC_IM_GETACTION]) {
		for (iter = conn->deny_head; iter != NULL; iter = iter->next)
			if (conn->PD->comparenicks(conn, sender, iter->nickname) == FE_SUCCESS)
				return;
		isonline_hack = sender;
		conn->UA[FC_IM_GETACTION](conn, conn->clientstruct, sender, automessage, message);
		isonline_hack = NULL;
	}
}

static void firetalk_callback_im_buddyonline(firetalk_t conn, client_t c, const char *const nickname, int online) {
	struct s_firetalk_buddy *buddyiter;

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
				if (conn->UA[FC_IM_BUDDYONLINE] != NULL)
					conn->UA[FC_IM_BUDDYONLINE](conn, conn->clientstruct, nickname);
			} else {
				buddyiter->away = 0;
				buddyiter->typing = 0;
				buddyiter->warnval = 0;
				buddyiter->idletime = 0;
				if (conn->UA[FC_IM_BUDDYOFFLINE] != NULL)
					conn->UA[FC_IM_BUDDYOFFLINE](conn, conn->clientstruct, nickname);
			}
		}
}

static void firetalk_callback_im_buddyaway(firetalk_t conn, client_t c, const char *const nickname, const int away) {
	struct s_firetalk_buddy *buddyiter;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->away != away) && (buddyiter->online == 1)) {
			buddyiter->away = away;
			if ((away == 1) && (conn->UA[FC_IM_BUDDYAWAY] != NULL))
				conn->UA[FC_IM_BUDDYAWAY](conn, conn->clientstruct, nickname);
			else if ((away == 0) && (conn->UA[FC_IM_BUDDYUNAWAY] != NULL))
				conn->UA[FC_IM_BUDDYUNAWAY](conn, conn->clientstruct, nickname);
		}
}

static void firetalk_im_insert_buddy(firetalk_t conn, const char *const name, const char *const group, const char *const friendly) {
	struct s_firetalk_buddy *newiter;

	newiter = calloc(1, sizeof(*newiter));
	if (newiter == NULL)
		abort();
	newiter->next = conn->buddy_head;
	conn->buddy_head = newiter;
	newiter->nickname = strdup(name);
	if (newiter->nickname == NULL)
		abort();
	newiter->group = strdup(group);
	if (newiter->group == NULL)
		abort();
	if (friendly == NULL)
		newiter->friendly = NULL;
	else {
		newiter->friendly = strdup(friendly);
		if (newiter->friendly == NULL)
			abort();
	}
}

static void firetalk_callback_buddyadded(firetalk_t conn, client_t c, const char *const name, const char *const group, const char *const friendly) {
	if (firetalk_im_find_buddy(conn, name) == NULL) {
		firetalk_im_insert_buddy(conn, name, group, friendly);
		if (conn->UA[FC_IM_BUDDYADDED] != NULL)
			conn->UA[FC_IM_BUDDYADDED](conn, conn->clientstruct, name, group, friendly);
	}
}

static void firetalk_callback_buddyremoved(firetalk_t conn, client_t c, const char *const name, const char *const group) {
	struct s_firetalk_buddy *iter;

	if (((iter = firetalk_im_find_buddy(conn, name)) != NULL) && ((group == NULL) || (strcmp(iter->group, group) == 0))) {
		firetalk_im_delete_buddy(conn, name);
		if (conn->UA[FC_IM_BUDDYREMOVED] != NULL)
			conn->UA[FC_IM_BUDDYREMOVED](conn, conn->clientstruct, name);
	}
}

static void firetalk_callback_typing(firetalk_t conn, client_t c, const char *const name, const int typing) {
	struct s_firetalk_buddy *buddyiter;

	assert(name != NULL);
	assert(typing >= 0);

	if (!conn->UA[FC_IM_TYPINGINFO])
		return;

	if ((buddyiter = firetalk_im_find_buddy(conn, name)) != NULL) {
		assert(buddyiter->online != 0);
		if (buddyiter->typing != typing) {
			buddyiter->typing = typing;
			conn->UA[FC_IM_TYPINGINFO](conn, conn->clientstruct, buddyiter->nickname, typing);
		}
	}
}

static void firetalk_callback_capabilities(firetalk_t conn, client_t c, char const *const nickname, const char *const caps) {
	struct s_firetalk_buddy *buddyiter;

	if (!conn->UA[FC_IM_CAPABILITIES])
		return;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->capabilities == NULL) || (strcmp(buddyiter->capabilities, caps) != 0)) {
			free(buddyiter->capabilities);
			buddyiter->capabilities = strdup(caps);
			conn->UA[FC_IM_CAPABILITIES](conn, conn->clientstruct, nickname, caps);
		}
}

static void firetalk_callback_warninfo(firetalk_t conn, client_t c, char const *const nickname, const long warnval) {
	struct s_firetalk_buddy *buddyiter;

	if (!conn->UA[FC_IM_EVILINFO])
		return;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->warnval != warnval) && (buddyiter->online == 1)) {
			buddyiter->warnval = warnval;
			conn->UA[FC_IM_EVILINFO](conn, conn->clientstruct, nickname, warnval);
		}
}

static void firetalk_callback_error(firetalk_t conn, client_t c, const int error, const char *const roomoruser, const char *const description) {
	if (conn->UA[FC_ERROR])
		conn->UA[FC_ERROR](conn, conn->clientstruct, error, roomoruser, description);
}

static void firetalk_callback_connectfailed(firetalk_t conn, client_t c, const int error, const char *const description) {
	if (conn->connected == FCS_NOTCONNECTED)
		return;

	conn->connected = FCS_NOTCONNECTED;
	if (conn->UA[FC_CONNECTFAILED])
		conn->UA[FC_CONNECTFAILED](conn, conn->clientstruct, error, description);
}

static void firetalk_callback_disconnect(firetalk_t conn, client_t c, const int error) {
	struct s_firetalk_buddy *buddyiter, *buddynext;
	struct s_firetalk_deny *denyiter, *denynext;
	struct s_firetalk_room *roomiter, *roomnext;

	if (conn->connected == FCS_NOTCONNECTED)
		return;

	for (buddyiter = conn->buddy_head; buddyiter != NULL; buddyiter = buddynext) {
		buddynext = buddyiter->next;
		buddyiter->next = NULL;
		free(buddyiter->nickname);
		buddyiter->nickname = NULL;
		free(buddyiter->group);
		buddyiter->group = NULL;
		free(buddyiter->friendly);
		buddyiter->friendly = NULL;
		if (buddyiter->capabilities != NULL) {
			free(buddyiter->capabilities);
			buddyiter->capabilities = NULL;
		}
		free(buddyiter);
	}
	conn->buddy_head = NULL;

	for (denyiter = conn->deny_head; denyiter != NULL; denyiter = denynext) {
		denynext = denyiter->next;
		denyiter->next = NULL;
		free(denyiter->nickname);
		denyiter->nickname = NULL;
		free(denyiter);
	}
	conn->deny_head = NULL;

	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomnext) {
		struct s_firetalk_member *memberiter;
		struct s_firetalk_member *membernext;

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

	if (conn->connected == FCS_ACTIVE) {
		conn->connected = FCS_NOTCONNECTED;
		if (conn->UA[FC_DISCONNECT])
			conn->UA[FC_DISCONNECT](conn, conn->clientstruct, error);
	} else
		conn->connected = FCS_NOTCONNECTED;
}

static void firetalk_callback_gotinfo(firetalk_t conn, client_t c, const char *const nickname, const char *const info, const int warning, const long online, const long idle, const int flags) {
	if (conn->UA[FC_IM_GOTINFO])
		conn->UA[FC_IM_GOTINFO](conn, conn->clientstruct, nickname, info, warning, online, idle, flags);
}

static void firetalk_callback_idleinfo(firetalk_t conn, client_t c, char const *const nickname, const long idletime) {
	struct s_firetalk_buddy *buddyiter;

	if (!conn->UA[FC_IM_IDLEINFO])
		return;

	if ((buddyiter = firetalk_im_find_buddy(conn, nickname)) != NULL)
		if ((buddyiter->idletime != idletime) && (buddyiter->online == 1)) {
			buddyiter->idletime = idletime;
			conn->UA[FC_IM_IDLEINFO](conn, conn->clientstruct, nickname, idletime);
		}
}

static void firetalk_callback_doinit(firetalk_t conn, client_t c, const char *const nickname) {
	if (conn->UA[FC_DOINIT])
		conn->UA[FC_DOINIT](conn, conn->clientstruct, nickname);
}

static void firetalk_callback_setidle(firetalk_t conn, client_t c, long *const idle) {
	if (conn->UA[FC_SETIDLE])
		conn->UA[FC_SETIDLE](conn, conn->clientstruct, idle);
}

static void firetalk_callback_eviled(firetalk_t conn, client_t c, const int newevil, const char *const eviler) {
	if (conn->UA[FC_EVILED])
		conn->UA[FC_EVILED](conn, conn->clientstruct, newevil, eviler);
}

static void firetalk_callback_newnick(firetalk_t conn, client_t c, const char *const nickname) {
	if (conn->UA[FC_NEWNICK])
		conn->UA[FC_NEWNICK](conn, conn->clientstruct, nickname);
}

static void firetalk_callback_passchanged(firetalk_t conn, client_t c) {
	if (conn->UA[FC_PASSCHANGED])
		conn->UA[FC_PASSCHANGED](conn, conn->clientstruct);
}

static void firetalk_callback_user_nickchanged(firetalk_t conn, client_t c, const char *const oldnick, const char *const newnick) {
	struct s_firetalk_buddy *buddyiter;
	struct s_firetalk_room *roomiter;
	struct s_firetalk_member *memberiter;
	char *tempstr;

	if ((buddyiter = firetalk_im_find_buddy(conn, oldnick)) != NULL)
		if (strcmp(buddyiter->nickname, newnick) != 0) {
			tempstr = buddyiter->nickname;
			buddyiter->nickname = strdup(newnick);
			if (buddyiter->nickname == NULL)
				abort();
			if (conn->UA[FC_IM_USER_NICKCHANGED])
				conn->UA[FC_IM_USER_NICKCHANGED](conn, conn->clientstruct, tempstr, newnick);
			free(tempstr);
		}

	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		for (memberiter = roomiter->member_head; memberiter != NULL; memberiter = memberiter->next)
			if (conn->PD->comparenicks(conn, memberiter->nickname, oldnick) == FE_SUCCESS) {
				if (strcmp(memberiter->nickname, newnick) != 0) {
					tempstr = memberiter->nickname;
					memberiter->nickname = strdup(newnick);
					if (memberiter->nickname == NULL)
						abort();
					if (conn->UA[FC_CHAT_USER_NICKCHANGED])
						conn->UA[FC_CHAT_USER_NICKCHANGED](conn, conn->clientstruct, roomiter->name, tempstr, newnick);
					free(tempstr);
				}
				break;
			}
}

static void firetalk_callback_chat_joined(firetalk_t conn, client_t c, const char *const room) {
	if (firetalk_chat_internal_add_room(conn, room) != FE_SUCCESS)
		return;
}

static void firetalk_callback_chat_left(firetalk_t conn, client_t c, const char *const room) {
	if (firetalk_chat_internal_remove_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->UA[FC_CHAT_LEFT])
		conn->UA[FC_CHAT_LEFT](conn, conn->clientstruct, room);
}

static void firetalk_callback_chat_kicked(firetalk_t conn, client_t c, const char *const room, const char *const by, const char *const reason) {
	if (firetalk_chat_internal_remove_room(conn, room) != FE_SUCCESS)
		return;
	if (conn->UA[FC_CHAT_KICKED])
		conn->UA[FC_CHAT_KICKED](conn, conn->clientstruct, room, by, reason);
}

static void firetalk_callback_chat_getmessage(firetalk_t conn, client_t c, const char *const room, const char *const from, const int automessage, const char *const message) {
	if (conn->UA[FC_CHAT_GETMESSAGE])
		conn->UA[FC_CHAT_GETMESSAGE](conn, conn->clientstruct, room, from, automessage, message);
}

static void firetalk_callback_chat_getaction(firetalk_t conn, client_t c, const char *const room, const char *const from, const int automessage, const char *const message) {
	if (conn->UA[FC_CHAT_GETACTION])
		conn->UA[FC_CHAT_GETACTION](conn, conn->clientstruct, room, from, automessage, message);
}

static void firetalk_callback_chat_invited(firetalk_t conn, client_t c, const char *const room, const char *const from, const char *const message) {
	if (conn->UA[FC_CHAT_INVITED])
		conn->UA[FC_CHAT_INVITED](conn, conn->clientstruct, room, from, message);
}

static void firetalk_callback_chat_user_joined(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const extra) {
	struct s_firetalk_room *iter;

	iter = firetalk_find_room(conn, room);
	if (iter == NULL)
		return;

	if (who == NULL) {
		if (iter->sentjoin == 0) {
			iter->sentjoin = 1;
			if (conn->UA[FC_CHAT_JOINED])
				conn->UA[FC_CHAT_JOINED](conn, conn->clientstruct, room);
		}
	} else {
		if (firetalk_chat_internal_add_member(conn, room, who) != FE_SUCCESS)
			return;
		if (iter->sentjoin == 1)
			if (conn->UA[FC_CHAT_USER_JOINED])
				conn->UA[FC_CHAT_USER_JOINED](conn, conn->clientstruct, room, who, extra);
	}
}

static void firetalk_callback_chat_user_left(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const reason) {
	if (firetalk_chat_internal_remove_member(conn, room, who) != FE_SUCCESS)
		return;
	if (conn->UA[FC_CHAT_USER_LEFT])
		conn->UA[FC_CHAT_USER_LEFT](conn, conn->clientstruct, room, who, reason);
}

static void firetalk_callback_chat_user_quit(firetalk_t conn, client_t c, const char *const who, const char *const reason) {
	struct s_firetalk_room *roomiter;
	struct s_firetalk_member *memberiter, *membernext;
	
	for (roomiter = conn->room_head; roomiter != NULL; roomiter = roomiter->next)
		for (memberiter = roomiter->member_head; memberiter != NULL; memberiter = membernext) {
			membernext = memberiter->next;
			if (conn->PD->comparenicks(conn, memberiter->nickname, who) == FE_SUCCESS)
				firetalk_callback_chat_user_left(conn, c, roomiter->name, who, reason);
		}
}

static void firetalk_callback_chat_gottopic(firetalk_t conn, client_t c, const char *const room, const char *const topic, const char *const author) {
	struct s_firetalk_room *r;

	r = firetalk_find_room(conn, room);
	if (r != NULL)
		if (conn->UA[FC_CHAT_GOTTOPIC])
			conn->UA[FC_CHAT_GOTTOPIC](conn, conn->clientstruct, room, topic, author);
}

#ifdef RAWIRCMODES
static void firetalk_callback_chat_modechanged(firetalk_t conn, client_t c, const char *const room, const char *const mode, const char *const by) {
	if (conn->UA[FC_CHAT_MODECHANGED])
		conn->UA[FC_CHAT_MODECHANGED](conn, conn->clientstruct, room, mode, by);
}
#endif

static void firetalk_callback_chat_user_opped(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by) {
	struct s_firetalk_room *r;
	struct s_firetalk_member *m;

	r = firetalk_find_room(conn, room);
	if (r == NULL)
		return;
	m = firetalk_find_member(conn, r, who);
	if (m == NULL)
		return;
	if (m->admin == 0) {
		m->admin = 1;
		if (conn->UA[FC_CHAT_USER_OPPED])
			conn->UA[FC_CHAT_USER_OPPED](conn, conn->clientstruct, room, who, by);
	}
}

static void firetalk_callback_chat_user_deopped(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by) {
	struct s_firetalk_room *r;
	struct s_firetalk_member *m;

	r = firetalk_find_room(conn, room);
	if (r == NULL)
		return;
	m = firetalk_find_member(conn, r, who);
	if (m == NULL)
		return;
	if (m->admin == 1) {
		m->admin = 0;
		if (conn->UA[FC_CHAT_USER_DEOPPED])
			conn->UA[FC_CHAT_USER_DEOPPED](conn, conn->clientstruct, room, who, by);
	}
}

static void firetalk_callback_chat_keychanged(firetalk_t conn, client_t c, const char *const room, const char *const what, const char *const by) {
	if (conn->UA[FC_CHAT_KEYCHANGED])
		conn->UA[FC_CHAT_KEYCHANGED](conn, conn->clientstruct, room, what, by);
}

static void firetalk_callback_chat_opped(firetalk_t conn, client_t c, const char *const room, const char *const by) {
	struct s_firetalk_room *r;

	r = firetalk_find_room(conn, room);
	if (r == NULL)
		return;
	if (r->admin == 0)
		r->admin = 1;
	else
		return;
	if (conn->UA[FC_CHAT_OPPED])
		conn->UA[FC_CHAT_OPPED](conn, conn->clientstruct, room, by);
}

static void firetalk_callback_chat_deopped(firetalk_t conn, client_t c, const char *const room, const char *const by) {
	struct s_firetalk_room *r;

	r = firetalk_find_room(conn, room);
	if (r == NULL)
		return;
	if (r->admin == 1)
		r->admin = 0;
	else
		return;
	if (conn->UA[FC_CHAT_DEOPPED])
		conn->UA[FC_CHAT_DEOPPED](conn, conn->clientstruct, room, by);
}

static void firetalk_callback_chat_user_kicked(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by, const char *const reason) {
	if (firetalk_chat_internal_remove_member(conn, room, who) != FE_SUCCESS)
		return;
	if (conn->UA[FC_CHAT_USER_KICKED])
		conn->UA[FC_CHAT_USER_KICKED](conn, conn->clientstruct, room, who, by, reason);
}

const char *firetalk_subcode_get_request_reply(firetalk_t conn, client_t c, const char *const command) {
	struct s_firetalk_subcode_callback *iter;

	for (iter = conn->subcode_request_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0)
			if (iter->staticresp != NULL)
				return(iter->staticresp);
	return(NULL);
}

static void firetalk_callback_file_offer(firetalk_t conn, client_t c, const char *const from, const char *const filename, const long size, const char *const ipstring, const char *const ip6string, const uint16_t port, const int type);

static void firetalk_callback_subcode_request(firetalk_t conn, client_t c, const char *const from, const char *const command, char *args) {
	struct s_firetalk_subcode_callback *iter;

	for (iter = conn->subcode_request_head; iter != NULL; iter = iter->next)
		if (strcmp(command, iter->command) == 0) {
			if (iter->staticresp != NULL)
				firetalk_subcode_send_reply(conn, from, command, iter->staticresp);
			else {
				isonline_hack = from;
				iter->callback(conn, conn->clientstruct, from, command, args);
				isonline_hack = NULL;
			}
			return;
		}

	if (strcmp(command, "ACTION") == 0)
		/* we don't support chatroom subcodes, so we're just going to assume that this is a private ACTION and let the protocol code handle the other case */
		firetalk_callback_im_getaction(conn, c, from, 0, args);
	else if (strcmp(command, "VERSION") == 0)
		firetalk_subcode_send_reply(conn, from, "VERSION", PACKAGE_NAME ":" PACKAGE_VERSION ":unknown");
	else if (strcmp(command, "CLIENTINFO") == 0)
		firetalk_subcode_send_reply(conn, from, "CLIENTINFO", "CLIENTINFO PING SOURCE TIME VERSION");
	else if (strcmp(command, "PING") == 0) {
		if (args)
			firetalk_subcode_send_reply(conn, from, "PING", args);
		else
			firetalk_subcode_send_reply(conn, from, "PING", "");
	} else if (strcmp(command, "TIME") == 0) {
		time_t temptime;
		char tempbuf[64];
		size_t s;

		time(&temptime);
		strncpy(tempbuf, ctime(&temptime), sizeof(tempbuf)-1);
		tempbuf[sizeof(tempbuf)-1] = 0;
		s = strlen(tempbuf);
		if (s > 0)
			tempbuf[s-1] = '\0';
		firetalk_subcode_send_reply(conn, from, "TIME", tempbuf);
	} else if ((strcmp(command, "DCC") == 0) && (args != NULL) && (strncasecmp(args, "SEND ", 5) == 0)) {
		/* DCC send */
		struct in_addr addr;
		unsigned long ip;
		long size = -1;
		uint16_t port;
		char **myargs;
#ifdef _FC_USE_IPV6
		struct in6_addr addr6;
		struct in6_addr *sendaddr6 = NULL;
#endif
		myargs = firetalk_parse_subcode_args(&args[5]);
		if ((myargs[0] != NULL) && (myargs[1] != NULL) && (myargs[2] != NULL)) {
			/* valid dcc send */
			if (myargs[3]) {
				size = atol(myargs[3]);
#ifdef _FC_USE_IPV6
				if (myargs[4]) {
					/* ipv6-enabled dcc */
					inet_pton(AF_INET6, myargs[4], &addr6);
					sendaddr6 = &addr6;
				}
#endif
			}
			sscanf(myargs[1], "%lu", &ip);
			ip = htonl(ip);
			memcpy(&addr.s_addr, &ip, 4);
			port = (uint16_t)atoi(myargs[2]);
			firetalk_callback_file_offer(conn, c, from, myargs[0], size, inet_ntoa(addr), NULL, port, FF_TYPE_DCC);
		}
	} else if (conn->subcode_request_default != NULL)
		conn->subcode_request_default->callback(conn, conn->clientstruct, from, command, args);
}

static void firetalk_callback_subcode_reply(firetalk_t conn, client_t c, const char *const from, const char *const command, const char *const args) {
	struct s_firetalk_subcode_callback *iter;

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
static void firetalk_callback_file_offer(firetalk_t conn, client_t c, const char *const from, const char *const filename, const long size, const char *const ipstring, const char *const ip6string, const uint16_t port, const int type) {
	struct s_firetalk_file *iter;

	iter = conn->file_head;
	conn->file_head = calloc(1, sizeof(struct s_firetalk_file));
	if (conn->file_head == NULL)
		abort();
	conn->file_head->who = strdup(from);
	if (conn->file_head->who == NULL)
		abort();
	conn->file_head->filename = strdup(filename);
	if (conn->file_head->filename == NULL)
		abort();
	conn->file_head->size = size;
	conn->file_head->bytes = 0;
	conn->file_head->acked = 0;
	conn->file_head->state = FF_STATE_WAITLOCAL;
	conn->file_head->direction = FF_DIRECTION_RECEIVING;
	conn->file_head->sockfd = -1;
	conn->file_head->filefd = -1;
	conn->file_head->port = htons(port);
	conn->file_head->type = type;
	conn->file_head->next = iter;
	conn->file_head->clientfilestruct = NULL;
	if (inet_aton(ipstring, &conn->file_head->inet_ip) == 0) {
		firetalk_file_cancel(c, conn->file_head);
		return;
	}
#ifdef _FC_USE_IPV6
	conn->file_head->tryinet6 = 0;
	if (ip6string)
		if (inet_pton(AF_INET6, ip6string, &conn->file_head->inet6_ip) != 0)
			conn->file_head->tryinet6 = 1;
#endif
	if (conn->UA[FC_FILE_OFFER])
		conn->UA[FC_FILE_OFFER](conn, conn->clientstruct, (void *)conn->file_head, from, filename, size);
}

void firetalk_handle_receive(firetalk_t conn, struct s_firetalk_file *filestruct) {
	/* we have to copy from sockfd to filefd until we run out, then send the packet */
	static char buffer[4096];
	unsigned long netbytes;
	ssize_t s;

	while ((s = recv(filestruct->sockfd, buffer, 4096, MSG_DONTWAIT)) == 4096) {
		if (write(filestruct->filefd, buffer, 4096) != 4096) {
			if (conn->UA[FC_FILE_ERROR])
				conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
		filestruct->bytes += 4096;
	}
	if (s != -1) {
		if (write(filestruct->filefd, buffer, (size_t)s) != s) {
			if (conn->UA[FC_FILE_ERROR])
				conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
		filestruct->bytes += s;
	}
	if (filestruct->type == FF_TYPE_DCC) {
		netbytes = htonl((uint32_t)filestruct->bytes);
		if (write(filestruct->sockfd, &netbytes, 4) != 4) {
			if (conn->UA[FC_FILE_ERROR])
				conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
			firetalk_file_cancel(conn, filestruct);
			return;
		}
	}
	if (conn->UA[FC_FILE_PROGRESS])
		conn->UA[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
	if (filestruct->bytes == filestruct->size) {
		if (conn->UA[FC_FILE_FINISH])
			conn->UA[FC_FILE_FINISH](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->size);
		firetalk_file_cancel(conn, filestruct);
	}
}

void firetalk_handle_send(firetalk_t conn, struct s_firetalk_file *filestruct) {
	/* we have to copy from filefd to sockfd until we run out or sockfd refuses the data */
	static char buffer[4096];
	ssize_t s;

	while ((s = read(filestruct->filefd, buffer, 4096)) == 4096) {
		if ((s = send(filestruct->sockfd, buffer, 4096, MSG_DONTWAIT|MSG_NOSIGNAL)) != 4096) {
			lseek(filestruct->filefd, -(4096 - s), SEEK_CUR);
			filestruct->bytes += s;
			if (conn->UA[FC_FILE_PROGRESS])
				conn->UA[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
			return;
		}
		filestruct->bytes += s;
		if (conn->UA[FC_FILE_PROGRESS])
			conn->UA[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
		if (filestruct->type == FF_TYPE_DCC) {
			uint32_t acked = 0;

			while (recv(filestruct->sockfd, &acked, 4, MSG_DONTWAIT) == 4)
				filestruct->acked = ntohl(acked);
		}
	}
	if (send(filestruct->sockfd, buffer, s, MSG_NOSIGNAL) != s) {
		if (conn->UA[FC_FILE_ERROR])
			conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, FE_IOERROR);
		firetalk_file_cancel(conn, filestruct);
		return;
	}
	filestruct->bytes += s;
	if (filestruct->type == FF_TYPE_DCC) {
		while (filestruct->acked < (uint32_t)filestruct->bytes) {
			uint32_t acked = 0;

			if (recv(filestruct->sockfd, &acked, 4, 0) == 4)
				filestruct->acked = ntohl(acked);
		}
	}
	if (conn->UA[FC_FILE_PROGRESS])
		conn->UA[FC_FILE_PROGRESS](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes, filestruct->size);
	if (conn->UA[FC_FILE_FINISH])
		conn->UA[FC_FILE_FINISH](conn, conn->clientstruct, filestruct, filestruct->clientfilestruct, filestruct->bytes);
	firetalk_file_cancel(conn, filestruct);
}

/* External function definitions */

const char *firetalk_strprotocol(const enum firetalk_protocol p) {
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
		case FE_DUPEUSER:
			return("User already in list");
		case FE_DUPEROOM:
			return("Room already in list");
		case FE_IOERROR:
        		return("Input/output error");
		case FE_BADHANDLE:
        		return("Invalid handle");
		case FE_TIMEOUT:
			return("Operation timed out");
		case FE_RPC:
			return("Error communicating over RPC");
		default:
			return("Invalid error number");
	}
}

void firetalk_destroy_handle(firetalk_t conn) {
	VERIFYCONN;

	assert(conn->deleted == 0);
//	assert(conn->handle != NULL);

	conn->PD->destroy_handle(conn, conn->handle);
	conn->handle = NULL;
	conn->deleted = 1;
}

fte_t	firetalk_disconnect(firetalk_t conn) {
	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(conn->PD->disconnect(conn, conn->handle));
}

fte_t	firetalk_signon(firetalk_t conn, const char *server, short port, const char *const username) {
	VERIFYCONN;

	if (conn->connected != FCS_NOTCONNECTED)
		firetalk_disconnect(conn);
	conn->connected = FCS_WAITING_SIGNON;

	errno = 0;
	return(conn->PD->connect(conn, conn->handle, server, port, username));
}

fte_t	firetalk_sendpass(firetalk_t conn, const char *const password) {
	VERIFYCONN;

	return(conn->PD->sendpass(conn, conn->handle, password));
}

static void firetalk_callback_connected(firetalk_t conn, client_t c) {
#ifdef FT_OLD_CONN_FD
	unsigned int l;
	struct sockaddr_in addr;

	l = (unsigned int)sizeof(struct sockaddr_in);
	getsockname(conn->fd, (struct sockaddr *)&addr, &l);
	memcpy(&conn->localip, &addr.sin_addr.s_addr, 4);
	conn->localip = htonl((uint32_t)conn->localip);
#endif
	conn->connected = FCS_ACTIVE;

	if (conn->UA[FC_CONNECTED])
		conn->UA[FC_CONNECTED](conn, conn->clientstruct);
}

static const firetalk_PI_t firetalk_PI = {
	im_getmessage:		firetalk_callback_im_getmessage,
	im_getaction:		firetalk_callback_im_getaction,
	im_buddyonline:		firetalk_callback_im_buddyonline,
	im_buddyaway:		firetalk_callback_im_buddyaway,
	buddyadded:		firetalk_callback_buddyadded,
	buddyremoved:		firetalk_callback_buddyremoved,
	typing:			firetalk_callback_typing,
	capabilities:		firetalk_callback_capabilities,
	warninfo:		firetalk_callback_warninfo,
	error:			firetalk_callback_error,
	connectfailed:		firetalk_callback_connectfailed,
	connected:		firetalk_callback_connected,
	disconnect:		firetalk_callback_disconnect,
	gotinfo:		firetalk_callback_gotinfo,
	idleinfo:		firetalk_callback_idleinfo,
	doinit:			firetalk_callback_doinit,
	setidle:		firetalk_callback_setidle,
	eviled:			firetalk_callback_eviled,
	newnick:		firetalk_callback_newnick,
	passchanged:		firetalk_callback_passchanged,
	user_nickchanged:	firetalk_callback_user_nickchanged,
	chat_joined:		firetalk_callback_chat_joined,
	chat_left:		firetalk_callback_chat_left,
	chat_kicked:		firetalk_callback_chat_kicked,
	chat_getmessage:	firetalk_callback_chat_getmessage,
	chat_getaction:		firetalk_callback_chat_getaction,
	chat_invited:		firetalk_callback_chat_invited,
	chat_user_joined:	firetalk_callback_chat_user_joined,
	chat_user_left:		firetalk_callback_chat_user_left,
	chat_user_quit:		firetalk_callback_chat_user_quit,
	chat_gottopic:		firetalk_callback_chat_gottopic,
#ifdef RAWIRCMODES
	chat_modechanged:	firetalk_callback_chat_modechanged,
#endif
	chat_user_opped:	firetalk_callback_chat_user_opped,
	chat_user_deopped:	firetalk_callback_chat_user_deopped,
	chat_keychanged:	firetalk_callback_chat_keychanged,
	chat_opped:		firetalk_callback_chat_opped,
	chat_deopped:		firetalk_callback_chat_deopped,
	chat_user_kicked:	firetalk_callback_chat_user_kicked,
	subcode_request:	firetalk_callback_subcode_request,
	subcode_reply:		firetalk_callback_subcode_reply,
	file_offer:		firetalk_callback_file_offer,
	needpass:		firetalk_callback_needpass,
};

extern const firetalk_PI_t firetalk_PI_remote;
extern const firetalk_PD_t firetalk_PD_remote;
firetalk_t remote_mainloop(firetalk_t conn);

firetalk_t firetalk_create_handle(const int protocol, void *clientstruct, const int dofork) {
	firetalk_t conn;

	if ((protocol < 0) || (protocol >= FP_MAX)) {
		firetalkerror = FE_BADPROTO;
		return(NULL);
	}

	if (!dofork && firetalk_protocols[protocol]->mustfork)
		return(NULL);

	conn = calloc(1, sizeof(*conn));
	if (conn == NULL)
		abort();
	conn->next = handle_head;
	handle_head = conn;
	conn->clientstruct = clientstruct;
	conn->connected = FCS_NOTCONNECTED;
	firetalk_sock_init(&(conn->rpcpeer));
	if ((dofork == 2) || (dofork && firetalk_protocols[protocol]->mustfork)) {
		int	pipe[2];
		char	c;

		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, pipe) != 0)
			return(NULL);

		switch (fork()) {
		  case -1:
			close(pipe[0]);
			close(pipe[1]);
			return(NULL);
		  case 0:
			close(pipe[1]);
			c = 'C';
			write(pipe[0], &c, 1);
			read(pipe[0], &c, 1);
			if (c != 'P') {
				close(pipe[0]);
				_exit(0);
			}
			conn->rpcpeer.fd = pipe[0];
			conn->rpcpeer.state = FCS_ACTIVE;
			conn->handle = firetalk_protocols[protocol]->create_handle(conn);
			conn->PD = firetalk_protocols[protocol];
			conn->PI = &firetalk_PI_remote;
			conn->next = NULL;
			return(remote_mainloop(conn));
		  default:
			close(pipe[0]);
			c = 'P';
			write(pipe[1], &c, 1);
			read(pipe[1], &c, 1);
			if (c != 'C') {
				close(pipe[1]);
				return(NULL);
			}
			conn->rpcpeer.fd = pipe[1];
			conn->rpcpeer.state = FCS_ACTIVE;
			conn->PD = &firetalk_PD_remote;
			conn->PI = &firetalk_PI;
		}
	} else {
		conn->handle = firetalk_protocols[protocol]->create_handle(conn);
		conn->PD = firetalk_protocols[protocol];
		conn->PI = &firetalk_PI;
	}
	return(conn);
}

fte_t	firetalk_handle_file_synack(firetalk_t conn, struct s_firetalk_file *file) {
	int	i;
	unsigned int o = sizeof(int);

	if (getsockopt(file->sockfd, SOL_SOCKET, SO_ERROR, &i, &o)) {
		firetalk_file_cancel(conn, file);
		return(FE_SOCKET);
	}

	if (i != 0) {
		firetalk_file_cancel(conn, file);
		return(FE_CONNECT);
	}

	file->state = FF_STATE_TRANSFERRING;

	if (conn->UA[FC_FILE_START])
		conn->UA[FC_FILE_START](conn, conn->clientstruct, file, file->clientfilestruct);
	return(FE_SUCCESS);
}

fte_t	firetalk_register_callback(firetalk_t conn, const int type, void (*function)(firetalk_t, void *, ...)) {
	VERIFYCONN;

	if (type < 0 || type >= FC_MAX)
		return(FE_CALLBACKNUM);
	conn->UA[type] = function;
	return(FE_SUCCESS);
}

fte_t	firetalk_im_add_buddy(firetalk_t conn, const char *const name, const char *const group, const char *const friendly) {
	struct s_firetalk_buddy *iter;

	VERIFYCONN;

	if ((iter = firetalk_im_find_buddy(conn, name)) != NULL) {
		if ((strcmp(iter->group, group) == 0) && (((iter->friendly == NULL) && (friendly == NULL)) || ((iter->friendly != NULL) && (friendly != NULL) && (strcmp(iter->friendly, friendly) == 0))))
			return(FE_DUPEUSER); /* not an error, user is in buddy list exactly where the client wants it */
		else {
			/* user is in buddy list somewhere other than where the clients wants it */
			if (conn->connected != FCS_NOTCONNECTED) {
				fte_t	ret;

				ret = conn->PD->im_remove_buddy(conn, conn->handle, iter->nickname, iter->group);
				if (ret != FE_SUCCESS)
					return(ret);
			}
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
		}
	} else
		firetalk_im_insert_buddy(conn, name, group, friendly);

        if (conn->connected != FCS_NOTCONNECTED) {
		fte_t	ret;

		ret = conn->PD->im_add_buddy(conn, conn->handle, name, group, friendly);
		if (ret != FE_SUCCESS)
			return(ret);
	}

	if ((isonline_hack != NULL) && (conn->PD->comparenicks(conn, name, isonline_hack) == FE_SUCCESS))
		firetalk_callback_im_buddyonline(conn, conn->handle, isonline_hack, 1);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_add_deny(firetalk_t conn, const char *const nickname) {
	int	ret;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	ret = firetalk_im_internal_add_deny(conn, nickname);
	if (ret != FE_SUCCESS)
		return(ret);

	return(conn->PD->im_add_deny(conn, conn->handle, nickname));
}

fte_t	firetalk_im_remove_deny(firetalk_t conn, const char *const nickname) {
	int	ret;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	ret = firetalk_im_internal_remove_deny(conn, nickname);
	if (ret != FE_SUCCESS)
		return(ret);

	return(conn->PD->im_remove_deny(conn, conn->handle, nickname));
}

fte_t	firetalk_save_config(firetalk_t conn) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(conn->PD->save_config(conn, conn->handle));
}

fte_t	firetalk_im_upload_buddies(firetalk_t conn) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(conn->PD->im_upload_buddies(conn, conn->handle));
}

fte_t	firetalk_im_upload_denies(firetalk_t conn) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(conn->PD->im_upload_denies(conn, conn->handle));
}

fte_t	firetalk_im_send_message(firetalk_t conn, const char *const dest, const char *const message, const int auto_flag) {
	fte_t	e;

	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	if ((conn->connected != FCS_ACTIVE) && (strcasecmp(dest, ":RAW") != 0))
		return(FE_NOTCONNECTED);

	e = conn->PD->im_send_message(conn, conn->handle, dest, message, auto_flag);
	if (e != FE_SUCCESS)
		return(e);

	e = conn->PD->periodic(conn);
	if (e != FE_SUCCESS && e != FE_IDLEFAST)
		return(e);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_send_action(firetalk_t conn, const char *const dest, const char *const message, const int auto_flag) {
	fte_t	e;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	e = conn->PD->im_send_action(conn, conn->handle, dest, message, auto_flag);
	if (e != FE_SUCCESS)
		return(e);

	e = conn->PD->periodic(conn);
	if (e != FE_SUCCESS && e != FE_IDLEFAST)
		return(e);

	return(FE_SUCCESS);
}

fte_t	firetalk_im_get_info(firetalk_t conn, const char *const nickname) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(conn->PD->get_info(conn, conn->handle, nickname));
}

fte_t	firetalk_set_info(firetalk_t conn, const char *const info) {
	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(conn->PD->set_info(conn, conn->handle, info));
}

fte_t	firetalk_im_list_buddies(firetalk_t conn) {
	struct s_firetalk_buddy *buddyiter;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	if (!conn->UA[FC_IM_LISTBUDDY])
		return(FE_SUCCESS);

	for (buddyiter = conn->buddy_head; buddyiter != NULL; buddyiter = buddyiter->next)
		conn->UA[FC_IM_LISTBUDDY](conn, conn->clientstruct, buddyiter->nickname, buddyiter->group, buddyiter->friendly, buddyiter->online, buddyiter->away, buddyiter->idletime);

	return(FE_SUCCESS);
}

fte_t	firetalk_chat_listmembers(firetalk_t conn, const char *const roomname) {
	struct s_firetalk_room *room;
	struct s_firetalk_member *memberiter;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	if (!conn->UA[FC_CHAT_LISTMEMBER])
		return(FE_SUCCESS);

	room = firetalk_find_room(conn, roomname);
	if (room == NULL)
		return(firetalkerror);

	for (memberiter = room->member_head; memberiter != NULL; memberiter = memberiter->next)
		conn->UA[FC_CHAT_LISTMEMBER](conn, conn->clientstruct, room->name, memberiter->nickname, memberiter->admin);

	return(FE_SUCCESS);
}

const char *firetalk_chat_normalize(firetalk_t conn, const char *const room) {
	return(conn->PD->room_normalize(conn, room));
}

fte_t	firetalk_set_away(firetalk_t conn, const char *const message, const int auto_flag) {
	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(conn->PD->set_away(conn, conn->handle, message, auto_flag));
}

fte_t	firetalk_set_nickname(firetalk_t conn, const char *const nickname) {
	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(conn->PD->set_nickname(conn, conn->handle, nickname));
}

fte_t	firetalk_set_password(firetalk_t conn, const char *const oldpass, const char *const newpass) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(conn->PD->set_password(conn, conn->handle, oldpass, newpass));
}

fte_t	firetalk_set_privacy(firetalk_t conn, const char *const mode) {
	VERIFYCONN;

	assert(mode != NULL);

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	return(conn->PD->set_privacy(conn, conn->handle, mode));
}

fte_t	firetalk_im_evil(firetalk_t conn, const char *const who) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	return(conn->PD->im_evil(conn, conn->handle, who));
}

fte_t	firetalk_chat_join(firetalk_t conn, const char *const room) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_join(conn, conn->handle, normalroom));
}

fte_t	firetalk_chat_part(firetalk_t conn, const char *const room) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected == FCS_NOTCONNECTED)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_part(conn, conn->handle, normalroom));
}

fte_t	firetalk_chat_send_message(firetalk_t conn, const char *const room, const char *const message, const int auto_flag) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	if (*room == ':')
		normalroom = room;
	else
		normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_send_message(conn, conn->handle, normalroom, message, auto_flag));
}

fte_t	firetalk_chat_send_action(firetalk_t conn, const char *const room, const char *const message, const int auto_flag) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_send_action(conn, conn->handle, normalroom, message, auto_flag));
}

fte_t	firetalk_chat_invite(firetalk_t conn, const char *const room, const char *const who, const char *const message) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_invite(conn, conn->handle, normalroom, who, message));
}

fte_t	firetalk_chat_set_topic(firetalk_t conn, const char *const room, const char *const topic) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_set_topic(conn, conn->handle, normalroom, topic));
}

fte_t	firetalk_chat_op(firetalk_t conn, const char *const room, const char *const who) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_op(conn, conn->handle, normalroom, who));
}

fte_t	firetalk_chat_deop(firetalk_t conn, const char *const room, const char *const who) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_deop(conn, conn->handle, normalroom, who));
}

fte_t	firetalk_chat_kick(firetalk_t conn, const char *const room, const char *const who, const char *const reason) {
	const char *normalroom;

	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	normalroom = conn->PD->room_normalize(conn, room);
	if (!normalroom)
		return(FE_ROOMUNAVAILABLE);

	return(conn->PD->chat_kick(conn, conn->handle, normalroom, who, reason));
}

fte_t	firetalk_subcode_send_request(firetalk_t conn, const char *const to, const char *const command, const char *const args) {
	VERIFYCONN;

	if (conn->connected != FCS_ACTIVE)
		return(FE_NOTCONNECTED);

	firetalk_enqueue(&conn->subcode_requests, to, conn->PD->subcode_encode(conn, conn->handle, command, args));
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_send_reply(firetalk_t conn, const char *const to, const char *const command, const char *const args) {
	VERIFYCONN;

	if ((conn->connected != FCS_ACTIVE) && (*to != ':'))
		return(FE_NOTCONNECTED);

	firetalk_enqueue(&conn->subcode_replies, to, conn->PD->subcode_encode(conn, conn->handle, command, args));
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_request_callback(firetalk_t conn, const char *const command, void (*callback)(firetalk_t, void *, const char *const, const char *const, const char *const)) {
	struct s_firetalk_subcode_callback *iter;

	VERIFYCONN;

	if (command == NULL) {
		if (conn->subcode_request_default)
			free(conn->subcode_request_default);
		conn->subcode_request_default = calloc(1, sizeof(struct s_firetalk_subcode_callback));
		if (conn->subcode_request_default == NULL)
			abort();
		conn->subcode_request_default->callback = (ptrtofnct)callback;
	} else {
		iter = conn->subcode_request_head;
		conn->subcode_request_head = calloc(1, sizeof(struct s_firetalk_subcode_callback));
		if (conn->subcode_request_head == NULL)
			abort();
		conn->subcode_request_head->next = iter;
		conn->subcode_request_head->command = strdup(command);
		if (conn->subcode_request_head->command == NULL)
			abort();
		conn->subcode_request_head->callback = (ptrtofnct)callback;
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_request_reply(firetalk_t conn, const char *const command, const char *const reply) {
	struct s_firetalk_subcode_callback *iter;

	VERIFYCONN;

	if (command == NULL) {
		if (conn->subcode_request_default)
			free(conn->subcode_request_default);
		conn->subcode_request_default = calloc(1, sizeof(struct s_firetalk_subcode_callback));
		if (conn->subcode_request_default == NULL)
			abort();
		conn->subcode_request_default->staticresp = strdup(reply);
		if (conn->subcode_request_default->staticresp == NULL)
			abort();
	} else {
		iter = conn->subcode_request_head;
		conn->subcode_request_head = calloc(1, sizeof(struct s_firetalk_subcode_callback));
		if (conn->subcode_request_head == NULL)
			abort();
		conn->subcode_request_head->next = iter;
		conn->subcode_request_head->command = strdup(command);
		if (conn->subcode_request_head->command == NULL)
			abort();
		conn->subcode_request_head->staticresp = strdup(reply);
		if (conn->subcode_request_head->staticresp == NULL)
			abort();
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_subcode_register_reply_callback(firetalk_t conn, const char *const command, void (*callback)(firetalk_t, void *, const char *const, const char *const, const char *const)) {
	struct s_firetalk_subcode_callback *iter;

	VERIFYCONN;

	if (command == NULL) {
		if (conn->subcode_reply_default)
			free(conn->subcode_reply_default);
		conn->subcode_reply_default = calloc(1, sizeof(struct s_firetalk_subcode_callback));
		if (conn->subcode_reply_default == NULL)
			abort();
		conn->subcode_reply_default->callback = (ptrtofnct)callback;
	} else {
		iter = conn->subcode_reply_head;
		conn->subcode_reply_head = calloc(1, sizeof(struct s_firetalk_subcode_callback));
		if (conn->subcode_reply_head == NULL)
			abort();
		conn->subcode_reply_head->next = iter;
		conn->subcode_reply_head->command = strdup(command);
		if (conn->subcode_reply_head->command == NULL)
			abort();
		conn->subcode_reply_head->callback = (ptrtofnct)callback;
	}
	return(FE_SUCCESS);
}

fte_t	firetalk_file_offer(firetalk_t conn, const char *const nickname, const char *const filename, void *clientfilestruct) {
	struct s_firetalk_file *iter;
	struct stat s;
	struct sockaddr_in addr;
	char	args[256];
	unsigned int l;

	VERIFYCONN;

	iter = conn->file_head;
	conn->file_head = calloc(1, sizeof(struct s_firetalk_file));
	if (conn->file_head == NULL)
		abort();
	conn->file_head->who = strdup(nickname);
	if (conn->file_head->who == NULL)
		abort();
	conn->file_head->filename = strdup(filename);
	if (conn->file_head->filename == NULL)
		abort();
	conn->file_head->sockfd = -1;
	conn->file_head->clientfilestruct = clientfilestruct;

	conn->file_head->filefd = open(filename, O_RDONLY);
	if (conn->file_head->filefd == -1) {
		firetalk_file_cancel(conn, conn->file_head);
		return(FE_IOERROR);
	}

	if (fstat(conn->file_head->filefd, &s) != 0) {
		firetalk_file_cancel(conn, conn->file_head);
		return(FE_IOERROR);
	}

	conn->file_head->size = (long)s.st_size;

	conn->file_head->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (conn->file_head->sockfd == -1) {
		firetalk_file_cancel(conn, conn->file_head);
		return(FE_SOCKET);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(conn->file_head->sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != 0) {
		firetalk_file_cancel(conn, conn->file_head);
		return(FE_SOCKET);
	}

	if (listen(conn->file_head->sockfd, 1) != 0) {
		firetalk_file_cancel(conn, conn->file_head);
		return(FE_SOCKET);
	}

	l = (unsigned int)sizeof(struct sockaddr_in);
	if (getsockname(conn->file_head->sockfd, (struct sockaddr *)&addr, &l) != 0) {
		firetalk_file_cancel(conn, conn->file_head);
		return(FE_SOCKET);
	}

	conn->file_head->bytes = 0;
	conn->file_head->state = FF_STATE_WAITREMOTE;
	conn->file_head->direction = FF_DIRECTION_SENDING;
	conn->file_head->port = ntohs(addr.sin_port);
	conn->file_head->next = iter;
	conn->file_head->type = FF_TYPE_DCC;
	snprintf(args, sizeof(args), "SEND %s %lu %u %ld", conn->file_head->filename, conn->localip, conn->file_head->port, conn->file_head->size);
	return(firetalk_subcode_send_request(conn, nickname, "DCC", args));
}

fte_t	firetalk_file_accept(firetalk_t conn, void *filehandle, void *clientfilestruct, const char *const localfile) {
	struct s_firetalk_file *fileiter;
	struct sockaddr_in addr;

	VERIFYCONN;

	fileiter = filehandle;
	fileiter->clientfilestruct = clientfilestruct;

	fileiter->filefd = open(localfile, O_WRONLY|O_CREAT|O_EXCL, S_IRWXU);
	if (fileiter->filefd == -1)
		return(FE_NOPERMS);

	addr.sin_family = AF_INET;
	addr.sin_port = fileiter->port;
	memcpy(&addr.sin_addr.s_addr, &fileiter->inet_ip, 4);
#if 0
	fileiter->sockfd = firetalk_sock_connect(&addr
#ifdef _FC_USE_IPV6
	, NULL
#endif
	);
#else
	fileiter->sockfd = -1;
#endif
	if (fileiter->sockfd == -1) {
		firetalk_file_cancel(conn, filehandle);
		return(FE_SOCKET);
	}
	fileiter->state = FF_STATE_WAITSYNACK;
	return(FE_SUCCESS);
}

fte_t	firetalk_file_cancel(firetalk_t conn, void *filehandle) {
	struct s_firetalk_file *fileiter, *prev;

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
			if (fileiter->sockfd >= 0) {
				close(fileiter->sockfd);
				fileiter->sockfd = -1;
			}
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

fte_t	firetalk_file_refuse(firetalk_t conn, void *filehandle) {
	return(firetalk_file_cancel(conn, filehandle));
}

fte_t	firetalk_compare_nicks(firetalk_t conn, const char *const nick1, const char *const nick2) {
	VERIFYCONN;

	return(conn->PD->comparenicks(conn, nick1, nick2));
}

fte_t	firetalk_isprint(firetalk_t conn, const int c) {
	VERIFYCONN;

	return(conn->PD->isprintable(conn, c));
}

fte_t	firetalk_select(void) {
	return(firetalk_select_custom(0, NULL, NULL, NULL, NULL));
}

fte_t	firetalk_select_custom(int n, fd_set *fd_read, fd_set *fd_write, fd_set *fd_except, struct timeval *timeout) {
	int	ret;
	fd_set *my_read, *my_write, *my_except;
	fd_set internal_read, internal_write, internal_except;
	struct timeval internal_timeout, *my_timeout;
	firetalk_t conn;

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
	for (conn = handle_head; conn != NULL; conn = conn->next) {
		struct s_firetalk_file *fileiter;

		if (conn->deleted)
			continue;

		if (conn->connected == FCS_NOTCONNECTED)
			continue;

		while (conn->subcode_requests.count > 0) {
			int	count = conn->subcode_requests.count;

			conn->PD->im_send_message(conn, conn->handle, conn->subcode_requests.keys[0], "", 0);
			assert(conn->subcode_requests.count < count);
		}

		while (conn->subcode_replies.count > 0) {
			int	count = conn->subcode_replies.count;

			conn->PD->im_send_message(conn, conn->handle, conn->subcode_replies.keys[0], "", 1);
			assert(conn->subcode_replies.count < count);
		}

		conn->PD->periodic(conn);
		if (conn->connected == FCS_NOTCONNECTED)
			continue;

		for (fileiter = conn->file_head; fileiter != NULL; fileiter = fileiter->next) {
			if (fileiter->state == FF_STATE_TRANSFERRING) {
				if (fileiter->sockfd >= n)
					n = fileiter->sockfd + 1;
				switch (fileiter->direction) {
				  case FF_DIRECTION_SENDING:
					assert(fileiter->sockfd >= 0);
					FD_SET(fileiter->sockfd, my_write);
					FD_SET(fileiter->sockfd, my_except);
					break;
				  case FF_DIRECTION_RECEIVING:
					assert(fileiter->sockfd >= 0);
					FD_SET(fileiter->sockfd, my_read);
					FD_SET(fileiter->sockfd, my_except);
					break;
				}
			} else if (fileiter->state == FF_STATE_WAITREMOTE) {
				assert(fileiter->sockfd >= 0);
				if (fileiter->sockfd >= n)
					n = fileiter->sockfd + 1;
				FD_SET(fileiter->sockfd, my_read);
				FD_SET(fileiter->sockfd, my_except);
			} else if (fileiter->state == FF_STATE_WAITSYNACK) {
				assert(fileiter->sockfd >= 0);
				if (fileiter->sockfd >= n)
					n = fileiter->sockfd + 1;
				FD_SET(fileiter->sockfd, my_write);
				FD_SET(fileiter->sockfd, my_except);
			}
		}
	}

	/* per-protocol preselect, UI prepoll */
	for (conn = handle_head; conn != NULL; conn = conn->next) {
		if (conn->deleted)
			continue;
		conn->PD->preselect(conn, conn->handle, my_read, my_write, my_except, &n);
		if (conn->UA[FC_PRESELECT])
			conn->UA[FC_PRESELECT](conn, conn->clientstruct);
	}

	/* select */
	if (n > 0) {
		ret = select(n, my_read, my_write, my_except, my_timeout);
		if (ret == -1)
			return(FE_PACKET);
	}

	/* per-protocol postselect, UI postpoll */
	for (conn = handle_head; conn != NULL; conn = conn->next) {
		if (conn->deleted)
			continue;

		conn->PD->postselect(conn, conn->handle, my_read, my_write, my_except);
		if (conn->UA[FC_POSTSELECT])
			conn->UA[FC_POSTSELECT](conn, conn->clientstruct);
	}

	/* internal postpoll */
	for (conn = handle_head; conn != NULL; conn = conn->next) {
		struct s_firetalk_file *fileiter, *filenext;

		if (conn->deleted)
			continue;

		for (fileiter = conn->file_head; fileiter != NULL; fileiter = filenext) {
			filenext = fileiter->next;
			if (fileiter->state == FF_STATE_TRANSFERRING) {
				assert(fileiter->sockfd >= 0);
				if (FD_ISSET(fileiter->sockfd, my_write))
					firetalk_handle_send(conn, fileiter);
				if ((fileiter->sockfd != -1) && FD_ISSET(fileiter->sockfd, my_read))
					firetalk_handle_receive(conn, fileiter);
				if ((fileiter->sockfd != -1) && FD_ISSET(fileiter->sockfd, my_except)) {
					if (conn->UA[FC_FILE_ERROR])
						conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct, FE_IOERROR);
					firetalk_file_cancel(conn, fileiter);
				}
			} else if (fileiter->state == FF_STATE_WAITREMOTE) {
				assert(fileiter->sockfd >= 0);
				if (FD_ISSET(fileiter->sockfd, my_read)) {
					unsigned int l = sizeof(struct sockaddr_in);
					struct sockaddr_in addr;
					int	s;

					s = accept(fileiter->sockfd, (struct sockaddr *)&addr, &l);
					if (s == -1) {
						if (conn->UA[FC_FILE_ERROR])
							conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct, FE_SOCKET);
						firetalk_file_cancel(conn, fileiter);
					} else {
						close(fileiter->sockfd);
						fileiter->sockfd = s;
						fileiter->state = FF_STATE_TRANSFERRING;
						if (conn->UA[FC_FILE_START])
							conn->UA[FC_FILE_START](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct);
					}
				} else if (FD_ISSET(fileiter->sockfd, my_except)) {
					if (conn->UA[FC_FILE_ERROR])
						conn->UA[FC_FILE_ERROR](conn, conn->clientstruct, fileiter, fileiter->clientfilestruct, FE_IOERROR);
					firetalk_file_cancel(conn, fileiter);
				}
			} else if (fileiter->state == FF_STATE_WAITSYNACK) {
				assert(fileiter->sockfd >= 0);
				if (FD_ISSET(fileiter->sockfd, my_write))
					firetalk_handle_file_synack(conn, fileiter);
				if (FD_ISSET(fileiter->sockfd, my_except))
					firetalk_file_cancel(conn, fileiter);
			}
		}
	}

	/* handle deleted connections */
	{
		struct s_firetalk_handle *connprev, *connnext;

		connprev = NULL;
		for (conn = handle_head; conn != NULL; conn = connnext) {
			connnext = conn->next;
			if (conn->deleted == 1) {
				assert(conn->handle == NULL);
				if (conn->buddy_head != NULL) {
					struct s_firetalk_buddy *iter, *iternext;

					for (iter = conn->buddy_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						if (iter->nickname != NULL) {
							free(iter->nickname);
							iter->nickname = NULL;
						}
						if (iter->group != NULL) {
							free(iter->group);
							iter->group = NULL;
						}
						if (iter->capabilities != NULL) {
							free(iter->capabilities);
							iter->capabilities = NULL;
						}
						free(iter);
					}
					conn->buddy_head = NULL;
				}
				if (conn->deny_head != NULL) {
					struct s_firetalk_deny *iter, *iternext;

					for (iter = conn->deny_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						if (iter->nickname != NULL) {
							free(iter->nickname);
							iter->nickname = NULL;
						}
						free(iter);
					}
					conn->deny_head = NULL;
				}
				if (conn->room_head != NULL) {
					struct s_firetalk_room *iter, *iternext;

					for (iter = conn->room_head; iter != NULL; iter = iternext) {
						struct s_firetalk_member *memberiter, *memberiternext;

						for (memberiter = iter->member_head; memberiter != NULL; memberiter = memberiternext) {
							memberiternext = memberiter->next;
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
					conn->room_head = NULL;
				}
				if (conn->file_head != NULL) {
					struct s_firetalk_file *iter, *iternext;

					for (iter = conn->file_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						if (iter->who != NULL) {
							free(iter->who);
							iter->who = NULL;
						}
						if (iter->filename != NULL) {
							free(iter->filename);
							iter->filename = NULL;
						}
						free(iter);
					}
					conn->file_head = NULL;
				}
				if (conn->subcode_request_head != NULL) {
					struct s_firetalk_subcode_callback *iter, *iternext;

					for (iter = conn->subcode_request_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
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
					conn->subcode_request_head = NULL;
				}
				if (conn->subcode_request_default != NULL) {
					if (conn->subcode_request_default->command != NULL) {
						free(conn->subcode_request_default->command);
						conn->subcode_request_default->command = NULL;
					}
					free(conn->subcode_request_default);
					conn->subcode_request_default = NULL;
				}
				if (conn->subcode_reply_head != NULL) {
					struct s_firetalk_subcode_callback *iter, *iternext;

					for (iter = conn->subcode_reply_head; iter != NULL; iter = iternext) {
						iternext = iter->next;
						free(iter->command);
						free(iter);
					}
					conn->subcode_reply_head = NULL;
				}
				if (conn->subcode_reply_default != NULL) {
					if (conn->subcode_reply_default->command != NULL) {
						free(conn->subcode_reply_default->command);
						conn->subcode_reply_default->command = NULL;
					}
					free(conn->subcode_reply_default);
					conn->subcode_reply_default = NULL;
				}
				if (connprev == NULL) {
					assert(conn == handle_head);
					handle_head = connnext;
				} else {
					assert(conn != handle_head);
					connprev->next = connnext;
				}

				free(conn);
			} else
				connprev = conn;
		}
	}

	return(FE_SUCCESS);
}
