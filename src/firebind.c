/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>
#include <naim/modutil.h>
#include "naim-int.h"

extern conn_t	*curconn;
extern win_t	win_input;
extern char	**names;
extern int	namec;
extern namescomplete_t namescomplete;
extern time_t	now;
extern double	nowf;
extern int	awayc;
extern awayar_t *awayar;

#define nFIRE_HANDLER(func) \
static void _firebind_ ## func(struct firetalk_connection_t *sess, conn_t *conn, ...)

#define nFIRE_CTCPHAND(func) \
static void _firebind_ctcp_ ## func(struct firetalk_connection_t *sess, conn_t *conn, const char *from, const char *command, const char *args)

#define nFIRE_CTCPREPHAND(func) \
static void _firebind_ctcprep_ ## func(struct firetalk_connection_t *sess, conn_t *conn, const char *from, const char *command, const char *args)

HOOK_DECLARE(proto_nickchanged);
nFIRE_HANDLER(nickchanged) {
	va_list	msg;
	const char *newnick;

	va_start(msg, conn);
	newnick = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_nickchanged, HOOK_T_CONN HOOK_T_STRING, conn, newnick);
}

HOOK_DECLARE(proto_buddy_nickchanged);
nFIRE_HANDLER(buddy_nickchanged) {
	va_list	msg;
	const char *oldnick, *newnick;

	va_start(msg, conn);
	oldnick = va_arg(msg, const char *);
	newnick = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddy_nickchanged, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING, conn, oldnick, newnick);
}

HOOK_DECLARE(proto_doinit);
nFIRE_HANDLER(doinit) {
	va_list	msg;
	const char *screenname;

	va_start(msg, conn);
	screenname = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_doinit, HOOK_T_CONN HOOK_T_STRING, conn, screenname);
}

nFIRE_HANDLER(setidle) {
	va_list	msg;
	long	*idle, idletime = script_getvar_int("idletime");

	va_start(msg, conn);
	idle = va_arg(msg, long *);
	va_end(msg);

	if ((*idle)/60 != idletime)
		*idle = 60*idletime;
}

HOOK_DECLARE(proto_warned);
nFIRE_HANDLER(warned) {
	va_list	msg;
	const char *who;
	uint32_t newlev;

	va_start(msg, conn);
	newlev = va_arg(msg, int);
	who = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_warned, HOOK_T_CONN HOOK_T_UINT32 HOOK_T_STRING, conn, newlev, who);
}

HOOK_DECLARE(proto_buddy_idle);
nFIRE_HANDLER(buddy_idle) {
	va_list	msg;
	const char *who;
	uint32_t idletime;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	idletime = va_arg(msg, long);
	va_end(msg);

	HOOK_CALL(proto_buddy_idle, HOOK_T_CONN HOOK_T_STRING HOOK_T_UINT32, conn, who, idletime);
}

HOOK_DECLARE(proto_buddy_eviled);
nFIRE_HANDLER(buddy_eviled) {
	va_list	msg;
	const char *who;
	uint32_t warnval;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	warnval = va_arg(msg, long);
	va_end(msg);

	HOOK_CALL(proto_buddy_eviled, HOOK_T_CONN HOOK_T_STRING HOOK_T_UINT32, conn, who, warnval);
}

HOOK_DECLARE(proto_buddy_capschanged);
nFIRE_HANDLER(buddy_caps) {
	va_list	msg;
	const char *who, *caps;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	caps = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddy_capschanged, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING, conn, who, caps);
}

HOOK_DECLARE(proto_buddy_typing);
nFIRE_HANDLER(buddy_typing) {
	va_list	msg;
	const char *who;
	uint32_t typing;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	typing = va_arg(msg, int);
	va_end(msg);

	HOOK_CALL(proto_buddy_typing, HOOK_T_CONN HOOK_T_STRING HOOK_T_UINT32, conn, who, typing);
}

HOOK_DECLARE(proto_buddy_away);
nFIRE_HANDLER(buddy_away) {
	va_list	msg;
	const char *who;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddy_away, HOOK_T_CONN HOOK_T_STRING, conn, who);
}

HOOK_DECLARE(proto_buddy_unaway);
nFIRE_HANDLER(buddy_unaway) {
	va_list	msg;
	const char *who;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddy_unaway, HOOK_T_CONN HOOK_T_STRING, conn, who);
}

HOOK_DECLARE(proto_buddyadded);
nFIRE_HANDLER(buddyadded) {
	va_list	msg;
	const char *screenname, *group, *friendly;

	va_start(msg, conn);
	screenname = va_arg(msg, const char *);
	group = va_arg(msg, const char *);
	friendly = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddyadded, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, screenname, group, friendly);
}

HOOK_DECLARE(proto_buddyremoved);
nFIRE_HANDLER(buddyremoved) {
	va_list	msg;
	const char *screenname;

	va_start(msg, conn);
	screenname = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddyremoved, HOOK_T_CONN HOOK_T_STRING, conn, screenname);
}

HOOK_DECLARE(proto_denyadded);
nFIRE_HANDLER(denyadded) {
	va_list	msg;
	const char *screenname;

	va_start(msg, conn);
	screenname = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_denyadded, HOOK_T_CONN HOOK_T_STRING, conn, screenname);
}

