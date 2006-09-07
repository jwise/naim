/* irc.c - FireTalk IRC protocol driver
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
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "firetalk-int.h"
#include "firetalk.h"

#define ROOMSTARTS "#+&"


static char irc_tolower(const char c) {
	if ((c >= 'A') && (c <= 'Z'))
		return((c - 'A') + 'a');
	if (c == '[')
		return('{');
	if (c == ']')
		return('{');
	if (c == '\\')
		return('|');
	return(c);
}

static int irc_compare_nicks_int(const char *const nick1, const char *const nick2) {
	int	i = 0;

        assert(nick1 != NULL);
        assert(nick2 != NULL);

	while (nick1[i] != '\0') {
		if (irc_tolower(nick1[i]) != irc_tolower(nick2[i]))
			return(1);
		i++;
	}
	if (nick2[i] != '\0')
		return(1);

	return(0);
}

typedef struct irc_whois_t {
	struct irc_whois_t *next;
	char	*nickname,
		*info;
	int	flags;
	long	online,
		idle;
} irc_whois_t;

ZERO_CTOR(irc_whois_t);
TYPE_NEW(irc_whois_t);
static inline void irc_whois_t_dtor(irc_whois_t *this) {
	free(this->nickname);
	free(this->info);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(irc_whois_t);
LIST_DELETE(irc_whois_t);

typedef struct firetalk_driver_connection_t {
	char	*nickname,
		*password,
		*chanmodes,
		*chanprefix,
		 buffer[512+1];
	irc_whois_t *whois_head;
	int	 maxmodes;
	unsigned char
		 nosilence:1;	/* are we on a network that understands SILENCE */
} irc_conn_t;

static inline void irc_conn_t_ctor(irc_conn_t *this) {
	memset(this, 0, sizeof(*this));
	this->chanmodes = strdup("b,k,l,imnpst");
	this->chanprefix = strdup("(ov)@+");
	this->maxmodes = 3;
}
TYPE_NEW(irc_conn_t);
static inline void irc_conn_t_dtor(irc_conn_t *this) {
	free(this->nickname);
	free(this->password);
	free(this->chanmodes);
	free(this->chanprefix);
	irc_whois_t_list_delete(this->whois_head);
	memset(this, 0, sizeof(*this));
}
TYPE_DELETE(irc_conn_t);

#if 0
static const char *const irc_normalize_user_nick(const char *const name) {
	static char
		buf[512];
	int	i;

	if (strchr(name, '!') == NULL)
		return(name);

	for (i = 0; (i < sizeof(buf)) && (name[i] != '!'); i++)
		buf[i] = name[i];
	buf[i] = 0;
	return(buf);
}

static const char *const irc_normalize_user_mask(const char *const name) {
	static char
		buf[512];

	if (strchr(name, '!') != NULL)
		return(name);

	snprintf(buf, sizeof(buf), "%s!*@*", name);
	return(buf);
}
#endif

static void irc_disc_user_rem(irc_conn_t *c, const char *disc, const char *name) {
	firetalk_connection_t *fchandle = firetalk_find_conn(c);

	if (firetalk_user_visible_but(fchandle, disc, name) == FE_NOMATCH)
		firetalk_callback_im_buddyonline(c, name, 0);
}

static void irc_disc_rem(irc_conn_t *c, const char *disc) {
	firetalk_connection_t *fchandle = firetalk_find_conn(c);
	firetalk_room_t *iter;
	firetalk_member_t *mem;

	iter = firetalk_find_room(fchandle, disc);
	assert(iter != NULL);

	for (mem = iter->member_head; mem != NULL; mem = mem->next)
		irc_disc_user_rem(c, disc, mem->nickname);
}

static const char *irc_normalize_room_name(const char *const name) {
	static char	newname[2048];

	if (strchr(ROOMSTARTS, *name))
		return(name);
	snprintf(newname, sizeof(newname), "#%s", name);
	return(newname);
}






