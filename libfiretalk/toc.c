/*
toc.c - FireTalk TOC protocol definitions
Copyright (C) 2000 Ian Gulliver
Copyright 2002-2005 Daniel Reed <n@ml.org>

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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#define __USE_XOPEN
#include <time.h>
#include <fcntl.h>
#include <errno.h>

/* Structures */

struct s_toc_room {
	struct s_toc_room *next;
	int exchange;
	char *name;
	char *id;
	int invited;
	int joined;
};

#define TOC_HTML_MAXLEN 65536

struct s_toc_infoget {
	int sockfd;
	struct s_toc_infoget *next;
#define TOC_STATE_CONNECTING 0
#define TOC_STATE_TRANSFERRING 1
	int state;
	char buffer[TOC_HTML_MAXLEN];
	int buflen;
};

struct s_toc_connection {
	unsigned short local_sequence;			/* our sequence number */
	unsigned short remote_sequence;			/* toc's sequence number */
	char *nickname;					/* our nickname (correctly spaced) */
	struct s_toc_room *room_head;
	struct s_toc_infoget *infoget_head;
	time_t lasttalk;				/* last time we talked */
	long lastidle;					/* last idle that we told the server */
	int passchange;					/* whether we're changing our password right now */
	int connectstate;
	unsigned char
		online:1,
		gotconfig:1;
	char	*profstr;
	size_t	proflen;
	struct {
		char	*command,
			*ect;
	}	*profar;
	int	profc;
	struct {
		char	*dest,
			*ect;
	}	*ectqar;
	int	ectqc;
	char	buddybuf[1024];
	int	buddybuflen;
};

typedef struct s_toc_connection *client_t;
#define _HAVE_CLIENT_T

#include "firetalk-int.h"
#include "firetalk.h"
#include "aim.h"
#include "safestring.h"

#define SFLAP_FRAME_SIGNON ((unsigned char)1)
#define SFLAP_FRAME_DATA ((unsigned char)2)
#define SFLAP_FRAME_ERROR ((unsigned char)3)
#define SFLAP_FRAME_SIGNOFF ((unsigned char)4)
#define SFLAP_FRAME_KEEPALIVE ((unsigned char)5)

#define SIGNON_STRING "FLAPON\r\n\r\n"

#define TOC_HEADER_LENGTH 6
#define TOC_SIGNON_LENGTH 24
#define TOC_HOST_SIGNON_LENGTH 4
#define TOC_USERNAME_MAXLEN 16

static char lastinfo[TOC_USERNAME_MAXLEN + 1] = "";

/* Internal Function Declarations */

static int toc_send_printf(client_t c, const char *const format, ...);
static fte_t
	toc_im_send_message(client_t c, const char *const dest, const char *const message, const int auto_flag);

const char
	*firetalk_nhtmlentities(const char *str, int len) {
	static char
		buf[1024];
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

const char
	*firetalk_htmlentities(const char *str) {
	return(firetalk_nhtmlentities(str, -1));
}

static unsigned char toc_get_frame_type_from_header(const unsigned char *const header) {
	return header[1];
}

static unsigned short toc_get_sequence_from_header(const unsigned char *const header) {
	unsigned short sequence;
	sequence = ntohs(* ((unsigned short *)(&header[2])));
	return sequence;
}

static unsigned short toc_get_length_from_header(const unsigned char *const header) {
	unsigned short length;
	length = ntohs(* ((unsigned short *)(&header[4])));
	return length;
}

#include <assert.h>
#ifdef DEBUG_ECHO
static void toc_echof(client_t c, const char *const where, const char *const format, ...) {
	va_list ap;
	char	buf[8*1024];
	void	statrefresh(void);

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	while (buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = 0;
	if (*buf != 0)
		firetalk_callback_chat_getmessage(c, ":RAW", where, 0, buf);

	statrefresh();
}

static void toc_echo_send(client_t c, const char *const where, const unsigned char *const data, size_t _length) {
	unsigned char	ft;
	unsigned short	sequence,
			length;

	assert(_length > 4);

	length = toc_get_length_from_header(data);
	ft = toc_get_frame_type_from_header(data);
	sequence = toc_get_sequence_from_header(data);

	assert(length == (_length-6));

	toc_echof(c, where, "frame=%X, sequence=out:%i, length=%i<br>&gt;&gt;&gt;&nbsp;%s\n",
		ft, sequence, length, firetalk_nhtmlentities((char *)data+TOC_HEADER_LENGTH, length));
}
#endif

/* Internal Function Definitions */

static int toc_internal_disconnect(client_t c, const int error) {
	if (c->room_head != NULL) {
		struct s_toc_room *iter, *iternext;

		for (iter = c->room_head; iter != NULL; iter = iternext) {
			iternext = iter->next;
			free(iter->name);
			iter->name = NULL;
			free(iter->id);
			iter->id = NULL;
			free(iter);
		}
		c->room_head = NULL;
	}
	if (c->infoget_head != NULL) {
		struct s_toc_infoget *iter, *iternext;

		for (iter = c->infoget_head; iter != NULL; iter = iternext) {
			iternext = iter->next;
			free(iter);
		}
		c->infoget_head = NULL;
	}
	if (c->nickname != NULL) {
		free(c->nickname);
		c->nickname = NULL;
	}
	if (c->profc > 0) {
		int	i;

		for (i = 0; i < c->profc; i++) {
			free(c->profar[i].command);
			c->profar[i].command = NULL;
			free(c->profar[i].ect);
			c->profar[i].ect = NULL;
		}
		free(c->profar);
		c->profar = NULL;
		c->profc = 0;
	}
	assert(c->profar == NULL);
	if (c->proflen > 0) {
		free(c->profstr);
		c->profstr = NULL;
		c->proflen = 0;
	}
	assert(c->profstr == NULL);
	if (c->ectqc > 0) {
		int	i;

		for (i = 0; i < c->ectqc; i++) {
			free(c->ectqar[i].dest);
			c->ectqar[i].dest = NULL;
			free(c->ectqar[i].ect);
			c->ectqar[i].ect = NULL;
		}
		free(c->ectqar);
		c->ectqar = NULL;
		c->ectqc = 0;
	}
	assert(c->ectqar == NULL);

	firetalk_callback_disconnect(c,error);
	return FE_SUCCESS;
}

static fte_t
	toc_find_packet(client_t c, unsigned char *buffer, unsigned short *bufferpos, char *outbuffer, const int frametype, unsigned short *l) {
	unsigned char	ft;
	unsigned short	sequence,
			length;

	if (*bufferpos < TOC_HEADER_LENGTH) /* don't have the whole header yet */
		return FE_NOTFOUND;

	length = toc_get_length_from_header(buffer);
	if (length > (8192 - TOC_HEADER_LENGTH)) {
		toc_internal_disconnect(c,FE_PACKETSIZE);
		return FE_DISCONNECT;
	}

	if (*bufferpos < length + TOC_HEADER_LENGTH) /* don't have the whole packet yet */
		return FE_NOTFOUND;

	ft = toc_get_frame_type_from_header(buffer);
	sequence = toc_get_sequence_from_header(buffer);

	memcpy(outbuffer, &buffer[TOC_HEADER_LENGTH], length);
	*bufferpos -= length + TOC_HEADER_LENGTH;
	memmove(buffer, &buffer[TOC_HEADER_LENGTH + length], *bufferpos);
	outbuffer[length] = '\0';
	*l = length;

#ifdef DEBUG_ECHO
	/*if (ft == frametype)*/ {
		char	buf[1024*8];
		int	j;

		assert(length < sizeof(buf));
		memmove(buf, outbuffer, length+1);
		for (j = 0; j < length; j++)
			if (buf[j] == 0)
				buf[j] = '0';
//			else if (buf[j] == '\n')
//				buf[j] = 'N';
			else if (buf[j] == '\r')
				buf[j] = 'R';

		toc_echof(c, "find_packet", "frame=%X, sequence=in:%i, length=%i<br>&lt;&lt;&lt;&nbsp;%s\n",
			ft, sequence, length, firetalk_htmlentities(buf));
	}
#endif

	if (frametype == SFLAP_FRAME_SIGNON)
		c->remote_sequence = sequence;
	else {
		if (sequence != ++c->remote_sequence) {
			toc_internal_disconnect(c, FE_SEQUENCE);
			return(FE_DISCONNECT);
		}
	}

	if (ft == frametype)
		return(FE_SUCCESS);
	else
		return(FE_WEIRDPACKET);
}

static unsigned short toc_fill_header(unsigned char *const header, unsigned char const frame_type, unsigned short const sequence, unsigned short const length) {
	header[0] = '*';		/* byte 0, length 1, magic 42 */
	header[1] = frame_type;		/* byte 1, length 1, frame type (defined above SFLAP_FRAME_* */
	header[2] = sequence/256;	/* byte 2, length 2, sequence number, network byte order */
	header[3] = (unsigned char) sequence%256;
	header[4] = length/256;		/* byte 4, length 2, length, network byte order */
	header[5] = (unsigned char) length%256;
	return 6 + length;
}

static unsigned short toc_fill_signon(unsigned char *const signon, const char *const username) {
	size_t length;
	length = strlen(username);
	signon[0] = '\0';		/* byte 0, length 4, flap version (1) */
	signon[1] = '\0';
	signon[2] = '\0';
	signon[3] = '\001';
	signon[4] = '\0';		/* byte 4, length 2, tlv tag (1) */
	signon[5] = '\001';
	signon[6] = length/256;		/* byte 6, length 2, username length, network byte order */
	signon[7] = (unsigned char) length%256;
	memcpy(&signon[8],username,length);
	return(length + 8);
}

static void
	toc_infoget_parse(client_t c, struct s_toc_infoget *i) {
	char	*tmp, *str = i->buffer,
		*name, *info;
	int	class = 0, warning;
	long	online, idle;

	i->buffer[i->buflen] = 0;
#ifdef DEBUG_ECHO
	toc_echof(c, "infoget_parse", "&lt;&lt;&lt;&nbsp;%s\n", firetalk_htmlentities(str));
#endif

#define USER_STRING "Username : <B>"
	if ((tmp = strstr(str, USER_STRING)) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, (lastinfo[0] == '\0')?NULL:lastinfo, "Can't find username in info HTML");
		return;
	}
	tmp += sizeof(USER_STRING) - 1;
	if ((str = strchr(tmp, '<')) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, (lastinfo[0] == '\0')?NULL:lastinfo, "Can't find end of username in info HTML");
		return;
	}
	*str++ = 0;
	name = tmp;

#define WARNING_STRING "Warning Level : <B>"
	if ((tmp = strstr(str, WARNING_STRING)) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, name, "Can't find warning level in info HTML");
		return;
	}

	*tmp = 0;
	if ((strstr(str, "free_icon.") != NULL) || (strstr(str, "dt_icon.") != NULL))
		class |= FF_SUBSTANDARD;
	if (strstr(str, "aol_icon.") != NULL)
		class |= FF_NORMAL;
	if (strstr(str, "admin_icon.") != NULL)
		class |= FF_ADMIN;

	tmp += sizeof(WARNING_STRING) - 1;
	if ((str = strchr(tmp, '<')) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, (lastinfo[0] == '\0')?NULL:lastinfo, "Can't find end of warning level in info HTML");
		return;
	}
	*str++ = 0;
	warning = atoi(tmp);