HOOK_DECLARE(proto_denyremoved);
nFIRE_HANDLER(denyremoved) {
	va_list	msg;
	const char *screenname;

	va_start(msg, conn);
	screenname = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_denyremoved, HOOK_T_CONN HOOK_T_STRING, conn, screenname);
}

HOOK_DECLARE(proto_buddy_coming);
nFIRE_HANDLER(buddy_coming) {
	va_list	msg;
	const char *who;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddy_coming, HOOK_T_CONN HOOK_T_STRING, conn, who);
}

HOOK_DECLARE(proto_buddy_going);
nFIRE_HANDLER(buddy_going) {
	va_list	msg;
	const char *who;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_buddy_going, HOOK_T_CONN HOOK_T_STRING, conn, who);
}

HOOK_DECLARE(proto_recvfrom);
static void naim_recvfrom(conn_t *const conn,
		const char *const _name, 
		const char *const _dest,
		const unsigned char *_message, uint32_t len,
		uint32_t flags) {
 	char	*name = NULL, *dest = NULL;
	unsigned char *message = malloc(len+1);

	if (_name != NULL)
		name = strdup(_name);
	if (_dest != NULL)
		dest = strdup(_dest);

	memmove(message, _message, len);
	message[len] = 0;
	HOOK_CALL(proto_recvfrom, HOOK_T_CONN HOOK_T_WRSTRING HOOK_T_WRSTRING HOOK_T_WRLSTRING HOOK_T_WRUINT32,
		conn, &name, &dest, &message, &len, &flags);
	free(name);
	free(dest);
	free(message);
}

static void do_replace(unsigned char *dest, const unsigned char *new, int wordlen, int len) {
	int	newlen = strlen(new);

	if (newlen > wordlen)
		memmove(dest+newlen, dest+wordlen, len-newlen);
	else if (newlen < wordlen)
		memmove(dest+newlen, dest+wordlen, len-wordlen);

	memmove(dest, new, newlen);
}

static void str_replace(const unsigned char *orig, const unsigned char *new, unsigned char *str, int strsize) {
	int	i, l = strlen(orig);

	assert(*str != 0);

	for (i = 0; (str[i] != 0) && (i+l < strsize); i++) {
		if (i > 0)
			switch (str[i-1]) {
				case ' ':
				case '>':
				case '(':
					break;
				default:
					continue;
			}
		switch (str[i+l]) {
			case ' ':
			case ',':
			case '.':
			case '!':
			case '?':
			case '<':
			case ')':
			case 0:
				break;
			default:
				continue;
		}
		if (strncmp(str+i, orig, l) == 0)
			do_replace(str+i, new, l, strsize-i);
	}
}

html_clean_t *html_cleanar = NULL;
int	html_cleanc = 0;

static const unsigned char *html_clean(const unsigned char *str) {
	static unsigned char buf[1024*4];
	int	i;

	assert(str != NULL);
	if (*str == 0)
		return(str);
	strncpy(buf, str, sizeof(buf)-1);
	buf[sizeof(buf)-1] = 0;
	for (i = 0; i < html_cleanc; i++)
		str_replace(html_cleanar[i].from, html_cleanar[i].replace, buf, sizeof(buf));
	return(buf);
}

nFIRE_HANDLER(im_handle) {
	va_list	msg;
	const char *name;
	int	isautoreply;
	const unsigned char *message;

	va_start(msg, conn);
	name = va_arg(msg, const char *);
	isautoreply = va_arg(msg, int);
	message = html_clean(va_arg(msg, const unsigned char *));
	va_end(msg);

	assert(message != NULL);

	naim_recvfrom(conn, name, NULL, message, strlen(message),
		isautoreply?RF_AUTOMATIC:RF_NONE);
}

nFIRE_HANDLER(act_handle) {
	va_list	msg;
	const char *who;
	int	isautoreply;
	const unsigned char *message;

	va_start(msg, conn);
	who = va_arg(msg, const char *);
	isautoreply = va_arg(msg, int);
	message = va_arg(msg, const unsigned char *);
	va_end(msg);

	if (message == NULL)
		message = "";

	naim_recvfrom(conn, who, NULL, message, strlen(message),
		isautoreply?RF_AUTOMATIC:RF_NONE | RF_ACTION);
}

nFIRE_HANDLER(chat_getmessage) {
	va_list	msg;
	const char *room, *who;
	int	isautoreply;
	const unsigned char *message;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	isautoreply = va_arg(msg, int);
	message = html_clean(va_arg(msg, const unsigned char *));
	va_end(msg);

	assert(who != NULL);
	assert(message != NULL);

	if ((conn->sn != NULL) && (firetalk_compare_nicks(conn->conn, who, conn->sn) == FE_SUCCESS))
		return;

	naim_recvfrom(conn, who, room, message, strlen(message),
		isautoreply?RF_AUTOMATIC:RF_NONE);
}