#ifdef DEBUG_ECHO
static void irc_echof(irc_conn_t *c, const char *const where, const char *const format, ...) {
	va_list	ap;
	char	buf[513];
	void	statrefresh(void);

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	while (buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = 0;
	if (*buf != 0)
		firetalk_callback_chat_getmessage(c, ":DEBUG", where, 0, buf);

	statrefresh();
}
#endif

static fte_t irc_compare_nicks(const char *const nick1, const char *const nick2) {
	if (irc_compare_nicks_int(nick1, nick2) == 0)
		return(FE_SUCCESS);
	return(FE_NOMATCH);
}

static fte_t irc_isprint(const int c) {
	if (isprint(c))
		return(FE_SUCCESS);
	return(FE_INVALIDFORMAT);
}

static fte_t irc_isnickfirst(const int c) {
	return(isalpha(c) || (c == '_') || (c == '[') || (c == ']') || (c == '\\') || (c == '`') || (c == '^') || (c == '{') || (c == '}'));
}

#if 0
static fte_t irc_isnick(const int c) {
	return(irc_isnickfirst(c) || isdigit(c) || (c == '-'));
}
#endif

static char *irc_html_to_irc(const char *const string) {
	static char *output = NULL;
	int o = 0;
	size_t l,i=0;

	l = strlen(string);

	output = realloc(output, (l * 4) + 1);
	if (output == NULL)
		abort();

	while (i < l) {
		assert(o < ((l * 4) + 1));
		switch(string[i]) {
			case '&':
				if (!strncasecmp(&string[i],"&amp;",5)) {
					output[o++] = '&';
					i += 5;
				} else if (!strncasecmp(&string[i],"&gt;",4)) {
					output[o++] = '>';
					i += 4;
				} else if (!strncasecmp(&string[i],"&lt;",4)) {
					output[o++] = '<';
					i += 4;
				} else if (!strncasecmp(&string[i],"&nbsp;",6)) {
					output[o++] = ' ';
					i += 6;
				} else
					output[o++] = string[i++];
				break;
			case '<':
				if (!strncasecmp(&string[i],"<b>",3)) {
					output[o++] = (char) 2;
					i += 3;
				} else if (!strncasecmp(&string[i],"</b>",4)) {
					output[o++] = (char) 2;
					i += 4;
				} else if (!strncasecmp(&string[i],"<i>",3)) {
					output[o++] = (char) 22;
					i += 3;
				} else if (!strncasecmp(&string[i],"</i>",4)) {
					output[o++] = (char) 22;
					i += 4;
				} else if (!strncasecmp(&string[i],"<u>",3)) {
					output[o++] = (char) 31;
					i += 3;
				} else if (!strncasecmp(&string[i],"</u>",4)) {
					output[o++] = (char) 31;
					i += 4;
				} else if (!strncasecmp(&string[i],"<br>",4)) {
					output[o++] = '\020';
					output[o++] = 'r';
					output[o++] = '\020';
					output[o++] = 'n';
					i += 4;
				} else
					output[o++] = string[i++];
				break;
			case '\r':
				output[o++] = '\020';
				output[o++] = 'r';
				i++;
				break;
			case '\n':
				output[o++] = '\020';
				output[o++] = 'n';
				i++;
				break;
			case '\020':
				output[o++] = '\020';
				output[o++] = '\020';
				i++;
				break;
			default:
				output[o++] = string[i++];
				break;
		}
	}
	assert(o <= ((l * 4) + 1));
	output[o] = '\0';
	return(output);
}

static const char *mIRCar[] = {
	"#FFFFFF",	// 0 white
	"#000000",	// 1 black
	"#0000FF",	// 2 blue     (navy)
	"#00FF00",	// 3 green
	"#FF0000",	// 4 red
	"#4E2F2F",	// 5 brown    (maroon)
	"#AA00FF",	// 6 purple
	"#FF7700",	// 7 orange   (olive)
	"#FFFF00",	// 8 yellow
	"#32CD32",	// 9 lt.green (lime)
	"#349F79",	// 10 teal    (a kinda green/blue cyan)
	"#70DB93",	// 11 lt.cyan (cyan ?) (aqua)
	"#3333FF",	// 12 lt.blue (royal)
	"#FF00AA",	// 13 pink    (light purple) (fuchsia)
	"#A8A8A8",	// 14 grey
	"#E6E8FA"	// 15 lt.grey (silver)
};

static const char *ANSIar[] = {
	"#000000",	// 30 black
	"#FF0000",	// 31 red
	"#00FF00",	// 32 green
	"#FFFF00",	// 33 yellow
	"#0000FF",	// 34 blue
	"#FF00FF",	// 35 magenta (purple)
	"#00FFFF",	// 36 cyan (aqua)
	"#FFFFFF",	// 37 white
};

static const char *irc_mIRC_to_html(const char *const string, size_t *pos) {
	int	i, col = 0;

	for (i = 0; (i < 2) && isdigit(string[*pos]); i++, (*pos)++) {
		col *= 10;
		col += string[*pos] - '0';
	}
	if (string[*pos] == ',') {
		(*pos)++;
		for (i = 0; (i < 2) && isdigit(string[*pos]); i++, (*pos)++)
			;
	}
	if ((col >= 0) && (col <= 15))
		return(mIRCar[col]);
	else
		return("#000000");
}

static char *irc_irc_to_html(const char *const string) {
	static char *output = NULL;
	int	o = 0;
	size_t	l, i = 0, s;
	int	infont = 0, inbold = 0, initalics = 0, inunderline = 0;

	assert(string != NULL);

	l = strlen(string);

	s = l*(sizeof("<font color=\"#RRGGBB\">")-1) + 1;
	output = realloc(output, s);
	if (output == NULL)
		abort();

	while (i < l) {
		switch(string[i]) {
		  case 2:
			if (inbold == 1) {
				memcpy(&output[o],"</B>",4);
				o += 4;
				inbold = 0;
			} else {
				memcpy(&output[o],"<B>",3);
				o += 3;
				inbold = 1;
			}
			break;
		  case 3:
			if (isdigit(string[i+1])) {
				i++;
				sprintf(output+o, "<font color=\"%s\">", irc_mIRC_to_html(string, &i));
				o += sizeof("<font color=\"#RRGGBB\">")-1;
				infont = 1;
				i--;
			} else if (infont == 1) {
				strcpy(output+o, "</font>");
				o += sizeof("</font>")-1;
				infont = 0;
			}
			break;
		  case 15:
			if (infont == 1) {
				strcpy(output+o, "</font>");
				o += sizeof("</font>")-1;
				infont = 0;
			}
			if (inbold == 1) {
				strcpy(output+o, "</B>");
				o += sizeof("</B>")-1;
				inbold = 0;
			}
			if (initalics == 1) {
				strcpy(output+o, "</I>");
				o += sizeof("</I>")-1;
				initalics = 0;
			}
			if (inunderline == 1) {
				strcpy(output+o, "</U>");
				o += sizeof("</U>")-1;
				inunderline = 0;
			}
			break;
		  case 27:
			if (string[i+1] == '[') {
				i += 2;
				while ((string[i] != 0) && (string[i] != 'm')) {
					int	num = 0;

					if (!isdigit(string[i]))
						break;
					while (isdigit(string[i])) {
						num *= 10;
						num += string[i] - '0';
						i++;
					}
					if (string[i] == ';')
						i++;
					switch (num) {
					  case 0:
						if (infont == 1) {
							strcpy(output+o, "</font>");
							o += sizeof("</font>")-1;
							infont = 0;
						}
						if (inbold == 1) {
							strcpy(output+o, "</B>");
							o += sizeof("</B>")-1;
							inbold = 0;
						}
						if (initalics == 1) {
							strcpy(output+o, "</I>");
							o += sizeof("</I>")-1;
							initalics = 0;
						}
						if (inunderline == 1) {
							strcpy(output+o, "</U>");
							o += sizeof("</U>")-1;
							inunderline = 0;
						}
						break;
					  case 1:
						if (inbold == 0) {
							strcpy(output+o, "<B>");
							o += sizeof("<B>")-1;
							inbold = 1;
						}
						break;
					  case 2:
						if (inbold == 1) {
							strcpy(output+o, "</B>");
							o += sizeof("</B>")-1;
							inbold = 0;
						}
						break;
					  case 3:
						if (inunderline == 0) {
							strcpy(output+o, "<U>");
							o += sizeof("<U>")-1;
							inunderline = 1;
						}
						break;
					  case 7:
						if (initalics == 0) {
							strcpy(output+o, "<I>");
							o += sizeof("<I>")-1;
							initalics = 1;
						}
						break;
					  case 30: case 31: case 32: case 33:
					  case 34: case 35: case 36: case 37:
						sprintf(output+o, "<font color=\"%s\">", ANSIar[num-30]);
						o += sizeof("<font color=\"#RRGGBB\">")-1;
						infont = 1;
						break;
					}
				}
				if (string[i] == 0)
					i--;
			}
			break;
		  case 22:
			if (initalics == 1) {
				memcpy(&output[o],"</I>",4);
				o += 4;
				initalics = 0;
			} else {
				memcpy(&output[o],"<I>",3);
				o += 3;
				initalics = 1;
			}
			break;
		  case 31:
			if (inunderline == 1) {
				memcpy(&output[o],"</U>",4);
				o += 4;
				inunderline = 0;
			} else {
				memcpy(&output[o],"<U>",3);
				o += 3;
				inunderline = 1;
			}
			break;
		  case '&':
			memcpy(&output[o],"&amp;",5);
			o += 5;
			break;
		  case '<':
			memcpy(&output[o],"&lt;",4);
			o += 4;
			break;
		  case '>':
			memcpy(&output[o],"&gt;",4);
			o += 4;
			break;
		  case 16:
			switch(string[++i]) {
			  case 16:
				output[o++] = '\020';
				break;
			  case 'r':
				if (string[i+1] == '\020' && string[i+2] == 'n') {
					i += 2;
					memcpy(&output[o],"<br>",4);
					o += 4;
				} else
					output[o++] = '\r';
				break;
			  case 'n':
				output[o++] = '\n';
				break;
			  default:
				output[o++] = string[i];
				break;
			}
			break;
		  case ' ':
			if (string[i+1] == ' ') {
				memcpy(&output[o], "&nbsp;", 6);
				o += 6;
				break;
			}
		  default:
			output[o++] = string[i];
			break;
		}
		i++;
	}

	output[o] = '\0';

	return(output);
}

static int irc_internal_disconnect(irc_conn_t *c, const fte_t error) {
#ifdef DEBUG_ECHO
	irc_echof(c, __FUNCTION__, "c=%#p, error=%i\n", c, error);
#endif

	irc_conn_t_dtor(c);

	firetalk_callback_disconnect(c, error);

	return(FE_SUCCESS);
}

static int irc_send_printf(irc_conn_t *c, const char *const format, ...) {
	va_list	ap;
	size_t	i,
		datai = 0;
	char	data[513];

	va_start(ap, format);
	for (i = 0; format[i] != 0; i++) {
		if (format[i] == '%') {
			switch (format[++i]) {








				case 's': {
						const char
							*s = irc_html_to_irc(va_arg(ap, char *));
						size_t	slen = strlen(s);

						if ((datai+slen) > (sizeof(data)-2-1))
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
			if (datai > (sizeof(data)-2-1))
				return(FE_PACKETSIZE);
		}
	}
	va_end(ap);
	data[datai] = 0;

#ifdef DEBUG_ECHO
	irc_echof(c, __FUNCTION__, "%s", data);
#endif

	strcpy(data+datai, "\r\n");
	datai += 2;

	{
		firetalk_connection_t *fchandle = firetalk_find_conn(c);

		firetalk_internal_send_data(fchandle, data, datai);
	}

	return(FE_SUCCESS);
}

static char **irc_recv_parse(irc_conn_t *c, unsigned char *buffer, uint16_t *bufferpos) {
	static char *args[256];
	static char data[513];
	size_t curarg;
	char *tempchr;
	char *tempchr2;

	args[0] = NULL;

	assert(*bufferpos < sizeof(data));
	memcpy(data, buffer, *bufferpos);
	data[*bufferpos] = '\0';

	tempchr = strchr(data, '\n');
	if (tempchr == NULL)
		return(NULL);
	if ((tempchr > data) && (tempchr[-1] == '\r'))
		tempchr[-1] = 0;
	else
		tempchr[0] = 0;
	*bufferpos -= (tempchr - data + 1);
	memmove(buffer, &buffer[tempchr - data + 1], *bufferpos);

#ifdef DEBUG_ECHO
	irc_echof(c, __FUNCTION__, "%s", data);
#endif

	curarg = 0;
	tempchr = data;
	if (*tempchr == ':')
		tempchr++;
	else
		args[curarg++] = ":SERVER";

	while ((curarg < sizeof(args)/sizeof(*args)) && ((tempchr2 = strchr(tempchr, ' ')) != NULL)) {
		args[curarg++] = tempchr;
		*tempchr2 = 0;
		tempchr = tempchr2 + 1;
		if (*tempchr == ':') {
			tempchr++;
			break;
		}
	}
	args[curarg++] = tempchr;
	args[curarg] = NULL;
	return(args);
}

static char *irc_get_nickname(const char *const hostmask) {
	static char data[512];
	char	*tempchr;

	strncpy(data, hostmask, sizeof(data)-1);
	data[sizeof(data)-1] = 0;

	if ((tempchr = strchr(data, '!')) != NULL)
		*tempchr = 0;
	return(data);
}

static fte_t irc_set_nickname(irc_conn_t *c, const char *const nickname) {
	return(irc_send_printf(c,"NICK %s",nickname));
}

static fte_t irc_set_password(irc_conn_t *c, const char *const oldpass, const char *const newpass) {
	return(FE_SUCCESS);
}

static void irc_destroy_conn(irc_conn_t *c) {
	if (firetalk_internal_get_connectstate(c) != FCS_NOTCONNECTED)
		irc_send_printf(c, "QUIT :Handle destroyed");
	irc_internal_disconnect(c, FE_USERDISCONNECT);
	irc_conn_t_delete(c);
}

static fte_t irc_disconnect(irc_conn_t *c) {
#ifdef DEBUG_ECHO
	irc_echof(c, __FUNCTION__, "c=%#p; state=%i\n", c, firetalk_internal_get_connectstate(c));
#endif
	if (firetalk_internal_get_connectstate(c) != FCS_NOTCONNECTED)
		irc_send_printf(c, "QUIT :User disconnected");
	return(irc_internal_disconnect(c, FE_USERDISCONNECT));
}

static fte_t irc_disconnected(irc_conn_t *c, const fte_t reason) {
#ifdef DEBUG_ECHO
	irc_echof(c, __FUNCTION__, "c=%#p, reason=%i; state=%i\n", c, reason, firetalk_internal_get_connectstate(c));
#endif
	assert(firetalk_internal_get_connectstate(c) == FCS_NOTCONNECTED);
	return(irc_internal_disconnect(c, reason));
}

static irc_conn_t *irc_create_conn(void) {
	irc_conn_t *c;

	if ((c = irc_conn_t_new()) == NULL)
		abort();

	return(c);
}

#if defined(HAVE_GETPWUID) && defined(HAVE_GETUID)
# include <pwd.h>
#endif

static fte_t irc_signon(irc_conn_t *c, const char *const nickname) {
#if defined(HAVE_GETPWUID) && defined(HAVE_GETUID)
	struct passwd	*pw = getpwuid(getuid());
	char	buf[1024];
	int	i;

	for (i = 0; (pw->pw_gecos[i] != 0) && (pw->pw_gecos[i] != ',') && (i < sizeof(buf)-1); i++)
		buf[i] = pw->pw_gecos[i];
	if (i == 0) {
		snprintf(buf, sizeof(buf), "http://naim.n.ml.org/ user");
		i = strlen(buf);
	}
	buf[i] = 0;

	if (irc_send_printf(c, "USER %s %s %s :%s", pw->pw_name, nickname, nickname, buf) != FE_SUCCESS)
		return(FE_PACKET);
#else
	if (irc_send_printf(c, "USER %s %s %s :%s", nickname, nickname, nickname, nickname) != FE_SUCCESS)
		return(FE_PACKET);
#endif

	if (irc_send_printf(c, "NICK %s", nickname) != FE_SUCCESS)
		return(FE_PACKET);

	STRREPLACE(c->nickname, nickname);

	return(FE_SUCCESS);
}

static fte_t irc_preselect(irc_conn_t *c, fd_set *read, fd_set *write, fd_set *except, int *n) {
	return(FE_SUCCESS);
}

static fte_t irc_postselect(irc_conn_t *c, fd_set *read, fd_set *write, fd_set *except) {
	return(FE_SUCCESS);
}

static void irc_addwhois(irc_conn_t *c, const char *const name, const char *const format, ...) {
	irc_whois_t *whoisiter;
	char	buf[1024];
	va_list	msg;

	va_start(msg, format);
	vsnprintf(buf, sizeof(buf), format, msg);
	va_end(msg);

	for (whoisiter = c->whois_head; whoisiter != NULL; whoisiter = whoisiter->next)
		if (irc_compare_nicks(name, whoisiter->nickname) == 0) {
			int	len = whoisiter->info?strlen(whoisiter->info):0;

			whoisiter->info = realloc(whoisiter->info, len+strlen(buf)+1);
			if (whoisiter->info == NULL)
				abort();
			strcpy(whoisiter->info+len, buf);
			break;
		}
}

static fte_t irc_got_data_parse(irc_conn_t *c, char **args) {
	irc_whois_t *whoisiter, *whoisiter2;
	char	*tempchr;

	{
		static unsigned int inwhois = 0;
		int	handled = 0,
			numeric;

		handled = 1;
		if (strcmp(args[1], "PING") == 0) {
			if (args[2] != NULL) {
				if (irc_send_printf(c, "PONG :%s", args[2]) != 0) {
					irc_internal_disconnect(c, FE_PACKET);
					return(FE_PACKET);
				}
			} else {
				if (irc_send_printf(c, "PONG") != 0) {
					irc_internal_disconnect(c, FE_PACKET);
					return(FE_PACKET);
				}
			}
		} else if (strcmp(args[1], "QUIT") == 0) {
			const char *name = irc_get_nickname(args[0]);

			firetalk_callback_im_buddyonline(c, name, 0);
			if (irc_compare_nicks(c->nickname, name) == 0)
				irc_internal_disconnect(c, FE_DISCONNECT);
			else
				firetalk_callback_chat_user_quit(c, name, irc_irc_to_html(args[2]));
		} else
			handled = 0;

		if (handled)
			return(FE_SUCCESS);

		numeric = atoi(args[1]);

		if (args[2] == NULL)
			goto unhandled;

		handled = 1;
		if (strcmp(args[1], "JOIN") == 0) {
			const char	*name = irc_get_nickname(args[0]);

			firetalk_callback_im_buddyonline(c, name, 1);
			if (irc_compare_nicks(c->nickname, name) == 0)
				firetalk_callback_chat_joined(c, args[2]);
			else {
				char	*extra = strchr(args[0], '!');

				firetalk_callback_chat_user_joined(c, args[2], name, (extra != NULL)?(extra+1):NULL);
			}
		} else if (strcmp(args[1], "PART") == 0) {
			const char	*name = irc_get_nickname(args[0]);

			if (irc_compare_nicks(c->nickname, name) == 0) {
				irc_disc_rem(c, args[2]);
				firetalk_callback_chat_left(c, args[2]);
			} else {
				irc_disc_user_rem(c, args[2], name);
				firetalk_callback_chat_user_left(c, args[2], name, (args[3] != NULL)?irc_irc_to_html(args[3]):NULL);
			}
		} else if (strcmp(args[1],"NICK") == 0) {
			const char *name = irc_get_nickname(args[0]);

			if (irc_compare_nicks(c->nickname, name) == 0) {
				STRREPLACE(c->nickname, args[2]);
				firetalk_callback_newnick(c, c->nickname);
			}
			firetalk_callback_user_nickchanged(c, name, args[2]);
		} else
			handled = 0;

		if (handled)
			return(FE_SUCCESS);

		if (args[3] == NULL)
			goto unhandled;

		handled = 1;
		if (strcmp(args[1],"PRIVMSG") == 0) {
			/* scan for CTCPs */
			while ((tempchr = strchr(args[3], 1))) {
				char	*sp;

				if ((sp = strchr(tempchr+1, 1))) {
					*sp = 0;

					/* we have a ctcp */
					if (strchr(ROOMSTARTS, args[2][0])) {
						/* chat room subcode */
						if (strncasecmp(&tempchr[1],"ACTION ",7) == 0)
							firetalk_callback_chat_getaction(c, args[2], irc_get_nickname(args[0]), 0, irc_irc_to_html(tempchr+8));
					} else {
						char *endcommand;

						endcommand = strchr(&tempchr[1], ' ');
						if (endcommand) {
							*endcommand = '\0';
							endcommand++;
							firetalk_callback_subcode_request(c, irc_get_nickname(args[0]), &tempchr[1], endcommand);
						} else
							firetalk_callback_subcode_request(c, irc_get_nickname(args[0]), &tempchr[1], NULL);
					}
					memmove(tempchr, sp+1, strlen(sp+1) + 1);
				} else
					break;
			}
			if (args[3][0] != '\0') {
				if (strchr(ROOMSTARTS, args[2][0]))
					firetalk_callback_chat_getmessage(c, args[2], irc_get_nickname(args[0]), 0, irc_irc_to_html(args[3]));
				else
					firetalk_callback_im_getmessage(c, irc_get_nickname(args[0]), 0, irc_irc_to_html(args[3]));
			}
		} else if (strcmp(args[1],"NOTICE") == 0) {
			const char	*name = irc_get_nickname(args[0]);

			/* scan for CTCP's */
			while ((tempchr = strchr(args[3], 1))) {
				char	*sp;

				if ((sp = strchr(tempchr+1, 1)))
					*sp = 0;
				/* we have a ctcp */
				if (strchr(ROOMSTARTS, args[2][0]) == NULL) {
					char *endcommand;
					endcommand = strchr(&tempchr[1], ' ');
					if (endcommand) {
						*endcommand = '\0';
						endcommand++;
						firetalk_callback_subcode_reply(c, name, &tempchr[1], endcommand);
					} else
						firetalk_callback_subcode_reply(c, name, &tempchr[1], NULL);
				}
				if (sp)
					memcpy(tempchr, sp+1, strlen(sp+1) + 1);
				else
					break;
			}
			if (strchr(name, '.') != NULL) {
				if (strncmp(args[3], "*** Notice -- ", sizeof("*** Notice -- ")-1) == 0)
					firetalk_callback_chat_getmessage(c, ":RAW", name, 1, irc_irc_to_html(args[3]+sizeof("*** Notice -- ")-1));
				else
					firetalk_callback_chat_getmessage(c, ":RAW", name, 1, irc_irc_to_html(args[3]));
			} else if (args[3][0] != '\0') {
				if (strchr(ROOMSTARTS, args[2][0]))
					firetalk_callback_chat_getmessage(c, args[2], name, 1, irc_irc_to_html(args[3]));
				else
					firetalk_callback_im_getmessage(c, name, 1, irc_irc_to_html(args[3]));
			}
		} else if (strcmp(args[1], "TOPIC") == 0) {
			firetalk_callback_chat_gottopic(c, args[2], irc_irc_to_html(args[3]), irc_get_nickname(args[0]));
		} else if (strcmp(args[1], "KICK") == 0) {
			const char *name = irc_get_nickname(args[3]);

			if (irc_compare_nicks(c->nickname, name) == 0) {
				irc_disc_rem(c, args[2]);
				firetalk_callback_chat_kicked(c, args[2], irc_get_nickname(args[0]), irc_irc_to_html(args[4]));
			} else {
				irc_disc_user_rem(c, args[2], name);

				tempchr = strdup(name);
				if (tempchr == NULL)
					abort();
				firetalk_callback_chat_user_kicked(c, args[2], tempchr, irc_get_nickname(args[0]), irc_irc_to_html(args[4]));
				free(tempchr);
				tempchr = NULL;
			}
		} else
			handled = 0;

		if (handled)
			return(FE_SUCCESS);

		if (numeric == 311)     // RPL_WHOISUSER
			inwhois = 1;
		else if (numeric == 318)// RPL_ENDOFWHOIS
			inwhois = 0;

		if (!inwhois)
			switch (numeric) {
			  case 333: /* :PREFIX 333 sn channel topicsetuser topicsettime */
			  case 306: /* :PREFIX 306 sn :You have been marked as being away */
			  case 305: /* :PREFIX 305 sn :You are no longer marked as being away */
			  case 396: /* :PREFIX 396 sn whoishost :is now your hidden host */
				return(FE_SUCCESS);
			}

		handled = 1;
		switch (numeric) {
			case 366: /* :PREFIX 366 sn channel :End of /NAMES list. */
				firetalk_callback_chat_user_joined(c, args[3], NULL, NULL);
				break;
			case 301: /* RPL_AWAY */
				break;
			case 313: /* RPL_WHOISOPER */
				for (whoisiter = c->whois_head; whoisiter != NULL; whoisiter = whoisiter->next)
					if (irc_compare_nicks(args[3],whoisiter->nickname) == 0) {
						whoisiter->flags |= FF_ADMIN;
						break;
					}
				break;
			case 318: /* RPL_ENDOFWHOIS */
				whoisiter2 = NULL;
				for (whoisiter = c->whois_head; whoisiter != NULL; whoisiter = whoisiter->next) {
					if (irc_compare_nicks(args[3], whoisiter->nickname) == 0) {
						/* manual whois */
						firetalk_callback_gotinfo(c, whoisiter->nickname, whoisiter->info, 0, whoisiter->online, whoisiter->idle, whoisiter->flags);
						if (whoisiter2)
							whoisiter2->next = whoisiter->next;
						else
							c->whois_head = whoisiter->next;
						irc_whois_t_delete(whoisiter);
						whoisiter = NULL;
						break;
					}
					whoisiter2 = whoisiter;
				}
				break;
			case 401: /* ERR_NOSUCHNICK */
				firetalk_callback_im_buddyonline(c, args[3], 0);
			case 441: /* ERR_USERNOTINCHANNEL */
			case 443: /* ERR_USERONCHANNEL */
				whoisiter2 = NULL;
				for (whoisiter = c->whois_head; whoisiter != NULL; whoisiter = whoisiter->next) {
					if (irc_compare_nicks(args[3], whoisiter->nickname) == 0) {
						if (whoisiter2)
							whoisiter2->next = whoisiter->next;
						else
							c->whois_head = whoisiter->next;
						irc_whois_t_delete(whoisiter);
						whoisiter = NULL;
						firetalk_callback_error(c, FE_BADUSER, args[3], args[4]);
						break;
					}
					whoisiter2 = whoisiter;
				}
				break;
			case 403: /* ERR_NOSUCHCHANNEL */
			case 442: /* ERR_NOTONCHANNEL */
				firetalk_callback_error(c, FE_BADROOM, args[3], args[4]);
				break;
			case 404: /* ERR_CANNOTSENDTOCHAN */
			case 405: /* ERR_TOOMANYCHANNELS */
			case 471: /* ERR_CHANNELISFULL */
			case 473: /* ERR_INVITEONLYCHAN */
			case 474: /* ERR_BANNEDFROMCHAN */
			case 475: /* ERR_BADCHANNELKEY */
				firetalk_callback_error(c, FE_ROOMUNAVAILABLE, args[3], args[4]);
				break;
			case 412: /* ERR_NOTEXTTOSEND */
				firetalk_callback_error(c, FE_BADMESSAGE, NULL, args[4]);
				break;
			case 421: /* ERR_UNKNOWNCOMMAND */
				if (strcmp(args[3], "SILENCE") == 0)
					c->nosilence = 1;
				goto unhandled;
				break;
			case 433: /* ERR_NICKNAMEINUSE */
				firetalk_callback_error(c, FE_BADUSER, NULL, "Nickname in use.");
				break;
			case 482: /* ERR_CHANOPRIVSNEEDED */
				firetalk_callback_error(c, FE_NOPERMS, args[3], "You need to be a channel operator.");
				break;
			default:
				handled = 0;
		}

		if (handled)
			return(FE_SUCCESS);

		if (args[4] == NULL)
			goto unhandled;

		if (strcmp(args[1],"MODE") == 0) {
			const char *source = irc_get_nickname(args[0]);
			int	loc, arg = 4, dir = 1;

			for (loc = 0; args[3][loc] != 0; loc++) {
				char	*tmp;
				int	arged;

				if (args[3][loc] == '+') {
					dir = 1;
					continue;
				} else if (args[3][loc] == '-') {
					dir = -1;
					continue;
				}

				if (((tmp = strchr(c->chanmodes, args[3][loc])) == NULL) && (strchr(c->chanprefix, args[3][loc]) == NULL))
					break;

				if ((tmp == NULL) || (tmp[1] == ','))
					arged = 1;
				else
					arged = 0;

				switch (args[3][loc]) {
				  case 'o':
					if (dir == 1) {
						firetalk_callback_chat_user_opped(c, args[2], args[arg], source);
						if (irc_compare_nicks(args[arg], c->nickname) == FE_SUCCESS)
							firetalk_callback_chat_opped(c, args[2], source);
					} else if (dir == -1) {
						firetalk_callback_chat_user_deopped(c, args[2], args[arg], source);
						if (irc_compare_nicks(args[arg], c->nickname) == FE_SUCCESS)
							firetalk_callback_chat_deopped(c, args[2], source);
					}
					break;
				  case 'k':
					if (dir == 1)
						firetalk_callback_chat_keychanged(c, args[2], args[arg], source);
					else
						firetalk_callback_chat_keychanged(c, args[2], NULL, source);
					break;
				  default:
					if (dir == 1)
						firetalk_callback_chat_modeset(c, args[2], source, args[3][loc], arged?args[arg]:NULL);
					else
						firetalk_callback_chat_modeunset(c, args[2], source, args[3][loc], arged?args[arg]:NULL);
					break;
				}

				if (arged)
					arg++;
			}
			return(FE_SUCCESS);
		}

		handled = 1;
		switch (numeric) {
			case 317: /* RPL_WHOISIDLE */
				for (whoisiter = c->whois_head; whoisiter != NULL; whoisiter = whoisiter->next)
					if (irc_compare_nicks(args[3], whoisiter->nickname) == 0) {
						whoisiter->online = atol(args[5]);
						whoisiter->idle = atol(args[4])/60;
					}
				break;
			case 312: /* RPL_WHOISSERVER */
				irc_addwhois(c, args[3], "<br>On server %s (%s)", args[4], irc_irc_to_html(args[5]));
				break;
			case 319: /* RPL_WHOISCHANNELS */
				irc_addwhois(c, args[3], "<br>On channels %s", irc_irc_to_html(args[4]));
				break;
			case 332: /* RPL_TOPIC */
				firetalk_callback_chat_gottopic(c, args[3], irc_irc_to_html(args[4]), NULL);
				break;
			default:
				handled = 0;
		}

		if (handled)
			return(FE_SUCCESS);

		if (args[5] == NULL)
			goto unhandled;

		if (numeric == 353) { /* RPL_NAMREPLY */
			char	*str = args[5];

			while ((str != NULL) && (*str != 0)) {
				int	oped = 0,
					voiced = 0;
				char	*sp;

				if ((sp = strchr(str, ' ')) != NULL)
					*sp = 0;

				while ((*str != 0) && !irc_isnickfirst(*str)) {
					if (*str == '@')
						oped = 1;
					else if (*str == '+')
						voiced = 1;
					str++;
				}

				firetalk_callback_chat_user_joined(c, args[4], str, NULL);
				firetalk_callback_im_buddyonline(c, str, 1);
				if (oped) {
					firetalk_callback_chat_user_opped(c, args[4], str, NULL);
					if (irc_compare_nicks(str, c->nickname) == FE_SUCCESS)
						firetalk_callback_chat_opped(c, args[4], NULL);
				}

				if (sp != NULL)
					str = sp+1;
				else
					str = NULL;
			}
			return(FE_SUCCESS);
		}

		if ((args[6] == NULL) || (args[7] == NULL))
			goto unhandled;

		if (numeric == 311) { /* RPL_WHOISUSER */
			char	*gecos = irc_irc_to_html(args[7]);
			int	i;

			for (i = 0; gecos[i] != 0; i++)
				if (strncasecmp(gecos+i, "<HTML>", sizeof("<HTML>")-1) == 0)
					break;
			if (gecos[i] != 0)
				irc_addwhois(c, args[3], "%s@%s (%s)", args[4], args[5], firetalk_htmlentities(gecos));
			else
				irc_addwhois(c, args[3], "%s@%s (<HTML>%s</HTML>)", args[4], args[5], gecos);
			return(FE_SUCCESS);
		} else if (numeric == 352) { /* WHO output */
			char	buf[1024];
			int	i;

			snprintf(buf, sizeof(buf), "%s %s %s %s", args[7], args[4], args[5], args[6]);
			for (i = 8; args[i] != NULL; i++)
				snprintf(buf, sizeof(buf), "%s, %s", buf, args[i]);
			firetalk_callback_chat_getmessage(c, ":RAW", args[3], 0, buf);
			return(FE_SUCCESS);
		}

	  unhandled: 
		if (inwhois) {
			char	buf[1024];
			int	i;

			snprintf(buf, sizeof(buf), "<br>%s", args[3]);
			for (i = numeric?4:3; args[i+1] != NULL; i++)
				;
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), " %s", args[i]);
			for (i = numeric?4:3; args[i+1] != NULL; i++)
				snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), " %s", args[i]);
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), " (%s)", args[1]);
			irc_addwhois(c, args[3], "%s", buf);
		} else {
			char	buf[1024];
			int	i;

			*buf = 0;
			for (i = 1; args[i+1] != NULL; i++)
				snprintf(buf, sizeof(buf), "%s<B>%s</B>, ", buf, args[i]);
			snprintf(buf, sizeof(buf), "%s%s", buf, args[i]);
			firetalk_callback_chat_getmessage(c, ":RAW", irc_get_nickname(args[0]), 0, buf);
		}
	}

	return(FE_SUCCESS);
}