#define ONLINE_STRING "Online Since : <B>"
	if ((tmp = strstr(str, ONLINE_STRING)) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, name, "Can't find online time in info HTML");
		return;
	}
	tmp += sizeof(ONLINE_STRING) - 1;
	if ((str = strchr(tmp, '<')) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, (lastinfo[0] == '\0')?NULL:lastinfo, "Can't find end of online time in info HTML");
		return;
	}
	*str++ = 0;
#ifdef HAVE_STRPTIME
	{
		struct tm tm;

		memset(&tm, 0, sizeof(tm));
		if (strptime(tmp, "%a %b %d %H:%M:%S %Y", &tm) == NULL)
			online = 0;
		else
			online = mktime(&tm);
		{ /* AOL's servers are permanently in UTC-0400 */
			struct tm *tmptr;
			time_t loc = 100000, gm = 100000;

			tmptr = localtime(&loc);
			loc = mktime(tmptr);
			tmptr = gmtime(&gm);
			gm = mktime(tmptr);
			online += loc-gm+4*60*60;
		}
# ifdef DEBUG_ECHO
		{
			time_t	now = time(NULL);

			toc_echof(c, "infoget_parse", "tmp=[%s], tm={ tm_year=%i, tm_mon=%i, tm_mday=%i }, online=%li, now=%li [%li], diff=%li [%li]\n",
				tmp, tm.tm_year, tm.tm_mon, tm.tm_mday, online, time(NULL), now, time(NULL)-online, now-online);
		}
# endif
	}
#else
	online = 0;
#endif

#define IDLE_STRING "Idle Minutes : <B>" 
	if ((tmp = strstr(str, IDLE_STRING)) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, name, "Can't find idle time in info HTML");
		return;
	}
	tmp += sizeof(IDLE_STRING) - 1;
	if ((str = strchr(tmp, '<')) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, (lastinfo[0] == '\0')?NULL:lastinfo, "Can't find end of idle time in info HTML");
		return;
	}
	*str++ = 0;
	idle = atoi(tmp);

#define INFO_STRING "<hr><br>\n"
#define INFO_END_STRING "<br><hr><I>Legend:"
	if ((tmp = strstr(str, INFO_STRING)) == NULL) {
		firetalk_callback_error(c, FE_INVALIDFORMAT, (lastinfo[0] == '\0')?NULL:lastinfo, "Can't find info string in info HTML");
		return;
	}
	tmp += sizeof(INFO_STRING) - 1;
	if ((str = strstr(tmp, INFO_END_STRING)) == NULL)
		info = NULL;
	else {
		*str = 0;
		info = aim_interpolate_variables(tmp, c->nickname);
		info = aim_handle_ect(c, name, info, 1);
	}

	firetalk_callback_gotinfo(c, name, info, warning, online, idle, class);
}

static void
	toc_infoget_remove(client_t c, struct s_toc_infoget *i, char *error) {
	struct s_toc_infoget *m, *m2;

	if (error != NULL)
		firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, error);
	m2 = NULL;
	for (m = c->infoget_head; m != NULL; m = m->next) {
		if (m == i) {
			if (m2 == NULL)
				c->infoget_head = m->next;
			else
				m2->next = m->next;
			close(m->sockfd);
			free(m);
			return;
		}
		m2 = m;
	}
}

static fte_t
	toc_preselect(client_t c, fd_set *read, fd_set *write, fd_set *except, int *n) {
	struct s_toc_infoget *i;

	for (i = c->infoget_head; i != NULL; i = i->next) {
		if (i->state == TOC_STATE_CONNECTING)
			FD_SET(i->sockfd, write);
		else if (i->state == TOC_STATE_TRANSFERRING)
			FD_SET(i->sockfd, read);
		FD_SET(i->sockfd, read);
		if (i->sockfd >= *n)
			*n = i->sockfd + 1;
	}

	if (c->ectqc > 0) {
		int	i;

		for (i = c->ectqc-1; i >= 0; i--)
			toc_im_send_message(c, c->ectqar[i].dest, "", 0);
	}
	if (c->buddybuflen > 0) {
		toc_send_printf(c, "toc_add_buddy%S", c->buddybuf);
		c->buddybuflen = 0;
	}
	return FE_SUCCESS;
}

static fte_t
	toc_postselect(client_t c, fd_set *read, fd_set *write, fd_set *except) {
	struct s_toc_infoget *i, *inext;

	for (i = c->infoget_head; i != NULL; i = inext) {
		inext = i->next;
		if (FD_ISSET(i->sockfd, except)) {
			toc_infoget_remove(c, i, strerror(errno));
			continue;
		}
		if (i->state == TOC_STATE_CONNECTING && FD_ISSET(i->sockfd,write)) {
			int r;
			unsigned int o = sizeof(int);
			if (getsockopt(i->sockfd, SOL_SOCKET, SO_ERROR, &r, &o)) {
				toc_infoget_remove(c, i, strerror(errno));
				continue;
			}
			if (r != 0) {
				toc_infoget_remove(c, i, strerror(r));
				continue;
			}
			if (send(i->sockfd, i->buffer, i->buflen, MSG_NOSIGNAL) != i->buflen) {
				toc_infoget_remove(c, i, strerror(errno));
				continue;
			}
			i->buflen = 0;
			i->state = TOC_STATE_TRANSFERRING;
		} else if (i->state == TOC_STATE_TRANSFERRING && FD_ISSET(i->sockfd, read)) {
			ssize_t s;
			while (1) {
				s = recv(i->sockfd, &i->buffer[i->buflen], TOC_HTML_MAXLEN - i->buflen - 1, MSG_DONTWAIT);
				if (s <= 0)
					break;
				i->buflen += s;
				if (i->buflen == TOC_HTML_MAXLEN - 1) {
					s = -2;
					break;
				}
			}
			if (s == -2) {
				toc_infoget_remove(c, i, "Too much data");
				continue;
			}
			if (s == -1) {
				if (errno != EAGAIN)
					toc_infoget_remove(c,i,strerror(errno));
				continue;
			}
			if (s == 0) {
				/* finished, parse results here */
				toc_infoget_parse(c, i);
				toc_infoget_remove(c, i, NULL);
				continue;
			}

		}
	}

	return FE_SUCCESS;
}