nFIRE_HANDLER(chat_act_handle) {
	va_list	msg;
	const char *room, *who;
	int	isautoreply;
	const unsigned char *message;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	isautoreply = va_arg(msg, int);
	message = va_arg(msg, const unsigned char *);
	va_end(msg);

	if (firetalk_compare_nicks(conn->conn, who, conn->sn) == FE_SUCCESS)
		return;

	if (message == NULL)
		message = "";

	naim_recvfrom(conn, who, room, message, strlen(message),
		isautoreply?RF_AUTOMATIC:RF_NONE | RF_ACTION);
}

void	naim_awaylog(conn_t *conn, const char *src, const char *msg) {
	naim_recvfrom(conn, src, ":AWAYLOG", msg, strlen(msg), RF_NOLOG);
}

HOOK_DECLARE(proto_connected);
nFIRE_HANDLER(connected) {
	HOOK_CALL(proto_connected, HOOK_T_CONN, conn);
}

HOOK_DECLARE(proto_connectfailed);
nFIRE_HANDLER(connectfailed) {
	va_list	msg;
	uint32_t err;
	const char *reason;

	va_start(msg, conn);
	err = va_arg(msg, int);
	reason = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_connectfailed, HOOK_T_CONN HOOK_T_UINT32 HOOK_T_STRING, conn, err, reason);
}

HOOK_DECLARE(proto_error_msg);
nFIRE_HANDLER(error_msg) {
	va_list	msg;
	uint32_t error;
	const char *target, *desc;

	va_start(msg, conn);
	error = va_arg(msg, int);
	target = va_arg(msg, const char *);
	desc = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_error_msg, HOOK_T_CONN HOOK_T_UINT32 HOOK_T_STRING HOOK_T_STRING, conn, error, target, desc);
}

HOOK_DECLARE(proto_disconnected);
nFIRE_HANDLER(disconnected) {
	va_list	msg;
	uint32_t error;

	va_start(msg, conn);
	error = va_arg(msg, int);
	va_end(msg);

	HOOK_CALL(proto_disconnected, HOOK_T_CONN HOOK_T_UINT32, conn, error);
}

nFIRE_HANDLER(needpass) {
	va_list	msg;
	char	*pass;
	int	len;
	const char *mypass;

	va_start(msg, conn);
	pass = va_arg(msg, char *);
	len = va_arg(msg, int);
	va_end(msg);

	assert(len > 1);

	if ((mypass = getvar(conn, "password")) == NULL) {
		if (conn != curconn)
			curconn = conn;
		echof(conn, NULL, "Password required to connect to %s.\n",
			conn->winname);
		echof(conn, NULL, "Please type your password and press Enter.\n");
		nw_getpass(&win_input, pass, len);
		nw_erase(&win_input);
		statrefresh();
	} else {
		strncpy(pass, mypass, len-1);
		pass[len-1] = 0;
	}
}

HOOK_DECLARE(proto_userinfo);
nFIRE_HANDLER(userinfo_handler) {
	va_list	msg;
	const char *SN;
	const unsigned char *info;
	uint32_t warning, online, idle, class;

	va_start(msg, conn);
	SN = va_arg(msg, const char *);
	info = va_arg(msg, const unsigned char *);
	warning = va_arg(msg, long);
	online = va_arg(msg, long);
	idle = va_arg(msg, long);
	class = va_arg(msg, long);
	va_end(msg);

	HOOK_CALL(proto_userinfo, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32 HOOK_T_UINT32 HOOK_T_UINT32 HOOK_T_UINT32,
		conn, SN, info, warning, online, idle, class);
}

HOOK_DECLARE(proto_chat_joined);
nFIRE_HANDLER(chat_joined) {
	va_list	msg;
	const char *room;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_joined, HOOK_T_CONN HOOK_T_STRING, conn, room);
}

HOOK_DECLARE(proto_chat_synched);
nFIRE_HANDLER(chat_synched) {
	va_list	msg;
	const char *room;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_synched, HOOK_T_CONN HOOK_T_STRING, conn, room);
}

HOOK_DECLARE(proto_chat_left);
nFIRE_HANDLER(chat_left) {
	va_list	msg;
	const char *room;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_left, HOOK_T_CONN HOOK_T_STRING, conn, room);
}

HOOK_DECLARE(proto_chat_kicked);
nFIRE_HANDLER(chat_kicked) {
	va_list	msg;
	const char *room, *by, *reason;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	reason = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_kicked, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, by, reason);
}

HOOK_DECLARE(proto_chat_invited);
nFIRE_HANDLER(chat_invited) {
	va_list	msg;
	const char *room, *who, *message;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	message = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_invited, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, who, message);
}

HOOK_DECLARE(proto_chat_user_joined);
nFIRE_HANDLER(chat_JOIN) {
	va_list	msg;
	const char *room, *who, *extra;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	extra = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_user_joined, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, who, extra);
}

HOOK_DECLARE(proto_chat_user_left);
nFIRE_HANDLER(chat_PART) {
	va_list	msg;
	const char *room, *who, *reason;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	reason = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_user_left, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, who, reason);
}

HOOK_DECLARE(proto_chat_user_kicked);
nFIRE_HANDLER(chat_KICK) {
	va_list	msg;
	const char *room, *who, *by, *reason;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	reason = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_user_kicked, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, who, by, reason);
}

HOOK_DECLARE(proto_chat_keychanged);
nFIRE_HANDLER(chat_KEYCHANGED) {
	va_list	msg;
	const char *room, *what, *by;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	what = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_keychanged, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, what, by);
}