static fte_t irc_got_data(irc_conn_t *c, firetalk_buffer_t *buffer) {
	char	**args;

	assert(firetalk_buffer_t_valid(buffer));
	while (((args = irc_recv_parse(c, buffer->buffer, &(buffer->pos))) != NULL) && (args[1] != NULL)) {
		fte_t	fte;

		assert(firetalk_buffer_t_valid(buffer));
		if ((fte = irc_got_data_parse(c, args)) != FE_SUCCESS)
			return(fte);
	}

	return(FE_SUCCESS);
}

static fte_t irc_got_data_connecting(irc_conn_t *c, firetalk_buffer_t *buffer) {
	char	**args;

	assert(firetalk_buffer_t_valid(buffer));
	while (((args = irc_recv_parse(c, buffer->buffer, &(buffer->pos))) != NULL) && (args[1] != NULL)) {
		assert(firetalk_buffer_t_valid(buffer));
		if (strcmp(args[1], "ERROR") == 0) {
			irc_send_printf(c, "QUIT :error");
			if (args[2] == NULL)
				firetalk_callback_connectfailed(c, FE_PACKET, "Server returned ERROR");
			else
				firetalk_callback_connectfailed(c, FE_PACKET, args[2]);
			return(FE_PACKET);
		} else {
			switch (atoi(args[1])) {
			  case   1: /* :PREFIX 001 sn :Welcome message */
				if (strcmp(c->nickname, args[2]) != 0) {
					firetalk_callback_user_nickchanged(c, c->nickname, args[2]);
					STRREPLACE(c->nickname, args[2]);
					firetalk_callback_newnick(c, args[2]);
				}
				break;
			  case   5: /* :PREFIX 005 sn{ key=value} */ {
					int	i;

					for (i = 3; args[i] != NULL; i++) {
						char	*key = args[i], *value;

						if ((value = strchr(args[i], '=')) != NULL)
							*value++ = 0;

						if ((value != NULL) && (strcmp(key, "MODES") == 0))
							c->maxmodes = atoi(value);
						else if ((value != NULL) && (strcmp(key, "CHANMODES") == 0))
							STRREPLACE(c->chanmodes, value);
						else if ((value != NULL) && (strcmp(key, "PREFIX") == 0))
							STRREPLACE(c->chanprefix, value);
					}
				}
				break;
			  case 376: /* End of MOTD */
			  case 422: /* MOTD is missing */
				firetalk_callback_doinit(c, c->nickname);
				firetalk_callback_connected(c);
				break;
			  case 431:
			  case 432:
			  case 436:
			  case 461:
				irc_send_printf(c, "QUIT :Invalid nickname");
				firetalk_callback_connectfailed(c, FE_BADUSER, "Invalid nickname");
				return(FE_BADUSER);
			  case 433:
				irc_send_printf(c, "QUIT :Invalid nickname");
				firetalk_callback_connectfailed(c, FE_BADUSER, "Nickname in use");
				return(FE_BADUSER);
			  case 465:
				irc_send_printf(c, "QUIT :banned");
				firetalk_callback_connectfailed(c, FE_BLOCKED, "You are banned");
				return(FE_BLOCKED);
			  default: {
					fte_t	fte;

					if ((fte = irc_got_data_parse(c, args)) != FE_SUCCESS)
						return(fte);
					break;
				}
			}
		}
	}

	return(FE_SUCCESS);
}