static char *toc_quote(const char *string, const int outside_flag) {
	static char output[2048];
	size_t length;
	size_t counter;
	int newcounter;

	while (*string == ' ')
		string++;

	length = strlen(string);
	if (outside_flag == 1) {
 		newcounter = 1;
		output[0] = '"';
	} else
		newcounter = 0;

	while ((length > 0) && (string[length-1] == ' '))
		length--;

	for (counter = 0; counter < length; counter++) {
		if (string[counter] == '$' || string[counter] == '{' || string[counter] == '}' || string[counter] == '[' || string[counter] == ']' || string[counter] == '(' || string[counter] == ')' || string[counter] == '\'' || string[counter] == '`' || string[counter] == '"' || string[counter] == '\\') {
			if (newcounter > (sizeof(output)-4))
				return NULL;
			output[newcounter++] = '\\';
			output[newcounter++] = string[counter];
		} else {
			if (newcounter > (sizeof(output)-3))
				return NULL;
			output[newcounter++] = string[counter];
		}
	}

	if (outside_flag == 1)
		output[newcounter++] = '"';
	output[newcounter] = '\0';

	return output;
}

static fte_t
	toc_compare_nicks(const char *s1, const char *s2) {
	if ((s1 == NULL) || (s2 == NULL))
		return(FE_NOMATCH);

	while (*s1 == ' ')
		s1++;
	while (*s2 == ' ')
		s2++;
	while (*s1 != 0) {
		if (tolower((unsigned char)(*s1)) != tolower((unsigned char)(*s2)))
			return(FE_NOMATCH);
		s1++;
		s2++;
		while (*s1 == ' ')
			s1++;
		while (*s2 == ' ')
			s2++;
	}
	if (*s2 != 0)
		return(FE_NOMATCH);
	return(FE_SUCCESS);
}

static char *toc_hash_password(const char *const password) {
#define HASH "Tic/Toc"
	const char hash[sizeof(HASH)] = HASH;
	static char output[2048];
	size_t counter;
	int newcounter;
	size_t length;

	length = strlen(password);

	output[0] = '0';
	output[1] = 'x';

	newcounter = 2;

	for (counter = 0; counter < length; counter++) {
		if (newcounter > 2044)
			return NULL;
		sprintf(&output[newcounter],"%02x",(unsigned int) password[counter] ^ hash[((counter) % (sizeof(HASH)-1))]);
		newcounter += 2;
	}

	output[newcounter] = '\0';

	return output;
}

static int toc_internal_add_room(client_t c, const char *const name, const int exchange) {
	struct s_toc_room *iter;

	iter = c->room_head;
	c->room_head = calloc(1, sizeof(struct s_toc_room));
	if (c->room_head == NULL)
		abort();
	c->room_head->next = iter;
	c->room_head->name = strdup(name);
	if (c->room_head->name == NULL)
		abort();
	c->room_head->id = NULL;
	c->room_head->exchange = exchange;
	return FE_SUCCESS;
}

static int toc_internal_set_joined(client_t c, const char *const name, const int exchange) {
	struct s_toc_room *iter;

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if (iter->joined == 0) {
			if ((iter->exchange == exchange) && (toc_compare_nicks(iter->name, name) == 0)) {
				iter->joined = 1;
				return FE_SUCCESS;
			}
		}
	return FE_NOTFOUND;
}

static int toc_internal_set_id(client_t c, const char *const name, const int exchange, const char *const id) {
	struct s_toc_room *iter;

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if ((iter->joined == 0) && (iter->exchange == exchange) && (toc_compare_nicks(iter->name, name) == 0)) {
			free(iter->id);
			iter->id = strdup(id);
			if (iter->id == NULL)
				abort();
			return FE_SUCCESS;
		}
	return FE_NOTFOUND;
}
	
static int toc_internal_find_exchange(client_t c, const char *const name) {
	struct s_toc_room *iter;

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if (iter->joined == 0)
			if (toc_compare_nicks(iter->name, name) == 0)
				return iter->exchange;

	firetalkerror = FE_NOTFOUND;
	return 0;
}

static char *toc_internal_split_name(const char *const string) {
	return strchr(string, ':') + 1;
}

static int toc_internal_split_exchange(const char *const string) {
	return atoi(string);
}

static char *toc_internal_find_room_id(client_t c, const char *const name) {
	struct s_toc_room *iter;
	char *namepart;
	int exchange;

	namepart = toc_internal_split_name(name);
	exchange = toc_internal_split_exchange(name);

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if (iter->exchange == exchange)
			if (toc_compare_nicks(iter->name, namepart) == 0)
				return iter->id;

	firetalkerror = FE_NOTFOUND;
	return NULL;
}

static char *toc_internal_find_room_name(client_t c, const char *const id) {
	struct s_toc_room *iter;
	static char newname[2048];

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if (toc_compare_nicks(iter->id,id) == 0) {
			snprintf(newname,2048,"%d:%s",iter->exchange,iter->name);
			return newname;
		}

	firetalkerror = FE_NOTFOUND;
	return NULL;
}

static unsigned char toc_debase64(const char c) {
	if (c >= 'A' && c <= 'Z')
		return (unsigned char) (c - 'A');
	if (c >= 'a' && c <= 'z')
		return (unsigned char) ((char) 26 + (c - 'a'));
	if (c >= '0' && c <= '9')
		return (unsigned char) ((char) 52 + (c - '0'));
	if (c == '+')
		return (unsigned char) 62;
	if (c == '/')
		return (unsigned char) 63;
	return (unsigned char) 0;
}

static char *toc_get_tlv_value(char **args, const int startarg, const int type) {
	int i,o,s,l;
	static unsigned char out[256];

	i = startarg;
	while (args[i]) {
		if (atoi(args[i]) == type) {
			/* got it, now de-base 64 the next block */
			i++;
			o = -8;
			l = (int) strlen(args[i]);
			for (s = 0; s <= l - 3; s += 4) {
				if (o >= 0)
					out[o] = (toc_debase64(args[i][s]) << 2) | (toc_debase64(args[i][s+1]) >> 4);
				o++;
				if (o >= 0)
					out[o] = (toc_debase64(args[i][s+1]) << 4) | (toc_debase64(args[i][s+2]) >> 2);
				o++;
				if (o >= 0)
					out[o] = (toc_debase64(args[i][s+2]) << 6) | toc_debase64(args[i][s+3]);
				o++;
			}
			return (char *) out;
		}
		i += 2;
	}
	return NULL;
}

static int toc_internal_set_room_invited(client_t c, const char *const name, const int invited) {
	struct s_toc_room *iter;

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if (toc_compare_nicks(iter->name,name) == 0) {
			iter->invited = invited;
			return FE_SUCCESS;
		}

	return FE_NOTFOUND;
}

static int toc_internal_get_room_invited(client_t c, const char *const name) {
	struct s_toc_room *iter;

	for (iter = c->room_head; iter != NULL; iter = iter->next)
		if (toc_compare_nicks(aim_normalize_room_name(iter->name),name) == 0)
			return iter->invited;
	return -1;
}

static int toc_send_printf(client_t c, const char *const format, ...) {
	va_list	ap;
	size_t	i,
		datai = TOC_HEADER_LENGTH;
	char	data[2048];

	va_start(ap, format);
	for (i = 0; format[i] != 0; i++) {
		if (format[i] == '%') {
			switch (format[++i]) {
				case 'd': {
						int	i = va_arg(ap, int);

						i = snprintf(data+datai, sizeof(data)-datai, "%i", i);
						if (i > 0)
							datai += i;
						break;
					}
				case 's': {
						const char
							*s = toc_quote(va_arg(ap, char *), 1);
						size_t	slen = strlen(s);

						if ((datai+slen) > (sizeof(data)-1))
							return(FE_PACKETSIZE);
						strcpy(data+datai, s);
						datai += slen;
						break;
					}
				case 'S': {
						const char
							*s = va_arg(ap, char *);
						size_t	slen = strlen(s);

						if ((datai+slen) > (sizeof(data)-1))
							return(FE_PACKETSIZE);
						strcpy(data+datai, s);
						datai += slen;
						break;
					}
				case '%':
					data[datai++] = '%';
					break;
			}
		} else {
			data[datai++] = format[i];
			if (datai > (sizeof(data)-1))
				return(FE_PACKETSIZE);
		}
	}
	va_end(ap);

#ifdef DEBUG_ECHO
	toc_echof(c, "send_printf", "frame=%X, sequence=out:%i, length=%i<br>&gt;&gt;&gt;&nbsp;%s\n",
		SFLAP_FRAME_DATA, c->local_sequence+1,
		datai-TOC_HEADER_LENGTH, firetalk_nhtmlentities(data+TOC_HEADER_LENGTH, datai-TOC_HEADER_LENGTH));
#endif

	{
		struct s_firetalk_handle
			*fchandle;
		unsigned short
			length;

		fchandle = firetalk_find_handle(c);
		data[datai] = 0;
		length = toc_fill_header((unsigned char *)data, SFLAP_FRAME_DATA, ++c->local_sequence, datai-TOC_HEADER_LENGTH+1);
		firetalk_internal_send_data(fchandle, data, length);
	}
	return(FE_SUCCESS);
}