HOOK_DECLARE(proto_chat_modeset);
nFIRE_HANDLER(chat_modeset) {
	va_list	msg;
	const char *room, *by, *mode, *arg;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	mode = va_arg(msg, const char *);
	arg = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_modeset, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, by, mode, arg);
}

HOOK_DECLARE(proto_chat_modeunset);
nFIRE_HANDLER(chat_modeunset) {
	va_list	msg;
	const char *room, *by, *mode, *arg;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	mode = va_arg(msg, const char *);
	arg = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_modeunset, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, by, mode, arg);
}

HOOK_DECLARE(proto_chat_oped);
nFIRE_HANDLER(chat_oped) {
	va_list	msg;
	const char *room, *by;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_oped, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING, conn, room, by);
}

HOOK_DECLARE(proto_chat_user_oped);
nFIRE_HANDLER(chat_OP) {
	va_list	msg;
	const char *room, *who, *by;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_user_oped, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, who, by);
}

HOOK_DECLARE(proto_chat_deoped);
nFIRE_HANDLER(chat_deoped) {
	va_list	msg;
	const char *room, *by;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_deoped, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING, conn, room, by);
}

HOOK_DECLARE(proto_chat_user_deoped);
nFIRE_HANDLER(chat_DEOP) {
	va_list	msg;
	const char *room, *who, *by;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	who = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_user_deoped, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, who, by);
}

HOOK_DECLARE(proto_chat_topicchanged);
nFIRE_HANDLER(chat_TOPIC) {
	va_list	msg;
	const char *room, *topic, *by;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	topic = va_arg(msg, const char *);
	by = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_topicchanged, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, topic, by);
}

HOOK_DECLARE(proto_chat_user_nickchanged);
nFIRE_HANDLER(chat_NICK) {
	va_list	msg;
	const char *room, *oldnick, *newnick;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	oldnick = va_arg(msg, const char *);
	newnick = va_arg(msg, const char *);
	va_end(msg);

	HOOK_CALL(proto_chat_user_nickchanged, HOOK_T_CONN HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, conn, room, oldnick, newnick);
}

nFIRE_HANDLER(chat_NAMES) {
	va_list	msg;
	const char *room, *nick;
	int	oped;
//	int	i, j;

	va_start(msg, conn);
	room = va_arg(msg, const char *);
	nick = va_arg(msg, const char *);
	oped = va_arg(msg, int);
	va_end(msg);

	if (namescomplete.buf != NULL) {
		assert(namescomplete.len > 0);
		if (namescomplete.foundmatch) {
			if (!namescomplete.foundmult && (strncasecmp(nick, namescomplete.buf, namescomplete.len) == 0))
				namescomplete.foundmult = 1;
			return;
		}
		if (strlen(namescomplete.buf) > namescomplete.len) {
			int	len = strlen(namescomplete.buf);

			assert(len > 0);
			if (namescomplete.buf[len-1] == ' ');
				len--;
			assert(len > 0);
			if (namescomplete.buf[len-1] == ',');
				len--;
			assert(len > 0);
			if (strncmp(namescomplete.buf, nick, len) == 0) {
				namescomplete.foundmult = namescomplete.foundfirst = 1;
				return;
			} else if (!namescomplete.foundfirst) {
				if (!namescomplete.foundmult && (strncasecmp(nick, namescomplete.buf, namescomplete.len) == 0))
					namescomplete.foundmult = 1;
				return;
			}
		}
		if (strncasecmp(nick, namescomplete.buf, namescomplete.len) == 0) {
			free(namescomplete.buf);
			namescomplete.buf = strdup(nick);
			namescomplete.foundmatch = 1;
		}
		return;
	}

	namec++;
	names = realloc(names, namec*sizeof(*names));
	names[namec-1] = malloc(strlen(nick) + oped + 1);
	sprintf(names[namec-1], "%s%s", oped?"@":"", nick);
//	for (i = 0, j = strlen(namesbuf); nick[i] != 0; i++, j++)
//		if (isspace(nick[i]))
//			namesbuf[j] = '_';
//		else
//			namesbuf[j] = nick[i];
//	namesbuf[j] = ' ';
//	namesbuf[j+1] = 0;
}

static int qsort_strcasecmp(const void *p1, const void *p2) {
	register char **b1 = (char **)p1, **b2 = (char **)p2;

	return(strcasecmp(*b1, *b2));
}

void	naim_chat_listmembers(conn_t *conn, const char *const chat) {
	firetalk_chat_listmembers(conn->conn, chat);
	if (names != NULL)
		qsort(names, namec, sizeof(*names), qsort_strcasecmp);
}

HOOK_DECLARE(proto_file_offer);
nFIRE_HANDLER(file_offer) {
	va_list	msg;
	struct firetalk_transfer_t *handle;
	const char *from, *filename;
	uint32_t size;

	va_start(msg, conn);
	handle = va_arg(msg, struct firetalk_transfer_t *);
	from = va_arg(msg, const char *);
	filename = va_arg(msg, const char *);
	size = va_arg(msg, long);
	va_end(msg);

	HOOK_CALL(proto_file_offer, HOOK_T_CONN HOOK_T_HANDLE HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32, conn, handle, from, filename, size);
}