static fte_t irc_chat_join(irc_conn_t *c, const char *const room) {
	return(irc_send_printf(c,"JOIN %s",room));
}

static fte_t irc_chat_part(irc_conn_t *c, const char *const room) {
	return(irc_send_printf(c,"PART %s",room));
}

static fte_t irc_chat_send_message(irc_conn_t *c, const char *const room, const char *const message, const int auto_flag) {
	if (strcasecmp(room, ":RAW") == 0)
		return(irc_send_printf(c, "%s", message));

#ifdef DEBUG_ECHO
	irc_echof(c, __FUNCTION__, "c=%#p, room=%#p \"%s\", message=%#p \"%s\", auto_flag=%i\n", c, room, room, message, message, auto_flag);
#endif

	if (auto_flag)
		return(irc_send_printf(c, "NOTICE %s :%s", room, message));
	else
		return(irc_send_printf(c, "PRIVMSG %s :%s", room, message));
}

static fte_t irc_chat_send_action(irc_conn_t *c, const char *const room, const char *const message, const int auto_flag) {
	return(irc_send_printf(c,"PRIVMSG %s :\001ACTION %s\001",room,message));
}

static fte_t irc_chat_invite(irc_conn_t *c, const char *const room, const char *const who, const char *const message) {
	return(irc_send_printf(c,"INVITE %s %s",who,room));
}