static char *toc_get_arg0(const char *const instring) {
	static char data[8192];
	char *tempchr;

	if (strlen(instring) > 8192) {
		firetalkerror = FE_PACKETSIZE;
		return NULL;
	}

	safe_strncpy(data,instring,8192);
	tempchr = strchr(data,':');
	if (tempchr)
		tempchr[0] = '\0';
	return data;
}

static char **toc_parse_args(char *const instring, const int maxargs) {
	static char *args[256];
	int curarg;
	char *tempchr;
	char *tempchr2;

	curarg = 0;
	tempchr = instring;

	while ((curarg < (maxargs - 1)) && (curarg < sizeof(args)/sizeof(*args)-1) && ((tempchr2 = strchr(tempchr, ':')) != NULL)) {
		args[curarg++] = tempchr;
		tempchr2[0] = '\0';
		tempchr = tempchr2 + 1;
	}
	args[curarg++] = tempchr;
	args[curarg] = NULL;
	return args;
}



static fte_t
	toc_isprint(const int c) {
	if ((c >= 0) && (c <= 255) && (isprint(c) || (c >= 160)))
		return(FE_SUCCESS);
	return(FE_INVALIDFORMAT);
}

static client_t
	toc_create_handle(void) {
	client_t c;

	c = calloc(1, sizeof(struct s_toc_connection));
	if (c == NULL)
		abort();
	c->nickname = NULL;
	c->room_head = NULL;
	c->infoget_head = NULL;
	c->lasttalk = time(NULL);
	c->profar = NULL;
	c->profstr = NULL;
	c->ectqar = NULL;

	return c;
}

static void
	toc_destroy_handle(client_t c) {
	(void) toc_internal_disconnect(c,FE_USERDISCONNECT);
	assert(c->nickname == NULL);
	assert(c->room_head == NULL);
	assert(c->infoget_head == NULL);
	assert(c->profstr == NULL);
	assert(c->profar == NULL);
	assert(c->ectqar == NULL);
	free(c);
}

static fte_t
	toc_disconnect(client_t c) {
	return toc_internal_disconnect(c,FE_USERDISCONNECT);
}

static fte_t
	toc_signon(client_t c, const char *const username) {
	struct s_firetalk_handle *conn;

	/* fill & send the flap signon packet */

	conn = firetalk_find_handle(c);

	c->lasttalk = time(NULL);
	c->connectstate = 0;
	c->gotconfig = 0;
	free(c->nickname);
	c->nickname = strdup(username);
	if (c->nickname == NULL)
		abort();

	/* send the signon string to indicate that we're speaking FLAP here */

#ifdef DEBUG_ECHO
	toc_echof(c, "signon", "frame=0, length=%i<br>&gt;&gt;&gt;&nbsp;%s\n", strlen(SIGNON_STRING), firetalk_htmlentities(SIGNON_STRING));
#endif
	firetalk_internal_send_data(conn,SIGNON_STRING,strlen(SIGNON_STRING));

	return FE_SUCCESS;
}

static fte_t
	toc_im_add_buddy(client_t c, const char *const _nickname, const char *const _group) {
	char	*name = toc_quote(_nickname, 1);
	int	slen = strlen(name);

	if ((c->buddybuflen+slen+1) >= sizeof(c->buddybuf)) {
		toc_send_printf(c, "toc_add_buddy%S", c->buddybuf);
		c->buddybuflen = 0;
	}
	if (c->online == 1) {
		strcpy(c->buddybuf+c->buddybuflen, " ");
		strcpy(c->buddybuf+c->buddybuflen+1, name);
		c->buddybuflen += slen+1;
	}
	return(FE_SUCCESS);
}

static fte_t
	toc_im_remove_buddy(client_t c, const char *const nickname) {
	return(toc_send_printf(c, "toc_remove_buddy %s", nickname));
}

static fte_t
	toc_im_add_deny(client_t c, const char *const nickname) {
	return(toc_send_printf(c, "toc_add_deny %s", nickname));
}

static fte_t
	toc_im_remove_deny(client_t c, const char *const nickname) {
	toc_send_printf(c, "toc_add_permit %s", nickname);
	return(toc_send_printf(c, "toc_add_deny"));
}

static fte_t
	toc_save_config(client_t c) {
	char	*lastgroup,
		data[2048];
	struct s_firetalk_handle *conn;
	struct s_firetalk_buddy *buddyiter;
	struct s_firetalk_deny *denyiter;

	lastgroup = strdup("Saved buddies");
	if (lastgroup == NULL)
		abort();

	conn = firetalk_find_handle(c);

	safe_strncpy(data, "m 4\n", 2048);
	for (buddyiter = conn->buddy_head; buddyiter != NULL; buddyiter = buddyiter->next) {
		if (strcasecmp(buddyiter->group, "User") == 0)
			continue;
		if (strlen(data) > 2000)
			return FE_PACKETSIZE;
		if (strcmp(buddyiter->group, lastgroup) != 0) {
			safe_strncat(data, "g ", 2048);
			safe_strncat(data, buddyiter->group, 2048);
			safe_strncat(data, "\n", 2048);
			free(lastgroup);
			lastgroup = strdup(buddyiter->group);
			if (lastgroup == NULL)
				abort();
		}
		safe_strncat(data, "b ", 2048);
		safe_strncat(data, buddyiter->nickname, 2048);
		safe_strncat(data, "\n", 2048);
	}
	free(lastgroup);
	lastgroup = NULL;
	for (denyiter = conn->deny_head; denyiter != NULL; denyiter = denyiter->next) {
		if (strlen(data) > 2000)
			return FE_PACKETSIZE;
		safe_strncat(data, "d ", 2048);
		safe_strncat(data, denyiter->nickname, 2048);
		safe_strncat(data, "\n", 2048);
	}
	return toc_send_printf(c,"toc_set_config %s",data);
}

static fte_t
	toc_im_upload_buddies(client_t c) {
	char	data[2048];
	struct s_firetalk_buddy *buddyiter;
	struct s_firetalk_handle *fchandle;
	unsigned short length; 

	fchandle = firetalk_find_handle(c);

	safe_strncpy(&data[TOC_HEADER_LENGTH], "toc_add_buddy", (size_t)2048 - TOC_HEADER_LENGTH);

	if (fchandle->buddy_head == NULL) {
		safe_strncat(&data[TOC_HEADER_LENGTH], " ", (size_t)2048 - TOC_HEADER_LENGTH);
		safe_strncat(&data[TOC_HEADER_LENGTH], toc_quote("naimhelp", 1), (size_t)2048 - TOC_HEADER_LENGTH);
	}

	for (buddyiter = fchandle->buddy_head; buddyiter != NULL; buddyiter = buddyiter->next) {
		safe_strncat(&data[TOC_HEADER_LENGTH], " ", (size_t)2048 - TOC_HEADER_LENGTH);
		safe_strncat(&data[TOC_HEADER_LENGTH], toc_quote(buddyiter->nickname, 1), (size_t)2048 - TOC_HEADER_LENGTH);
		if (strlen(&data[TOC_HEADER_LENGTH]) > 2000) {
			length = toc_fill_header((unsigned char *)data, SFLAP_FRAME_DATA, ++c->local_sequence, strlen(&data[TOC_HEADER_LENGTH])+1);

#ifdef DEBUG_ECHO
			toc_echo_send(c, "im_upload_buddies", (unsigned char *)data, length);
#endif
			firetalk_internal_send_data(fchandle, data, length);
			safe_strncpy(&data[TOC_HEADER_LENGTH], "toc_add_buddy", (size_t)2048 - TOC_HEADER_LENGTH);
		}
	}
	length = toc_fill_header((unsigned char *)data, SFLAP_FRAME_DATA, ++c->local_sequence, strlen(&data[TOC_HEADER_LENGTH])+1);

#ifdef DEBUG_ECHO
	toc_echo_send(c, "im_upload_buddies", (unsigned char *)data, length);
#endif
	firetalk_internal_send_data(fchandle, data, length);
	return FE_SUCCESS;
}