HOOK_DECLARE(proto_file_start);
nFIRE_HANDLER(file_start) {
	va_list	msg;
	struct firetalk_transfer_t *handle;
	transfer_t *transfer;

	va_start(msg, conn);
	handle = va_arg(msg, struct firetalk_transfer_t *);
	transfer = va_arg(msg, transfer_t *);
	va_end(msg);

	HOOK_CALL(proto_file_start, HOOK_T_CONN HOOK_T_HANDLE HOOK_T_HANDLE, conn, handle, transfer);
}

HOOK_DECLARE(proto_file_progress);
nFIRE_HANDLER(file_progress) {
	va_list	msg;
	struct firetalk_transfer_t *handle;
	transfer_t *transfer;
	uint32_t bytes, size;

	va_start(msg, conn);
	handle = va_arg(msg, struct firetalk_transfer_t *);
	transfer = va_arg(msg, transfer_t *);
	bytes = va_arg(msg, long);
	size = va_arg(msg, long);
	va_end(msg);

	HOOK_CALL(proto_file_progress, HOOK_T_CONN HOOK_T_HANDLE HOOK_T_HANDLE HOOK_T_UINT32 HOOK_T_UINT32, conn, handle, transfer, bytes, size);
}

HOOK_DECLARE(proto_file_finish);
nFIRE_HANDLER(file_finish) {
	va_list	msg;
	struct firetalk_transfer_t *handle;
	transfer_t *transfer;
	uint32_t size;

	va_start(msg, conn);
	handle = va_arg(msg, struct firetalk_transfer_t *);
	transfer = va_arg(msg, transfer_t *);
	size = va_arg(msg, long);
	va_end(msg);

	HOOK_CALL(proto_file_finish, HOOK_T_CONN HOOK_T_HANDLE HOOK_T_HANDLE HOOK_T_UINT32, conn, handle, transfer, size);
}

HOOK_DECLARE(proto_file_error);
nFIRE_HANDLER(file_error) {
	va_list	msg;
	struct firetalk_transfer_t *handle;
	transfer_t *transfer;
	uint32_t error;

	va_start(msg, conn);
	handle = va_arg(msg, struct firetalk_transfer_t *);
	transfer = va_arg(msg, transfer_t *);
	error = va_arg(msg, int);
	va_end(msg);

	HOOK_CALL(proto_file_error, HOOK_T_CONN HOOK_T_HANDLE HOOK_T_HANDLE HOOK_T_UINT32, conn, handle, transfer, error);
}

static time_t lastctcp = 0;

nFIRE_CTCPHAND(PING) {
	if (lastctcp < now-1) {
		firetalk_subcode_send_reply(sess, from, "PING", args);
		lastctcp = now;
		echof(conn, "CTCP", "<font color=\"#00FFFF\">%s</font> pinged you.\n",
			from);
	}
}

nFIRE_CTCPHAND(LC) {
	if ((args == NULL) || (*args == 0))
		return;

	if (firetalk_compare_nicks(conn->conn, conn->sn, from) == FE_SUCCESS) {
		conn->lag = nowf - atof(args);
		bupdate();
	}
}

nFIRE_CTCPHAND(HEXTEXT) {
	unsigned char
		buf[4*1024];
	int	i;

	if ((args == NULL) || (*args == 0))
		return;

	for (i = 0; (i/2 < sizeof(buf)-1) && (args[i] != 0) && (args[i+1] != 0); i += 2)
		buf[i/2] = (hexdigit(args[i]) << 4) | hexdigit(args[i+1]);
	buf[i/2] = 0;

#if 0
	echof(curconn, "HEXTEXT", "<-- %s %s %s", from, args, buf);
#endif

	naim_recvfrom(conn, from, NULL, buf, i/2, RF_ENCRYPTED);
}

nFIRE_CTCPREPHAND(HEXTEXT) {
	char	buf[4*1024];
	int	i;

	if ((args == NULL) || (*args == 0))
		return;

	for (i = 0; (i/2 < sizeof(buf)-1) && (args[i] != 0) && (args[i+1] != 0); i += 2)
		buf[i/2] = (hexdigit(args[i]) << 4) | hexdigit(args[i+1]);
	buf[i/2] = 0;

#if 0
	echof(curconn, "HEXTEXT", "<-- %s %s %s", from, args, buf);
#endif

	naim_recvfrom(conn, from, NULL, buf, i/2, RF_ENCRYPTED | RF_AUTOMATIC);
}