static fte_t irc_im_send_message(irc_conn_t *c, const char *const dest, const char *const message, const int auto_flag) {
	firetalk_connection_t *fchandle;
	char	buf[512+1];

	if (strcasecmp(dest, ":RAW") == 0)
		return(irc_send_printf(c, "%s", message));

	fchandle = firetalk_find_conn(c);
	if (auto_flag) {
		snprintf(buf, sizeof(buf), "NOTICE %s :%s", dest, message);
		firetalk_queue_append(buf, sizeof(buf), &(fchandle->subcode_replies), dest);
	} else {
		snprintf(buf, sizeof(buf), "PRIVMSG %s :%s", dest, message);
		firetalk_queue_append(buf, sizeof(buf), &(fchandle->subcode_requests), dest);
	}
	return(irc_send_printf(c, "%s", buf));
}

static fte_t irc_im_send_action(irc_conn_t *c, const char *const dest, const char *const message, const int auto_flag) {
	return(irc_send_printf(c, "PRIVMSG %s :\001ACTION %s\001", dest, message));
}

static fte_t irc_chat_set_topic(irc_conn_t *c, const char *const room, const char *const topic) {
	return(irc_send_printf(c,"TOPIC %s :%s",room,topic));
}

static fte_t irc_chat_op(irc_conn_t *c, const char *const room, const char *const who) {
	return(irc_send_printf(c,"MODE %s +o %s",room,who));
}