static fte_t
	toc_im_upload_denies(client_t c) {
	char	data[2048];
	struct s_firetalk_deny *denyiter;
	unsigned short length; 
	struct s_firetalk_handle *fchandle;

	fchandle = firetalk_find_handle(c);

	if (fchandle->deny_head == NULL)
		return FE_SUCCESS;

	safe_strncpy(&data[TOC_HEADER_LENGTH], "toc_add_deny", (size_t)2048 - TOC_HEADER_LENGTH);
	for (denyiter = fchandle->deny_head; denyiter != NULL; denyiter = denyiter->next) {
		safe_strncat(&data[TOC_HEADER_LENGTH], " ", (size_t)2048 - TOC_HEADER_LENGTH);
		safe_strncat(&data[TOC_HEADER_LENGTH], toc_quote(denyiter->nickname, 0), (size_t)2048 - TOC_HEADER_LENGTH);
		if (strlen(&data[TOC_HEADER_LENGTH]) > 2000) {
			length = toc_fill_header((unsigned char *)data, SFLAP_FRAME_DATA, ++c->local_sequence, strlen(&data[TOC_HEADER_LENGTH])+1);

#ifdef DEBUG_ECHO
			toc_echo_send(c, "im_upload_denies", (unsigned char *)data, length);
#endif
			firetalk_internal_send_data(fchandle, data, length);
			safe_strncpy(&data[TOC_HEADER_LENGTH], "toc_add_deny", (size_t)2048 - TOC_HEADER_LENGTH);
		}
	}
	length = toc_fill_header((unsigned char *)data, SFLAP_FRAME_DATA, ++c->local_sequence, strlen(&data[TOC_HEADER_LENGTH])+1);

#ifdef DEBUG_ECHO
	toc_echo_send(c, "im_upload_denies", (unsigned char *)data, length);
#endif
	firetalk_internal_send_data(fchandle, data, length);
	return FE_SUCCESS;
}

static fte_t
	toc_im_send_message(client_t c, const char *const dest, const char *const message, const int auto_flag) {
	if ((auto_flag == 0) && (toc_compare_nicks(dest,c->nickname) == FE_NOMATCH))
		c->lasttalk = time(NULL);

	if (strcasecmp(dest, ":RAW") == 0) {
		if (auto_flag != 1)
			return(toc_send_printf(c, "%S", message));
		else
			return(FE_SUCCESS);
	}

	if (auto_flag == 1)
		return toc_send_printf(c, "toc_send_im %s %s auto", dest, aim_interpolate_variables(message, dest));
	else {
		int	i;

		for (i = 0; i < c->ectqc; i++)
			if (toc_compare_nicks(dest, c->ectqar[i].dest) == 0) {
				char	*newstr = malloc(strlen(message) + strlen(c->ectqar[i].ect) + 1);

				if (newstr != NULL) {
					fte_t	fte;

					strcpy(newstr, message);
					strcat(newstr, c->ectqar[i].ect);
					fte = toc_send_printf(c, "toc_send_im %s %s",
						dest, newstr);
					free(newstr);
					newstr = NULL;

					free(c->ectqar[i].dest);
					free(c->ectqar[i].ect);
					memmove(c->ectqar+i, c->ectqar+i+1,
						c->ectqc-i-1);
					c->ectqc--;
					assert(c->ectqc >= 0);
					if (c->ectqc == 0) {
						free(c->ectqar);
						c->ectqar = NULL;
					} else
						c->ectqar = realloc(c->ectqar, (c->ectqc)*sizeof(*(c->ectqar)));

					return(fte);
				}
			}
		return(toc_send_printf(c, "toc_send_im %s %s", 
			dest, message));
	}
}

static fte_t
	toc_im_send_action(client_t c, const char *const dest, const char *const message, const int auto_flag) {
	char tempbuf[2048]; 

	if (strlen(message) > 2042)
		return FE_PACKETSIZE;

	safe_strncpy(tempbuf, "/me ", 2048);
	safe_strncat(tempbuf, message, 2048);

	if (auto_flag == 0)
		c->lasttalk = time(NULL);
	
	if (auto_flag == 1)
		return toc_send_printf(c, "toc_send_im %s %s auto", dest, tempbuf);
	else
		return toc_send_printf(c, "toc_send_im %s %s", dest, tempbuf);
}

static fte_t
	toc_get_info(client_t c, const char *const nickname) {
	safe_strncpy(lastinfo, nickname, (size_t)TOC_USERNAME_MAXLEN + 1);
	return toc_send_printf(c, "toc_get_info %s", nickname);
}

static fte_t
	toc_set_info(client_t c, const char *const info) {
	if (c->proflen > 0) {
		size_t	infolen = strlen(info);
		char	*newstr;

		if (((infolen = strlen(info)) + c->proflen) >= 1024) {
			firetalk_callback_error(c, FE_MESSAGETRUNCATED, NULL, "Profile too long");
			infolen = 1024 - c->proflen - 1;
		}

		newstr = malloc(infolen + c->proflen + 1);
		if (newstr != NULL) {
			fte_t	fte;

			strncpy(newstr, info, infolen);
			strcpy(newstr+infolen, c->profstr);
			fte = toc_send_printf(c, "toc_set_info %s", newstr);
			free(newstr);
			return(fte);
		}
	}
	return(toc_send_printf(c, "toc_set_info %s", info));
}

static fte_t
	toc_set_away(client_t c, const char *const message, const int auto_flag) {
#ifdef DEBUG_ECHO
	toc_echof(c, "set_away", "message=%#p \"%s\", auto_flag=%i\n", message, firetalk_htmlentities(message), auto_flag);
#endif
	if (message)
		return toc_send_printf(c, "toc_set_away %s", message);
	else
		return toc_send_printf(c, "toc_set_away");
}

static fte_t
	toc_set_nickname(client_t c, const char *const nickname) {
	return toc_send_printf(c, "toc_format_nickname %s", nickname);
}

static fte_t
	toc_set_password(client_t c, const char *const oldpass, const char *const newpass) {
	c->passchange++;
	return toc_send_printf(c, "toc_change_passwd %s %s", oldpass, newpass);
}

static fte_t
	toc_im_evil(client_t c, const char *const who) {
	return toc_send_printf(c, "toc_evil %s norm", who);
}