nFIRE_CTCPHAND(AUTOPEER) {
	buddylist_t *blist;
	char	*str;

	if ((args == NULL) || (*args == 0))
		return;

	if ((blist = rgetlist(conn, from)) == NULL) {
		if (getvar_int(conn, "autopeerverbose") > 0)
			status_echof(conn, "Received autopeer message (%s) from non-buddy %s.\n",
				args, from);
		if ((strcmp(args, "-AUTOPEER") != 0) && (strcmp(args, "-AUTOCRYPT") != 0)) {
			if (getvar_int(conn, "autobuddy")) {
				status_echof(conn, "Adding <font color=\"#00FFFF\">%s</font> to your buddy list due to autopeer.\n",
					from);
				blist = raddbuddy(conn, from, DEFAULT_GROUP, NULL);
				bnewwin(conn, from, BUDDY);
				firetalk_im_add_buddy(conn->conn, from, USER_GROUP(blist), NULL);
			} else {
				if (getvar_int(conn, "autopeerverbose") > 0)
					status_echof(conn, "Declining automatic negotiation with <font color=\"#00FFFF\">%s</font> (add <font color=\"#00FFFF\">%s</font> to your buddy list).\n", 
						from, from);
				firetalk_subcode_send_request(conn->conn, from, "AUTOPEER", "-AUTOPEER");
				return;
			}
		} else {
			if (getvar_int(conn, "autopeerverbose") > 0)
				status_echof(conn, "... ignored.\n");
			return;
		}
	}
	assert(blist != NULL);

	str = strdup(args);
	args = str;
	while (args != NULL) {
		char	buf[1024],
			*sp = strchr(args, ' '),
			*co;

		if (sp != NULL)
			*sp = 0;

		if ((co = strchr(args, ':')) != NULL) {
			*co = 0;
			co++;
			if (*co == 0)
				co = NULL;
		}

		if (strcmp(args, "--") == 0)
			break;
		else if (strcasecmp(args, "+AUTOPEER") == 0) {
			int	lev;

			if (co != NULL)
				lev = atoi(co);
			else
				lev = 1;

			if (blist->peer != lev) {
				const char
					*autocrypt_flag,
					*autozone_flag1,
					*autozone_flag2;

				if (getvar_int(conn, "autopeerverbose") > 0)
					status_echof(conn, "Peer level %i automatically negotiated with %s.\n",
						lev, from);

				if ((lev > 2) && (blist->peer == 0) && (blist->crypt == NULL) && (getvar_int(conn, "autocrypt") > 0))
					autocrypt_flag = " +AUTOCRYPT";
				else
					autocrypt_flag = "";

				if ((lev > 2) && ((autozone_flag2 = getvar(conn, "autozone")) != NULL) && (*autozone_flag2 != 0))
					autozone_flag1 = " +AUTOZONE:";
				else {
					autozone_flag1 = "";
					autozone_flag2 = "";
				}

				snprintf(buf, sizeof(buf), "+AUTOPEER:%i%s%s%s", 4,
					autocrypt_flag, autozone_flag1, autozone_flag2);

				blist->peer = lev;
				firetalk_subcode_send_request(conn->conn, from, "AUTOPEER", buf);
			}
		} else if (strcasecmp(args, "-AUTOPEER") == 0) {
			if (getvar_int(conn, "autopeerverbose") > 0) {
				if (blist->peer == 0)
					status_echof(conn, "Automatic negotiation with <font color=\"#00FFFF\">%s</font> declined (you are probably not on <font color=\"#00FFFF\">%s</font>'s buddy list).\n", 
						from, from);
				else if (blist->peer > 0)
					status_echof(conn, "Negotiated session with <font color=\"#00FFFF\">%s</font> terminated.\n",
						from);
			}
			if (blist->crypt != NULL) {
				free(blist->crypt);
				blist->crypt = NULL;
				firetalk_subcode_send_request(conn->conn, from, "AUTOPEER", "-AUTOCRYPT");
			}
			blist->peer = 0;
		} else if (strcasecmp(args, "+AUTOCRYPT") == 0) {
			if (!getvar_int(conn, "autocrypt"))
				firetalk_subcode_send_request(conn->conn, from, "AUTOPEER", "-AUTOCRYPT");
			else {
				if (co == NULL) {
					char	key[21],
						buf[1024];
					int	i = 0;

					while (i < sizeof(key)-1) {
						key[i] = 1 + rand()%255;
						if (!isspace(key[i]) && (firetalk_isprint(conn->conn, key[i]) == FE_SUCCESS))
							i++;
					}
					key[i] = 0;

					snprintf(buf, sizeof(buf), "+AUTOCRYPT:%s", key);
					firetalk_subcode_send_request(conn->conn, from, "AUTOPEER", buf);

					co = key;
				}

				if ((blist->crypt == NULL) || (strcmp(blist->crypt, co) != 0)) {
					STRREPLACE(blist->crypt, co);
					if (getvar_int(conn, "autopeerverbose") > 0)
						status_echof(conn, "Now encrypting messages sent to <font color=\"#00FFFF\">%s</font> with XOR [%s].\n",
							from, co);
				}
			}
		} else if (strcasecmp(args, "+AUTOZONE") == 0) {
			if (co == NULL)
				status_echof(conn, "Received blank time zone from peer <font color=\"#00FFFF\">%s</font> <scratches head>.\n",
					from);
			else
				STRREPLACE(blist->tzname, co);
		}

		if ((strcasecmp(args, "-AUTOCRYPT") == 0) || (strcasecmp(args, "-AUTOPEER") == 0)) {
			if (blist->crypt != NULL) {
				free(blist->crypt);
				blist->crypt = NULL;
				firetalk_subcode_send_request(conn->conn, from, "AUTOPEER", "-AUTOCRYPT");
				if (getvar_int(conn, "autopeerverbose") > 0)
					status_echof(conn, "No longer encrypting messages sent to %s.\n",
						from);
			}
		}

		if (sp != NULL) {
			args = sp+1;
			while (isspace(*args))
				args++;
			if (*args == 0)
				args = NULL;
		} else
			args = NULL;
	}
	free(str);
}