static fte_t irc_chat_deop(irc_conn_t *c, const char *const room, const char *const who) {
	return(irc_send_printf(c,"MODE %s -o %s",room,who));
}

static fte_t irc_chat_kick(irc_conn_t *c, const char *const room, const char *const who, const char *const reason) {
	if (reason)
		return(irc_send_printf(c,"KICK %s %s :%s",room,who,reason));
	else
		return(irc_send_printf(c,"KICK %s %s",room,who));
}

static fte_t irc_im_add_buddy(irc_conn_t *c, const char *const name, const char *const group, const char *const friendly) {
	firetalk_connection_t *fchandle = firetalk_find_conn(c);

	if (firetalk_user_visible(fchandle, name) == FE_SUCCESS)
		firetalk_callback_im_buddyonline(c, name, 1);
	return(FE_SUCCESS);
}

static fte_t irc_im_remove_buddy(irc_conn_t *c, const char *const name, const char *const group) {
	firetalk_callback_im_buddyonline(c, name, 0);

	return(FE_SUCCESS);
}

static fte_t irc_im_add_deny(irc_conn_t *c, const char *const nickname) {
	if (!c->nosilence)
		return(irc_send_printf(c, "SILENCE +%s!*@*", nickname));
	else
		return(FE_SUCCESS);
}