static fte_t
	toc_got_data(client_t c, unsigned char *buffer, unsigned short *bufferpos) {
	char *tempchr1;
	char *tempchr2;
	char *tempchr3;
	char data[8192 - TOC_HEADER_LENGTH + 1];
	char *arg0;
	char **args;
	fte_t	r;
	unsigned short l;

  got_data_start:
	r = toc_find_packet(c, buffer, bufferpos, data, SFLAP_FRAME_DATA, &l);
	if (r == FE_NOTFOUND)
		return FE_SUCCESS;
	else if (r != FE_SUCCESS)
		return r;

	arg0 = toc_get_arg0(data);
	if (!arg0)
		return FE_SUCCESS;
	if (strcmp(arg0, "ERROR") == 0) {
		args = toc_parse_args(data, 3);
		if (!args[1]) {
			(void)toc_internal_disconnect(c, FE_INVALIDFORMAT);
			return FE_INVALIDFORMAT;
		}
		switch (atoi(args[1])) {
			case 901:
				firetalk_callback_error(c, FE_USERUNAVAILABLE, args[2], NULL);
				break;
			case 902:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, args[2], NULL);
				break;
			case 903:
				firetalk_callback_error(c, FE_TOOFAST, NULL, NULL);
				break;
			case 911:
				if (c->passchange != 0) {
					c->passchange--;
					firetalk_callback_error(c, FE_NOCHANGEPASS, NULL, NULL);
				} else
					firetalk_callback_error(c, FE_BADUSER, NULL, NULL);
				break;
			case 915:
				firetalk_callback_error(c, FE_TOOFAST, NULL, NULL);
				break;
			case 950:
				firetalk_callback_error(c, FE_ROOMUNAVAILABLE, args[2], NULL);
				break;
			case 960:
				firetalk_callback_error(c, FE_TOOFAST, args[2], NULL);
				break;
			case 961:
				firetalk_callback_error(c, FE_INCOMINGERROR, args[2], "Message too big.");
				break;
			case 962:
				firetalk_callback_error(c, FE_INCOMINGERROR, args[2], "Message sent too fast.");
				break;
			case 970:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, NULL);
				break;
			case 971:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Too many matches.");
				break;
			case 972:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Need more qualifiers.");
				break;
			case 973:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Directory service unavailable.");
				break;
			case 974:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Email lookup restricted.");
				break;
			case 975:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Keyword ignored.");
				break;
			case 976:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "No keywords.");
				break;
			case 977:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Language not supported.");
				break;
			case 978:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, "Country not supported.");
				break;
			case 979:
				firetalk_callback_error(c, FE_USERINFOUNAVAILABLE, NULL, NULL);
				break;
			case 980:
				firetalk_callback_error(c, FE_BADUSERPASS, NULL, NULL);
				break;
			case 981:
				firetalk_callback_error(c, FE_UNKNOWN, NULL, "Service temporarily unavailable.");
				break;
			case 982:
				firetalk_callback_error(c, FE_BLOCKED, NULL, "Your warning level is too high to sign on.");
				break;
			case 983:
				firetalk_callback_error(c, FE_BLOCKED, NULL, "You have been connected and disconnecting too frequently. Wait 10 minutes.");
				break;
			case 989:
				firetalk_callback_error(c, FE_UNKNOWN, NULL, args[1]);
				break;
			default:
				firetalk_callback_error(c, FE_UNKNOWN, NULL, args[1]);
				break;
		}
	} else if (strcmp(arg0, "PAUSE") == 0) {
		c->connectstate = 1;
		firetalk_internal_set_connectstate(c, FCS_WAITING_SIGNON);
	} else if (strcmp(arg0, "IM_IN") == 0) {
		args = toc_parse_args(data, 4);
		if ((args[1] == NULL) || (args[2] == NULL) || (args[3] == NULL)) {
			(void)toc_internal_disconnect(c, FE_INVALIDFORMAT);
			return FE_INVALIDFORMAT;
		}
		(void)aim_handle_ect(c, args[1], args[3], args[2][0] == 'T'?1:0);
		if (strlen(args[3]) > 0) {
			char *mestart;

			if (strncasecmp(args[3],"/me ",4) == 0)
				firetalk_callback_im_getaction(c, args[1], args[2][0] == 'T'?1:0, &args[3][4]);
			else if ((mestart = strstr(args[3],">/me ")) != NULL)
				firetalk_callback_im_getaction(c, args[1], args[2][0] == 'T'?1:0, &mestart[5]);
			else {
				if (args[2][0] == 'T') /* interpolate only auto-messages */
					firetalk_callback_im_getmessage(c, args[1], 1, aim_interpolate_variables(args[3], c->nickname));
				else
					firetalk_callback_im_getmessage(c, args[1], 0, args[3]);
			}
		}
	} else if (strcmp(arg0, "UPDATE_BUDDY") == 0) {
		args = toc_parse_args(data,7);
		if (!args[1] || !args[2] || !args[3] || !args[4] || !args[5] || !args[6]) {
			(void)toc_internal_disconnect(c, FE_INVALIDFORMAT);
			return FE_INVALIDFORMAT;
		}
		firetalk_callback_im_buddyonline(c, args[1], args[2][0] == 'T'?1:0);
		firetalk_callback_user_nickchanged(c, args[1], args[1]);
		firetalk_callback_im_buddyaway(c, args[1], args[6][2] == 'U'?1:0);
		firetalk_callback_idleinfo(c, args[1], atol(args[5]));
	} else if (strcmp(arg0, "GOTO_URL") == 0) {
		struct s_toc_infoget *i;
		/* create a new infoget object and set it to connecting state */
		args = toc_parse_args(data, 3);
		if (!args[1] || !args[2]) {
			(void)toc_internal_disconnect(c, FE_INVALIDFORMAT);
			return FE_INVALIDFORMAT;
		}
		i = c->infoget_head;
		c->infoget_head = calloc(1, sizeof(struct s_toc_infoget));
		if (c->infoget_head == NULL)
			abort();
#define TOC_HTTP_REQUEST "GET /%s HTTP/1.0\r\n\r\n"

		snprintf(c->infoget_head->buffer, TOC_HTML_MAXLEN, TOC_HTTP_REQUEST, args[2]);
		c->infoget_head->buflen = strlen(c->infoget_head->buffer);
		c->infoget_head->next = i;
		c->infoget_head->state = TOC_STATE_CONNECTING;
		c->infoget_head->sockfd = firetalk_internal_connect(firetalk_internal_remotehost4(c)
#ifdef _FC_USE_IPV6
				, firetalk_internal_remotehost6(c)
#endif
				);
		if (c->infoget_head->sockfd == -1) {
			firetalk_callback_error(c, FE_CONNECT, (lastinfo[0] == '\0')?NULL:lastinfo, "Failed to connect to info server");
			free(c->infoget_head);
			c->infoget_head = i;
			return FE_SUCCESS;
		}
	} else if (strcmp(arg0, "EVILED") == 0) {
		args = toc_parse_args(data, 3);
		if (!args[1]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "EVILED");
			return FE_SUCCESS;
		}
		firetalk_callback_eviled(c, atoi(args[1]), args[2]);
	} else if (strcmp(arg0, "CHAT_JOIN") == 0) {
		int i;
		args = toc_parse_args(data, 3);
		if (!args[1] || !args[2]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "CHAT_JOIN");
			return FE_SUCCESS;
		}
		i = toc_internal_find_exchange(c, args[2]);
		if (i != 0)
			if (toc_internal_set_id(c, args[2], i, args[1]) == FE_SUCCESS)
				if (toc_internal_set_joined(c, args[2], i) == FE_SUCCESS)
					firetalk_callback_chat_joined(c, toc_internal_find_room_name(c, args[1]));
	} else if (strcmp(arg0, "CHAT_LEFT") == 0) {
		args = toc_parse_args(data, 2);
		if (!args[1]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "CHAT_LEFT");
			return FE_SUCCESS;
		}
		firetalk_callback_chat_left(c, toc_internal_find_room_name(c, args[1]));
	} else if (strcmp(arg0, "CHAT_IN") == 0) {
		char *mestart;

		args = toc_parse_args(data,5);
		if (!args[1] || !args[2] || !args[3] || !args[4]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "CHAT_IN");
			return FE_SUCCESS;
		}
#ifdef DEBUG_ECHO
			toc_echof(c, "got_data", "CHAT_IN args[1]=%s, args[2]=%s, args[3]=%s, args[4]=%s\n",
				args[1], args[2], args[3], firetalk_htmlentities(args[4]));
#endif
		if (strncasecmp(args[4], "<HTML><PRE>", 11) == 0) {
			args[4] = &args[4][11];
			if ((tempchr1 = strchr(args[4], '<')))
				tempchr1[0] = '\0';
		}
		if (strncasecmp(args[4], "/me ", 4) == 0)
			firetalk_callback_chat_getaction(c, toc_internal_find_room_name(c, args[1]), args[2], 0, &args[4][4]);
		else if ((mestart = strstr(args[4], ">/me ")) != NULL)
			firetalk_callback_chat_getaction(c, toc_internal_find_room_name(c, args[1]), args[2], 0, &mestart[5]);
		else
			firetalk_callback_chat_getmessage(c, toc_internal_find_room_name(c, args[1]), args[2], 0, args[4]);
	} else if (strcmp(arg0, "CHAT_INVITE") == 0) {
		args = toc_parse_args(data, 5);
		if (!args[1] || !args[2] || !args[3] || !args[4]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "CHAT_INVITE");
			return FE_SUCCESS;
		}
		if (toc_internal_add_room(c, args[1], 4) == FE_SUCCESS)
			if (toc_internal_set_room_invited(c, args[1], 1) == FE_SUCCESS)
				if (toc_internal_set_id(c, args[1], 4, args[2]) == FE_SUCCESS)
					firetalk_callback_chat_invited(c, args[1], args[3], args[4]);
	} else if (strcmp(arg0, "CHAT_UPDATE_BUDDY") == 0) {
		args = toc_parse_args(data, 4);
		if (!args[1] || !args[2] || !args[3]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "CHAT_UPDATE_BUDDY");
			return FE_SUCCESS;
		}
		tempchr1 = args[3];
		tempchr3 = toc_internal_find_room_name(c, args[1]);
		while ((tempchr2 = strchr(tempchr1, ':'))) {
			/* cycle through list of buddies */
			tempchr2[0] = '\0';
			if (args[2][0] == 'T')
				firetalk_callback_chat_user_joined(c, tempchr3, tempchr1);
			else
				firetalk_callback_chat_user_left(c, tempchr3, tempchr1, NULL);
			tempchr1 = tempchr2 + 1;
		}
		if (args[2][0] == 'T') {
			firetalk_callback_chat_user_joined(c, tempchr3, tempchr1);
			firetalk_callback_chat_user_joined(c, tempchr3, NULL);
		} else
			firetalk_callback_chat_user_left(c, tempchr3, tempchr1, NULL);
	} else if (strcmp(arg0,"ADMIN_NICK_STATUS") == 0) {
		/* ignore this one */
	} else if (strcmp(arg0,"DIR_STATUS") == 0) {
		/* ignore this one */
	} else if (strcmp(arg0, "NICK") == 0) {
		args = toc_parse_args(data, 2);
		if (!args[1]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "NICK");
			return FE_SUCCESS;
		}
		firetalk_callback_user_nickchanged(c,c->nickname, args[1]);
		free(c->nickname);
		c->nickname = strdup(args[1]);
		if (c->nickname == NULL)
			abort();
		firetalk_callback_newnick(c, args[1]);
	} else if (strcmp(arg0, "ADMIN_PASSWD_STATUS") == 0) {
		c->passchange--;
		args = toc_parse_args(data, 3);
		if (!args[1]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "ADMIN_PASSWD_STATUS");
			return FE_SUCCESS;
		}
		if (atoi(args[1]) != 0)
			firetalk_callback_error(c, FE_NOCHANGEPASS, NULL, NULL);
		else
			firetalk_callback_passchanged(c);
	} else if (strcmp(arg0, "RVOUS_PROPOSE") == 0) {
		args = toc_parse_args(data, 255);
		if (!args[1] || !args[2] || !args[3] || !args[4] || !args[5] || !args[6] || !args[7] || !args[8]) {
			firetalk_callback_error(c, FE_INVALIDFORMAT, NULL, "RVOUS_PROPOSE");
			return FE_SUCCESS;
		}
		if (strcmp(args[2], "09461343-4C7F-11D1-8222-444553540000") == 0)
			firetalk_callback_file_offer(c, args[1], toc_get_tlv_value(args, 9, 10001), -1, args[7], NULL, (uint16_t)atoi(args[8]), FF_TYPE_RAW);
	} else
		firetalk_callback_error(c, FE_WEIRDPACKET, NULL, arg0);

	goto got_data_start;
}