nFIRE_CTCPHAND(default) {
	if (getvar_int(conn, "ctcpverbose") > 0) {
		if (args == NULL)
			echof(conn, "CTCP", "Unknown CTCP %s from <font color=\"#00FFFF\">%s</font>.\n",
				command, from);
		else
			echof(conn, "CTCP", "Unknown CTCP %s from <font color=\"#00FFFF\">%s</font>: %s.\n",
				command, from, args);
	}
}

nFIRE_CTCPREPHAND(VERSION) {
	char	*str = strdup(args), *ver, *env;
	int	i, show = 1;

	for (i = 0; i < awayc; i++)
		if (firetalk_compare_nicks(conn->conn, from, awayar[i].name) == FE_SUCCESS) {
			show = 0;
			break;
		}

	if (((ver = strchr(str, ':')) != NULL)
	 && ((env = strchr(ver+1, ':')) != NULL)
	 && (strchr(env+1, ':') == NULL)) {
		*ver++ = 0;
		*env++ = 0;
		if (show)
			echof(conn, NULL, "<font color=\"#00FFFF\">%s</font> is running %s version %s (%s).\n",
				from, str, ver, env);
	} else if (show)
		echof(conn, NULL, "CTCP VERSION reply from <font color=\"#00FFFF\">%s</font>: %s.\n",
			from, args);
	free(str);
}

nFIRE_CTCPREPHAND(AWAY) {
	int	time;
	const char *rest;
	buddywin_t *bwin;

	if ((args == NULL) || (*args == 0))
		return;

	time = atoi(args);

	if (((time > 0) || (strncmp(args, "0 ", 2) == 0))
		&& ((rest = strchr(args, ' ')) != NULL)) {
		rest++;
		if (*rest == ':')
			rest++;
	} else {
		time = -1;
		rest = args;
	}

	if ((bwin = bgetwin(conn, from, BUDDY)) != NULL)
		STRREPLACE(bwin->blurb, rest);

	if (awayc > 0) {
		int	i;

		assert(awayar != NULL);

		for (i = 0; i < awayc; i++)
			if (firetalk_compare_nicks(conn->conn, from, awayar[i].name) == FE_SUCCESS) {
				if (bwin == NULL)
					status_echof(conn, "<font color=\"#00FFFF\">%s</font> is now away: %s.\n",
						from, rest);
				else
					window_echof(bwin, "<font color=\"#00FFFF\">%s</font> is now away: %s.\n",
						user_name(NULL, 0, conn, bwin->e.buddy), rest);
				awayar[i].gotaway = 1;
				return;
			}
	}

	if (time >= 0)
		echof(conn, NULL, "<font color=\"#00FFFF\">%s</font> has been away for %s: %s.\n",
			from, dtime(time*60), rest);
	else
		echof(conn, NULL, "CTCP AWAY reply from <font color=\"#00FFFF\">%s</font>: %s.\n",
			from, rest);
}

nFIRE_CTCPREPHAND(default) {
	if (args == NULL)
		echof(conn, NULL, "CTCP %s reply from <font color=\"#00FFFF\">%s</font>.\n",
			command, from);
	else
		echof(conn, NULL, "CTCP %s reply from <font color=\"#00FFFF\">%s</font>: %s.\n",
			command, from, args);
}

