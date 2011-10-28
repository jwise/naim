/*
toctest.c - TOC2 server testing code
Copyright (C) 2000 Ian Gulliver
Copyright 2003-2004 Daniel Reed <n@ml.org>
Copyright 2006 Joshua Wise <joshua@joshuawise.com>

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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "firetalk-int.h"
#include "firetalk.h"

#define FIRETALK_TEST_STRING	"FireTalk test for " PACKAGE_NAME " " PACKAGE_VERSION



struct {
	void	*conn;
	char	*username;
	char	*password;
	int	waitingfor;
	char	*waitingstr;
	int	haderror;
}	conn1 = { 0 };

int	proto = -1;

char	*events[] = {
	  "",
#define WF_DOINIT 1
	  "DOINIT",
#define WF_BUDDYONLINE 2
	  "BUDDYONLINE",
#define WF_BUDDYOFFLINE 3
	  "BUDDYOFFLINE",
#define WF_IM_GETMESSAGE 4
	  "IM_GETMESSAGE",
#define WF_IM_GETACTION 5
	  "IM_GETACTION",
#define WF_IM_BUDDYAWAY 6
	  "IM_BUDDYAWAY",
#define WF_IM_BUDDYUNAWAY 7
	  "IM_BUDDYUNAWAY",
#define WF_GOTINFO 8
	  "GOTINFO",
#define WF_SUBCODE_REPLY 9
	  "SUBCODE_REPLY",
#define WF_CHAT_JOINED 10
	  "CHAT_JOINED",
#define WF_CHAT_USER_JOINED 11
	  "CHAT_USER_JOINED",
#define WF_CHAT_USER_LEFT 12
	  "CHAT_USER_LEFT",
#define WF_CHAT_LEFT 13
	  "CHAT_LEFT",
#define WF_CHAT_GETMESSAGE 14
	  "CHAT_GETMESSAGE",
#define WF_CHAT_GETACTION 15
	  "CHAT_GETACTION",
#define WF_CONNECTED 16
	  "CONNECTED",
};

#ifdef DEBUG_ECHO
void statrefresh(void) {
}
void status_echof(void) {
}
void *curconn = NULL;
#endif

#define CALLBACK_SANITY()					\
	assert((c == conn1.conn));


#define WAITING_MUSTBE(EVENT) do {				\
	if (c == conn1.conn) {				\
		assert(conn1.waitingfor == EVENT);		\
		conn1.waitingfor = 0;				\
		conn1.waitingstr = NULL;			\
	} else							\
		abort();					\
	fprintf(stderr, " [caught %s]", events[EVENT]);		\
	fflush(stderr);						\
} while (0)

#define WAITING_WANT(EVENT, STR) do {				\
	if (c == conn1.conn) {					\
		if (conn1.waitingfor == EVENT) {		\
			if (strcmp(STR, conn1.waitingstr) == 0)	{ \
				conn1.waitingfor = 0;		\
				conn1.waitingstr = NULL;	\
			} else					\
				fprintf(stderr, " [%s:%i: warning: spurious %s event (want [[[%s]]], got [[[%s]]])]", \
					__FILE__, __LINE__, events[EVENT], conn1.waitingstr, STR); \
		} else if (conn1.waitingfor != 0)		\
			fprintf(stderr, " [%s:%i: warning: spurious %s event while waiting for %s event]", \
				__FILE__, __LINE__, events[EVENT], events[conn1.waitingfor]); \
		else						\
			fprintf(stderr, " [%s:%i: warning: spurious %s event (unwanted)]", \
				__FILE__, __LINE__, events[EVENT]); \
	} else							\
		abort();					\
	fprintf(stderr, " [caught %s]", events[EVENT]);		\
	fflush(stderr);						\
} while (0)


void	needpass(void *c, void *cs, char *pass, int size) {
	pass[size-1] = 0;
	if (c == conn1.conn)
		strncpy(pass, conn1.password, size-1);
}

void	doinit(void *c, void *cs, char *nickname) {
	CALLBACK_SANITY();
	WAITING_MUSTBE(WF_DOINIT);
	if (c == conn1.conn)
		conn1.waitingfor = WF_CONNECTED;
	else
		abort();
}

void	error(void *c, void *cs, const int error, const char *const roomoruser, const char *const description) {
	CALLBACK_SANITY();
	if (roomoruser == NULL)
		fprintf(stderr, " [%s:%i: error %i (%s): %s]", __FILE__, __LINE__, error, firetalk_strerror(error), description);
	else
		fprintf(stderr, " [%s:%i: error %i in %s (%s): %s]", __FILE__, __LINE__, error, roomoruser, firetalk_strerror(error), description);
	conn1.haderror++;

	fflush(stderr);
}

void	connected(void *c, void *cs) {
	CALLBACK_SANITY();
	WAITING_MUSTBE(WF_CONNECTED);
}

void	connectfailed(void *c, void *cs) {
	CALLBACK_SANITY();
	fprintf(stderr, "\r%s:%i: connection failed\r\n", __FILE__, __LINE__);
	abort();
}

void	buddy_online(void *c, void *cs, char *nickname) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_BUDDYONLINE, nickname);
}

void	buddy_offline(void *c, void *cs, char *nickname) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_BUDDYOFFLINE, nickname);
}

void	im_getmessage(void *c, void *cs, char *n, int a, char *m) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_IM_GETMESSAGE, m);
}

void	im_getaction(void *c, void *cs, char *n, int a, char *m) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_IM_GETACTION, m);
}

void	im_buddyaway(void *c, void *cs, char *nickname) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_IM_BUDDYAWAY, nickname);
}

void	im_buddyunaway(void *c, void *cs, char *nickname) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_IM_BUDDYUNAWAY, nickname);
}

void	gotinfo(void *c, void *cs, char *n, char *i) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_GOTINFO, n);
}

void	subcode_reply(void *c, void *cs, const char *const from, const char *const command, const char *const args) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_SUBCODE_REPLY, args);
}

void	chat_joined(void *c, void *cs, char *room) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_CHAT_JOINED, room);
}

void	chat_user_joined(void *c, void *cs, char *room, char *who) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_CHAT_USER_JOINED, who);
}

void	chat_getmessage(char *c, void *cs, const char *const room, const char *const from, const int autoflag, const char *const m) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_CHAT_GETMESSAGE, m);
}

void	chat_getaction(char *c, void *cs, const char *const room, const char *const from, const int autoflag, const char *const m) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_CHAT_GETACTION, m);
}

void	chat_user_left(void *c, void *cs, char *room, char *who, char *reason) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_CHAT_USER_LEFT, who);
}

void	chat_left(void *c, void *cs, char *room) {
	CALLBACK_SANITY();
	WAITING_WANT(WF_CHAT_LEFT, room);
}

#define DO_TEST(suffix, args, failtest, errorcode, failed) do {				\
	fprintf(stderr, "calling %s...", #suffix #args);				\
	fflush(stderr);									\
	errno = 0;									\
	ret = (void *)firetalk_ ## suffix args;						\
	if (failtest) {									\
		fprintf(stderr, " error %i (%s", (int)errorcode, firetalk_strerror((int)errorcode)); \
		if (errno != 0)								\
			fprintf(stderr, ": %m");					\
		fprintf(stderr, ")\r\n");						\
		failed++;								\
		break;									\
	}										\
	fprintf(stderr, " done");							\
	if ((ret != (void *)0) && (ret < (void *)10000))				\
		fprintf(stderr, " (%i)", (int)ret);					\
	else if (ret != (void *)0)							\
		fprintf(stderr, " (%p)", ret);						\
	fprintf(stderr, "\r\n");							\
} while (0)

#define DO_WAITFOR(conn, event, str, failed) do {					\
	struct timeval t;								\
	time_t	tt;									\
	int myfailed = 0;								\
											\
	conn.waitingfor = event;							\
	conn.waitingstr = str;								\
	conn.haderror = 0;								\
	time(&tt);									\
	fprintf(stderr, "%s waiting (up to 20 seconds) for event %s...\r\n", conn.username, events[event]); \
	while (((tt + 20) > time(NULL)) && (conn.waitingfor != 0) && !conn.haderror) {	\
		int myfailed = 0;							\
		t.tv_sec = 20 - (time(NULL)-tt);					\
		t.tv_usec = 0;								\
		fprintf(stderr, " [%lu]\t", 20 - (time(NULL)-tt));			\
		DO_TEST(select_custom, (0, NULL, NULL, NULL, &t), ret != FE_SUCCESS, ret, myfailed); \
		if (myfailed)								\
			break;								\
	}										\
	if (myfailed) {									\
		failed++;								\
		break;									\
	}										\
	if (conn.haderror) {								\
		fprintf(stderr, "%s had error while waiting for event %s\r\n", conn.username, events[event]); \
		failed++;								\
		break;									\
	}										\
	if (conn.waitingfor != 0) {							\
		fprintf(stderr, "%s waiting (up to 20 seconds) for event %s... timed out waiting for event %s\r\n", conn.username, events[event], events[event]); \
		failed++;								\
		break;									\
	}										\
	fprintf(stderr, "%s waiting (up to 20 seconds) for event %s... done, %s event caught\r\n", conn.username, events[event], events[event]); \
	fprintf(stderr, "%s waiting for server sync...", conn.username);		\
	fflush(stderr);									\
	sleep(1);									\
	fprintf(stderr, " done\r\n");							\
} while (0)

struct {
	char	*username;
	char	*password;
} names[] = {
	{ "TOC2 Test 01", "toc2test" },
	{ "TOC2 Test 02", "toc2test" },
	{ NULL, NULL },
};

struct {
	char	*hostname;
	int	port;
	int	failed;
} servers[] = {
	{ "aimexpress.oscar.aol.com", 5190, -1 },
	{ "toc-m01.blue.aol.com", 9898, -1 },
	{ "toc-m02.blue.aol.com", 9898, -1 },
	{ NULL, 0, -1 }
};

const char *botusername = "Sour is a Hacker";

int	main(int argc, char *argv[]) {
	int	i;
	int	uid = 0;
//	const char *protostr;

	fprintf(stderr, "TOC2 test for FireTalk/" PACKAGE_NAME " " PACKAGE_VERSION "\r\n");
//	for (i = 0; (protostr = firetalk_strprotocol(i)) != NULL; i++) {
	for (i = 0; servers[i].hostname; i++) {
		void	*ret;
		int	proto;

		proto = firetalk_find_protocol("TOC2");
		if (proto == -1)
		{
			fprintf(stderr, "Protocol not found: TOC2\n");
			return 1;
		}

		if (names[uid].username == NULL)
			uid = 0;

		conn1.username = strdup(names[uid].username);
		if (conn1.username == NULL)
			abort();
		conn1.password = strdup(names[uid].password);
		if (conn1.password == NULL)
			abort();

		fprintf(stderr, "Testing server %s on port %d\r\n", servers[i].hostname, servers[i].port);
		fprintf(stderr, "\r\n");
		servers[i].failed = 0;

#define T if (servers[i].failed) goto out;

		DO_TEST(create_conn, (proto, NULL), ret == NULL, firetalkerror, servers[i].failed); T
			conn1.conn = ret;
		DO_TEST(register_callback, (conn1.conn, FC_ERROR, (ptrtofnct)error), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_TEST(register_callback, (conn1.conn, FC_NEEDPASS, (ptrtofnct)needpass), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_TEST(register_callback, (conn1.conn, FC_CONNECTED, (ptrtofnct)connected), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_TEST(register_callback, (conn1.conn, FC_CONNECTFAILED, (ptrtofnct)connectfailed), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_TEST(signon, (conn1.conn, NULL, 0, conn1.username), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_WAITFOR(conn1, WF_CONNECTED, NULL, servers[i].failed); T

		DO_TEST(subcode_register_reply_callback, (conn1.conn, "PING", subcode_reply), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_TEST(subcode_send_request, (conn1.conn, botusername, "PING", FIRETALK_TEST_STRING), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_WAITFOR(conn1, WF_SUBCODE_REPLY, FIRETALK_TEST_STRING, servers[i].failed); T

		DO_TEST(register_callback, (conn1.conn, FC_IM_GOTINFO, (ptrtofnct)gotinfo), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_TEST(im_get_info, (conn1.conn, botusername), ret != FE_SUCCESS, ret, servers[i].failed); T
		DO_WAITFOR(conn1, WF_GOTINFO, botusername, servers[i].failed); T

		out:
		DO_TEST(disconnect, (conn1.conn), ret != FE_SUCCESS, ret, servers[i].failed);

		if (servers[i].failed)
		{
			fprintf(stderr, "SERVER FAILED (sorry)\r\n");
		}
		fprintf(stderr, "destroying conn1.conn...");
		fflush(stderr);
		firetalk_destroy_conn(conn1.conn);
		fprintf(stderr, " done\r\n");

		free(conn1.username);
		conn1.username = NULL;
		free(conn1.password);
		conn1.password = NULL;

		fprintf(stderr, "\r\n");

		uid++;
	}
	printf("Results:\n");
	for (i = 0; servers[i].hostname; i++)
		printf("%s\t%d\t%d\n", servers[i].hostname, servers[i].port, servers[i].failed);
	return(0);
}