static fte_t
	toc_got_data_connecting(client_t c, unsigned char *buffer, unsigned short *bufferpos) {
	char data[8192 - TOC_HEADER_LENGTH + 1];
	char password[128];
	fte_t	r;
	unsigned short length;
	char *arg0;
	char **args;
	char *tempchr1;
	char *tempchr2;
	int permit_mode;
	firetalk_t fchandle;

  got_data_connecting_start:
	r = toc_find_packet(c,buffer,bufferpos,data,c->connectstate == 0 ? SFLAP_FRAME_SIGNON : SFLAP_FRAME_DATA,&length);
	if (r == FE_NOTFOUND)
		return FE_SUCCESS;
	else if (r != FE_SUCCESS)
		return r;

	switch (c->connectstate) {
		case 0: /* we're waiting for the flap version number */
			if (length != TOC_HOST_SIGNON_LENGTH) {
				firetalk_callback_connectfailed(c,FE_PACKETSIZE,"Host signon length incorrect");
				return FE_PACKETSIZE;
			}
			if (data[0] != '\0' || data[1] != '\0' || data[2] != '\0' || data[3] != '\1') {
				firetalk_callback_connectfailed(c,FE_VERSION,NULL);
				return FE_VERSION;
			}
			srand((unsigned int) time(NULL));
			c->local_sequence = (unsigned short) 1+(unsigned short) (65536.0*rand()/(RAND_MAX+1.0));

			length = toc_fill_header((unsigned char *) data,SFLAP_FRAME_SIGNON,++c->local_sequence,toc_fill_signon((unsigned char *)&data[TOC_HEADER_LENGTH],c->nickname));

			fchandle = firetalk_find_handle(c);
#ifdef DEBUG_ECHO
			toc_echo_send(c, "got_data_connecting", (unsigned char *)data, length);
#endif
			firetalk_internal_send_data(fchandle, data, length);

			firetalk_callback_needpass(c, password, 128);

			c->connectstate = 1;
			r = toc_send_printf(c,"toc_signon login.oscar.aol.com 5190 %s %s english \"" PACKAGE_NAME ":" PACKAGE_VERSION ":contact " PACKAGE_BUGREPORT "\"",c->nickname,toc_hash_password(password));
			if (r != FE_SUCCESS) {
				firetalk_callback_connectfailed(c,r,"Error sending login string");
				return r;
			}
			break;
		case 1:
			arg0 = toc_get_arg0(data);
			if (strcmp(arg0,"SIGN_ON") != 0) {
				if (strcmp(arg0,"ERROR") == 0) {
					args = toc_parse_args(data,3);
					if (args[1]) {
						switch (atoi(args[1])) {
						  case 980:
							firetalk_callback_connectfailed(c,FE_BADUSERPASS,NULL);
							return FE_BADUSERPASS;
						  case 981:
							firetalk_callback_connectfailed(c,FE_SERVER,NULL);
							return FE_SERVER;
						  case 982:
							firetalk_callback_connectfailed(c,FE_BLOCKED,"Your warning level is too high to sign on");
							return FE_BLOCKED;
						  case 983:
							firetalk_callback_connectfailed(c,FE_BLOCKED,"You have been connecting and disconnecting too frequently");
							return FE_BLOCKED;
						  default: {
							char	buf[1024];

							if (args[2] != NULL)
								snprintf(buf, sizeof(buf), "Unrecognized server error %s: %s", args[1], args[2]);
							else
								snprintf(buf, sizeof(buf), "Unrecognized server error %s (no further diagnostics available)", args[1]);
							firetalk_callback_connectfailed(c,FE_UNKNOWN,buf);
							return FE_UNKNOWN;
						  }
						}
					} else {
						firetalk_callback_connectfailed(c,FE_UNKNOWN,"No error code supplied; this is probably a server failure");
						return FE_UNKNOWN;
					}
				}
				firetalk_callback_connectfailed(c,FE_UNKNOWN,data);
				return FE_UNKNOWN;
			}
			c->connectstate = 2;
			break;
		case 2:
		case 3:
			arg0 = toc_get_arg0(data);
			if (arg0 == NULL)
				return FE_SUCCESS;
			if (strcmp(arg0,"NICK") == 0) {
				args = toc_parse_args(data,2);
				if (args[1]) {
					free(c->nickname);
					c->nickname = strdup(args[1]);
					if (c->nickname == NULL)
						abort();
				}
				c->connectstate = 3;
			} else if (strcmp(arg0,"CONFIG") == 0) {
				char	*group;

				group = strdup("Saved buddy");
				if (group == NULL)
					abort();

				fchandle = firetalk_find_handle(c);
				args = toc_parse_args(data,2);
				if (!args[1]) {
					firetalk_callback_connectfailed(c,FE_INVALIDFORMAT,"CONFIG");
					return FE_INVALIDFORMAT;
				}
				tempchr1 = args[1];
				permit_mode = 0;
				c->gotconfig = 1;
				while ((tempchr2 = strchr(tempchr1,'\n'))) {
					/* rather stupidly, we have to tell it everything back that it just told us.  how dumb */
					*tempchr2 = 0;
					switch (tempchr1[0]) {
					  case 'm':	/* permit mode */
						permit_mode = ((int)(tempchr1[2] - '0'));
						break;
					  case 'd':	/* deny */
						firetalk_im_internal_add_deny(fchandle, tempchr1+2);
						break;
					  case 'b':	/* buddy */
						firetalk_im_add_buddy(fchandle, tempchr1+2, group);
						break;
					  case 'g':	/* group */
						free(group);
						group = strdup(tempchr1+2);
						if (group == NULL)
							abort();
						break;
					}
					tempchr1 = tempchr2+1;
				}
				free(group);
				group = NULL;
				if (permit_mode != 4)
					r = toc_send_printf(c, "toc_add_deny");
					if (r != FE_SUCCESS) {
						firetalk_callback_connectfailed(c,r,"Error re-uploading denies");
						return r;
					}
			} else {
				firetalk_callback_connectfailed(c,FE_WEIRDPACKET,arg0);
				return FE_WEIRDPACKET;
			}
			if (c->gotconfig == 1 && c->connectstate == 3) {
				/* ask the client to handle its init */
				firetalk_callback_doinit(c,c->nickname);
				if (toc_im_upload_buddies(c) != FE_SUCCESS) {
					firetalk_callback_connectfailed(c,FE_PACKET,"Error uploading buddies");
					return FE_PACKET;
				}
				if (toc_im_upload_denies(c) != FE_SUCCESS) {
					firetalk_callback_connectfailed(c,FE_PACKET,"Error uploading denies");
					return FE_PACKET;
				}
				r = toc_send_printf(c,"toc_set_dir :");
				if (r != FE_SUCCESS) {
					firetalk_callback_connectfailed(c,r,"Error clearing directory");
					return r;
				}
				r = toc_send_printf(c,"toc_init_done");
				if (r != FE_SUCCESS) {
					firetalk_callback_connectfailed(c,r,"Error completing signon");
					return r;
				}

				c->online = 1;

#if 0
				r = toc_send_printf(c,"toc_set_caps 09461343-4C7F-11D1-8222-444553540000");
				if (r != FE_SUCCESS) {
					firetalk_callback_connectfailed(c,r,"Error setting capabilities");
					return r;
				}
#endif
				firetalk_callback_connected(c);
				return FE_SUCCESS;
			}
			break;

	}
	goto got_data_connecting_start;
}