conn_t	*naim_newconn(int proto) {
	conn_t	*conn = calloc(1, sizeof(conn_t));

	assert(conn != NULL);

	conn->proto = proto;
	naim_lastupdate(conn);

	{
		conn->conn = firetalk_create_conn(proto, conn);
		firetalk_register_callback(conn->conn, FC_DOINIT,			_firebind_doinit);
		firetalk_register_callback(conn->conn, FC_CONNECTED,			_firebind_connected);
		firetalk_register_callback(conn->conn, FC_CONNECTFAILED,		_firebind_connectfailed);
		firetalk_register_callback(conn->conn, FC_ERROR,			_firebind_error_msg);
		firetalk_register_callback(conn->conn, FC_DISCONNECT,			_firebind_disconnected);
		firetalk_register_callback(conn->conn, FC_SETIDLE,			_firebind_setidle);

		firetalk_register_callback(conn->conn, FC_EVILED,			_firebind_warned);
		firetalk_register_callback(conn->conn, FC_NEWNICK,			_firebind_nickchanged);
		/* FC_PASSCHANGED */

		firetalk_register_callback(conn->conn, FC_IM_GOTINFO,			_firebind_userinfo_handler);
		firetalk_register_callback(conn->conn, FC_IM_USER_NICKCHANGED,		_firebind_buddy_nickchanged);
		firetalk_register_callback(conn->conn, FC_IM_GETMESSAGE,		_firebind_im_handle);
		firetalk_register_callback(conn->conn, FC_IM_GETACTION,			_firebind_act_handle);
		firetalk_register_callback(conn->conn, FC_IM_BUDDYADDED,		_firebind_buddyadded);
		firetalk_register_callback(conn->conn, FC_IM_BUDDYREMOVED,		_firebind_buddyremoved);
		firetalk_register_callback(conn->conn, FC_IM_DENYADDED,			_firebind_denyadded);
		firetalk_register_callback(conn->conn, FC_IM_DENYREMOVED,		_firebind_denyremoved);
		firetalk_register_callback(conn->conn, FC_IM_BUDDYONLINE,		_firebind_buddy_coming);
		firetalk_register_callback(conn->conn, FC_IM_BUDDYOFFLINE,		_firebind_buddy_going);
		firetalk_register_callback(conn->conn, FC_IM_BUDDYAWAY,			_firebind_buddy_away);
		firetalk_register_callback(conn->conn, FC_IM_BUDDYUNAWAY,		_firebind_buddy_unaway);
		firetalk_register_callback(conn->conn, FC_IM_IDLEINFO,			_firebind_buddy_idle);
		firetalk_register_callback(conn->conn, FC_IM_TYPINGINFO,		_firebind_buddy_typing);
		firetalk_register_callback(conn->conn, FC_IM_EVILINFO,			_firebind_buddy_eviled);
		firetalk_register_callback(conn->conn, FC_IM_CAPABILITIES,		_firebind_buddy_caps);

		firetalk_register_callback(conn->conn, FC_CHAT_JOINED,			_firebind_chat_joined);
		firetalk_register_callback(conn->conn, FC_CHAT_SYNCHED,			_firebind_chat_synched);
		firetalk_register_callback(conn->conn, FC_CHAT_LEFT,			_firebind_chat_left);
		firetalk_register_callback(conn->conn, FC_CHAT_KICKED,			_firebind_chat_kicked);
		firetalk_register_callback(conn->conn, FC_CHAT_KEYCHANGED,		_firebind_chat_KEYCHANGED);
		firetalk_register_callback(conn->conn, FC_CHAT_GETMESSAGE,		_firebind_chat_getmessage);
		firetalk_register_callback(conn->conn, FC_CHAT_GETACTION,		_firebind_chat_act_handle);
		firetalk_register_callback(conn->conn, FC_CHAT_INVITED,			_firebind_chat_invited);
		firetalk_register_callback(conn->conn, FC_CHAT_MODESET,			_firebind_chat_modeset);
		firetalk_register_callback(conn->conn, FC_CHAT_MODEUNSET,		_firebind_chat_modeunset);
		firetalk_register_callback(conn->conn, FC_CHAT_OPPED,			_firebind_chat_oped);
		firetalk_register_callback(conn->conn, FC_CHAT_DEOPPED,			_firebind_chat_deoped);
		firetalk_register_callback(conn->conn, FC_CHAT_USER_JOINED,		_firebind_chat_JOIN);
		firetalk_register_callback(conn->conn, FC_CHAT_USER_LEFT,		_firebind_chat_PART);
		firetalk_register_callback(conn->conn, FC_CHAT_GOTTOPIC,		_firebind_chat_TOPIC);
		firetalk_register_callback(conn->conn, FC_CHAT_USER_OPPED,		_firebind_chat_OP);
		firetalk_register_callback(conn->conn, FC_CHAT_USER_DEOPPED,		_firebind_chat_DEOP);
		firetalk_register_callback(conn->conn, FC_CHAT_USER_KICKED,		_firebind_chat_KICK);
		firetalk_register_callback(conn->conn, FC_CHAT_USER_NICKCHANGED,	_firebind_chat_NICK);
		firetalk_register_callback(conn->conn, FC_CHAT_LISTMEMBER,		_firebind_chat_NAMES);

		firetalk_register_callback(conn->conn, FC_FILE_OFFER,			_firebind_file_offer);
		firetalk_register_callback(conn->conn, FC_FILE_START,			_firebind_file_start);
		firetalk_register_callback(conn->conn, FC_FILE_PROGRESS,		_firebind_file_progress);
		firetalk_register_callback(conn->conn, FC_FILE_FINISH,			_firebind_file_finish);
		firetalk_register_callback(conn->conn, FC_FILE_ERROR,			_firebind_file_error);

		firetalk_register_callback(conn->conn, FC_NEEDPASS,			_firebind_needpass);

		firetalk_subcode_register_request_callback(conn->conn, "PING",		_firebind_ctcp_PING);
		firetalk_subcode_register_request_callback(conn->conn, "LC",		_firebind_ctcp_LC);
		firetalk_subcode_register_request_callback(conn->conn, "HEXTEXT",	_firebind_ctcp_HEXTEXT);
		firetalk_subcode_register_request_callback(conn->conn, "AUTOPEER",	_firebind_ctcp_AUTOPEER);
		firetalk_subcode_register_request_callback(conn->conn, NULL,		_firebind_ctcp_default);

		firetalk_subcode_register_reply_callback(conn->conn, "HEXTEXT",		_firebind_ctcprep_HEXTEXT);
		firetalk_subcode_register_reply_callback(conn->conn, "VERSION",		_firebind_ctcprep_VERSION);
		firetalk_subcode_register_reply_callback(conn->conn, "AWAY",		_firebind_ctcprep_AWAY);
		firetalk_subcode_register_reply_callback(conn->conn, NULL,		_firebind_ctcprep_default);
	}

	return(conn);
}