static fte_t irc_im_remove_deny(irc_conn_t *c, const char *const nickname) {
	if (!c->nosilence)
		return(irc_send_printf(c, "SILENCE -%s!*@*", nickname));
	else
		return(FE_SUCCESS);
}

static fte_t irc_im_evil(irc_conn_t *c, const char *const who) {
	return(FE_SUCCESS);
}

static fte_t irc_set_privacy(irc_conn_t *c, const char *const mode) {
	return(FE_SUCCESS);
}

static fte_t irc_get_info(irc_conn_t *c, const char *const nickname) {
	irc_whois_t *iter;

	if ((iter = irc_whois_t_new()) == NULL)
		abort();
	iter->next = c->whois_head;
	c->whois_head = iter;
	STRREPLACE(iter->nickname, nickname);
	return(irc_send_printf(c, "WHOIS %s", nickname));
}

static fte_t irc_set_info(irc_conn_t *c, const char *const info) {
	return(FE_SUCCESS);
}

static fte_t irc_set_away(irc_conn_t *c, const char *const message, const int auto_flag) {
	if (message)
		return(irc_send_printf(c,"AWAY :%s",message));
	else
		return(irc_send_printf(c,"AWAY"));
}

static fte_t irc_periodic(firetalk_connection_t *const conn) {
	return(FE_SUCCESS);
}