static fte_t
	toc_periodic(struct s_firetalk_handle *const c) {
	struct s_toc_connection *conn;
	long idle;
	char data[32];

	conn = c->handle;

	if (firetalk_internal_get_connectstate(conn) != FCS_ACTIVE)
		return FE_NOTCONNECTED;

	idle = (long)(time(NULL) - conn->lasttalk);
	firetalk_callback_setidle(conn, &idle);

	if (idle < 600)
		idle = 0;

	if (idle/60 == conn->lastidle/60)
		return FE_IDLEFAST;

	if ((conn->lastidle/60 == 0) && (idle/60 == 0))
		return(FE_IDLEFAST);
	if ((conn->lastidle/60 != 0) && (idle/60 != 0))
		return(FE_IDLEFAST);

	conn->lastidle = idle;
	sprintf(data, "%ld", idle);
	return(toc_send_printf(conn, "toc_set_idle %s", data));
}

static fte_t
	toc_chat_join(client_t c, const char *const room) {
	int i;
	i = toc_internal_get_room_invited(c,room);
	if (i == 1) {
		(void) toc_internal_set_room_invited(c,room,0);
		return toc_send_printf(c,"toc_chat_accept %s",toc_internal_find_room_id(c,room));
	} else {
		int m;
		char *s;
		(void) toc_internal_add_room(c,s = toc_internal_split_name(room),m = toc_internal_split_exchange(room));
		return toc_send_printf(c,"toc_chat_join %d %s",m,s);
	}
}

static fte_t
	toc_chat_part(client_t c, const char *const room) {
	char *temp = toc_internal_find_room_id(c,room);
	if (!temp)
		return FE_ROOMUNAVAILABLE;
	return toc_send_printf(c,"toc_chat_leave %s",temp);
}

static fte_t
	toc_chat_set_topic(client_t c, const char *const room, const char *const topic) {
	return FE_SUCCESS;
}

static fte_t
	toc_chat_op(client_t c, const char *const room, const char *const who) {
	return FE_SUCCESS;
}

static fte_t
	toc_chat_deop(client_t c, const char *const room, const char *const who) {
	return FE_SUCCESS;
}

static fte_t
	toc_chat_kick(client_t c, const char *const room, const char *const who, const char *const reason) {
	return FE_SUCCESS;
}

static fte_t
	toc_chat_send_message(client_t c, const char *const room, const char *const message, const int auto_flag) {
	if (strlen(message) > 232)
		return(FE_PACKETSIZE);

	if (strcasecmp(room, ":RAW") == 0) {
		if (auto_flag != 1)
			return(toc_send_printf(c, "%S", message));
		else
			return(FE_SUCCESS);
	} else {
		char *temp = toc_internal_find_room_id(c,room);

		if (!temp)
			return FE_ROOMUNAVAILABLE;
		return toc_send_printf(c,"toc_chat_send %s %s",temp,message);
	}
}

static fte_t
	toc_chat_send_action(client_t c, const char *const room, const char *const message, const int auto_flag) {
	char tempbuf[2048];

	if (strlen(message) > 232-4)
		return FE_PACKETSIZE;

	safe_strncpy(tempbuf,"/me ",2048);
	safe_strncat(tempbuf,message,2048);
	return toc_send_printf(c,"toc_chat_send %s %s",toc_internal_find_room_id(c,room),tempbuf);
}

static fte_t
	toc_chat_invite(client_t c, const char *const room, const char *const who, const char *const message) {
	char *roomid;
	roomid = toc_internal_find_room_id(c,room);
	if (roomid != NULL)
		return toc_send_printf(c,"toc_chat_invite %s %s %s",toc_internal_find_room_id(c,room),message,who);
	else
		return FE_NOTFOUND;
}

static void
	ect_prof(client_t c, const char *const command,
		const char *const ect) {
	int	i;
	size_t	len = 0;

	for (i = 0; i < c->profc; i++)
		if (strcmp(c->profar[i].command, command) == 0)
			break;
	if (i == c->profc) {
		c->profc++;
		c->profar = realloc(c->profar, (c->profc)*sizeof(*(c->profar)));
		c->profar[i].command = strdup(command);
		c->profar[i].ect = strdup(ect);
	} else {
		free(c->profar[i].ect);
		c->profar[i].ect = strdup(ect);
	}

	for (i = 0; i < c->profc; i++)
		len += strlen(c->profar[i].ect);
	free(c->profstr);
	c->profstr = malloc(len+1);
	*(c->profstr) = 0;
	for (i = 0; i < c->profc; i++)
		strcat(c->profstr, c->profar[i].ect);
	c->proflen = len;
}

static void
	ect_queue(client_t c, const char *const to,
		const char *const ect) {
	int	i;

	for (i = 0; i < c->ectqc; i++)
		if (toc_compare_nicks(to, c->ectqar[i].dest) == 0) {
			char	*newstr = malloc(strlen(c->ectqar[i].ect)
					+ strlen(ect) + 1);

			if (newstr != NULL) {
				strcpy(newstr, c->ectqar[i].ect);
				strcat(newstr, ect);
				free(c->ectqar[i].ect);
				c->ectqar[i].ect = newstr;
				return;
			}
		}

	c->ectqc++;
	c->ectqar = realloc(c->ectqar, (c->ectqc)*sizeof(*(c->ectqar)));
	c->ectqar[i].dest = strdup(to);
	c->ectqar[i].ect = strdup(ect);
}

static char
	*subcode_ect(char *buf, size_t buflen, const char *const command,
		const char *const args) {
	if (command == NULL)
		return(NULL);

	if (args == NULL)
		snprintf(buf, buflen, "<font ECT=\"%s\"></font>",
			command);
	else
		snprintf(buf, buflen, "<font ECT=\"%s %s\"></font>",
			command, firetalk_htmlentities(args));
	return(buf);
}

static fte_t
	toc_subcode_send_request(client_t c, const char *const to, const char *const command, const char *const args) {
	char	buf[8*1024],
		*ect;

	if (isdigit(c->nickname[0]))
		return(FE_SUCCESS);
	if ((ect = subcode_ect(buf, sizeof(buf), command, args)) == NULL)
		return(FE_SUCCESS);

	ect_queue(c, to, ect);
	return(FE_SUCCESS);
}

static fte_t
	toc_subcode_send_reply(client_t c, const char *const to, const char *const command, const char *const args) {
	char	buf[8*1024],
		*ect;

	if ((ect = subcode_ect(buf, sizeof(buf), command, args)) == NULL)
		return(FE_SUCCESS);

	if (to != NULL)
		return(toc_im_send_message(c, to, ect, 1));
	else {
		if (args != NULL)
			ect_prof(c, command, ect);
		else
			ect_prof(c, command, "");

		return(FE_SUCCESS);
	}
}

const firetalk_protocol_t firetalk_protocol_toc = {
	strprotocol:		"TOC",
	default_server:		"toc.n.ml.org",
	default_port:		9898,
	default_buffersize:	1024*8,
	periodic:		toc_periodic,
	preselect:		toc_preselect,
	postselect:		toc_postselect,
	got_data:		toc_got_data,
	got_data_connecting:	toc_got_data_connecting,
	comparenicks:		toc_compare_nicks,
	isprintable:		toc_isprint,
	disconnect:		toc_disconnect,
	signon:			toc_signon,
	save_config:		toc_save_config,
	get_info:		toc_get_info,
	set_info:		toc_set_info,
	set_away:		toc_set_away,
	set_nickname:		toc_set_nickname,
	set_password:		toc_set_password,
	im_add_buddy:		toc_im_add_buddy,
	im_remove_buddy:	toc_im_remove_buddy,
	im_add_deny:		toc_im_add_deny,
	im_remove_deny:		toc_im_remove_deny,
	im_upload_buddies:	toc_im_upload_buddies,
	im_upload_denies:	toc_im_upload_denies,
	im_send_message:	toc_im_send_message,
	im_send_action:		toc_im_send_action,
	im_evil:		toc_im_evil,
	chat_join:		toc_chat_join,
	chat_part:		toc_chat_part,
	chat_invite:		toc_chat_invite,
	chat_set_topic:		toc_chat_set_topic,
	chat_op:		toc_chat_op,
	chat_deop:		toc_chat_deop,
	chat_kick:		toc_chat_kick,
	chat_send_message:	toc_chat_send_message,
	chat_send_action:	toc_chat_send_action,
	subcode_send_request:	toc_subcode_send_request,
	subcode_send_reply:	toc_subcode_send_reply,
	room_normalize:		aim_normalize_room_name,
	create_handle:		toc_create_handle,
	destroy_handle:		toc_destroy_handle,
};