#if 0
static fte_t irc_subcode_send_request(irc_conn_t *c, const char *const to, const char *const command, const char *const args) {
	if (args == NULL) {
		if (irc_send_printf(c,"PRIVMSG %s :\001%s\001",to,command) != 0) {
			irc_internal_disconnect(c,FE_PACKET);
			return(FE_PACKET);
		}
	} else {
		if (irc_send_printf(c,"PRIVMSG %s :\001%s %s\001",to,command,args) != 0) {
			irc_internal_disconnect(c,FE_PACKET);
			return(FE_PACKET);
		}
	}
	return(FE_SUCCESS);
}

static fte_t irc_subcode_send_reply(irc_conn_t *c, const char *const to, const char *const command, const char *const args) {
	if (to == NULL)
		return(FE_SUCCESS);

	if (args == NULL) {
		if (irc_send_printf(c,"NOTICE %s :\001%s\001",to,command) != 0) {
			irc_internal_disconnect(c,FE_PACKET);
			return(FE_PACKET);
		}
	} else {
		if (irc_send_printf(c,"NOTICE %s :\001%s %s\001",to,command,args) != 0) {
			irc_internal_disconnect(c,FE_PACKET);
			return(FE_PACKET);
		}
	}
	return(FE_SUCCESS);
}
#endif

static char *irc_ctcp_encode(irc_conn_t *c, const char *const command, const char *const message) {
	char	*str;

	if (message != NULL) {
		str = malloc(1 + strlen(command) + 1 + strlen(message) + 1 + 1);
		if (str == NULL)
			abort();
		sprintf(str, "\001%s %s\001", command, message);
	} else {
		str = malloc(1 + strlen(command) + 1 + 1);
		if (str == NULL)
			abort();
		sprintf(str, "\001%s\001", command);
	}

	return(str);
}

const firetalk_driver_t firetalk_protocol_irc = {
	strprotocol:		"IRC",
	default_server:		"irc.n.ml.org",
	default_port:		6667,
	default_buffersize:	512,
	periodic:		irc_periodic,
	preselect:		irc_preselect,
	postselect:		irc_postselect,
	got_data:		irc_got_data,
	got_data_connecting:	irc_got_data_connecting,
	comparenicks:		irc_compare_nicks,
	isprintable:		irc_isprint,
	disconnect:		irc_disconnect,
	disconnected:		irc_disconnected,
	signon:			irc_signon,
	get_info:		irc_get_info,
	set_info:		irc_set_info,
	set_away:		irc_set_away,
	set_nickname:		irc_set_nickname,
	set_password:		irc_set_password,
	im_add_buddy:		irc_im_add_buddy,
	im_remove_buddy:	irc_im_remove_buddy,
	im_add_deny:		irc_im_add_deny,
	im_remove_deny:		irc_im_remove_deny,
	im_send_message:	irc_im_send_message,
	im_send_action:		irc_im_send_action,
	im_evil:		irc_im_evil,
	chat_join:		irc_chat_join,
	chat_part:		irc_chat_part,
	chat_invite:		irc_chat_invite,
	chat_set_topic:		irc_chat_set_topic,
	chat_op:		irc_chat_op,
	chat_deop:		irc_chat_deop,
	chat_kick:		irc_chat_kick,
	chat_send_message:	irc_chat_send_message,
	chat_send_action:	irc_chat_send_action,
//	subcode_send_request:	irc_subcode_send_request,
//	subcode_send_reply:	irc_subcode_send_reply,
	subcode_encode:		irc_ctcp_encode,
	set_privacy:		irc_set_privacy,
	room_normalize:		irc_normalize_room_name,
	create_conn:		irc_create_conn,
	destroy_conn:		irc_destroy_conn,
};
