/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2003 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>
#include <sys/stat.h>

#include "naim-int.h"

extern conn_t	*curconn;
extern time_t	now;
extern faimconf_t	faimconf;
time_t	awaytime;

void	logim(conn_t *conn, const char *source, const char *target,
	const unsigned char *msg) {
	struct tm	*tmptr = NULL;

	if (target == NULL)
		target = conn->sn;
	if ((source == NULL) || (target == NULL))
		return;
	if ((awaytime > 0) && (*target != ':')) {
		int	awaylog = getvar_int(conn, "awaylog"),
			private = (firetalk_compare_nicks(conn->conn, conn->sn, target) == FE_SUCCESS);

		if ((awaylog > 1) || ((awaylog == 1) && private)) {
			char	srcbuf[1024];
			int	srccol;

			srccol = naim_strtocol(source);

			if (private)
				snprintf(srcbuf, sizeof(srcbuf), "<font color=\"#%06X\">%s</font>", srccol, source);
			else
				snprintf(srcbuf, sizeof(srcbuf), "<font color=\"#%06X\">%s:%s</font>", srccol, source, target);
			naim_awaylog(conn, srcbuf, msg);
		}
	}

	if (getvar_int(conn, "log") == 0) {
		if (conn->logfile != NULL)
			fclose(conn->logfile);
		return;
	}

	tmptr = localtime(&now);
	assert(tmptr != NULL);

	if (conn->logfile == NULL) {
		char	*f;

		if ((f = getvar(conn, "logfile")) == NULL) {
			echof(conn, "LOGIM", "$%s:log set to 1, but $%s:logfile not set.\n",
				conn->winname, conn->winname);
			return;
		}
		if ((conn->logfile = fopen(f, "a")) == NULL) {
			echof(conn, "LOGIM", "Unable to open \"%s\": %m", f);
			return;
		}
		fchmod(fileno(conn->logfile), 0600);
		fprintf(conn->logfile, "\n*** Log opened %04i-%02i-%02iT"
			"%02i:%02i.<br>\n", 1900+tmptr->tm_year, tmptr->tm_mon+1,
			tmptr->tm_mday, tmptr->tm_hour, tmptr->tm_min);
	}
	assert(source != NULL);
	assert(*source != 0);
	assert(target != NULL);
	assert(*target != 0);
	assert(msg != NULL);
	fprintf(conn->logfile, "%04i-%02i-%02iT%02i:%02i %s -&gt; %s | %s<br>\n",
		1900+tmptr->tm_year, tmptr->tm_mon+1, tmptr->tm_mday,
		tmptr->tm_hour, tmptr->tm_min, source, target, msg);
	fflush(conn->logfile);
}

void	naim_send_message(conn_t *conn, const char *SN, const unsigned char *msg, int chat, int autom) {
	buddylist_t	*blist;
	unsigned char	buf[4*1024];
	int	ret;

	if ((blist = rgetlist(conn, SN)) != NULL) {
		if (blist->crypt != NULL) {
			int	i, j = 0, sendhex = 0;

			for (i = 0; (msg[i] != 0) && (i < sizeof(buf)-1); i++) {
				buf[i] = msg[i] ^ blist->crypt[j++];
				if (((i == 0) && isspace(buf[i]))
					|| ((sendhex == 0) && (firetalk_isprint(conn->conn, buf[i]) != FE_SUCCESS)))
					sendhex = 1;
				if (blist->crypt[j] == 0)
					j = 0;
			}
			buf[i] = 0;
			if (sendhex) {
				char	buf2[sizeof(buf)*2];

				for (j = 0; j < i; j++)
					sprintf(buf2+j*2, "%02X", buf[j]);
				buf[j] = 0;
				if (autom == 0)
					firetalk_subcode_send_request(conn->conn, SN, "HEXTEXT", buf2);
				else
					firetalk_subcode_send_reply(conn->conn, SN, "HEXTEXT", buf2);
				return;
			}
			msg = buf;
		}
		if (getvar_int(conn, "autopeer") != 0) {
			if (blist->peer == 0) {
				blist->peer = -1;
				firetalk_subcode_send_request(conn->conn, SN, "AUTOPEER", "+AUTOPEER:3");
			}
		}
	}

	if (chat == 0)
		ret = firetalk_im_send_message(conn->conn, SN, msg, autom);
	else
		ret = firetalk_chat_send_message(conn->conn, SN, msg, autom);
	if (ret != FE_SUCCESS) {
		buddywin_t	*bwin = bgetanywin(conn, SN);

		if (bwin == NULL)
			echof(conn, NULL, "Unable to send message to %s: %s.\n", SN, firetalk_strerror(ret));
		else {
			window_echof(bwin, "Unable to send message: %s.\n", firetalk_strerror(ret));
			if (ret == FE_PACKETSIZE)
				window_echof(bwin, "Try shortening your message or splitting it into multiple messages and resending.\n");
		}
	}
}

void	naim_send_im(conn_t *conn, const char *SN, const char *msg, const int _auto) {
	buddywin_t	*bwin = bgetanywin(conn, SN);
	unsigned char	buf[2048];
	const char	*pre = getvar(conn, "im_prefix"),
			*post = getvar(conn, "im_suffix");

	if ((conn->online > 0)			// if you are online
		&& (	   (bwin == NULL)	// and it's someone random
			|| (bwin->et == CHAT)	//  or it's a chat
			|| ((bwin->et == BUDDY) && (bwin->e.buddy->offline == 0))
						//  or it's an online buddy
		)
	) {
		if (_auto == 0)
			updateidletime();
		if ((pre != NULL) || (post != NULL)) {
			snprintf(buf, sizeof(buf), "%s%s%s", pre?pre:"", msg, post?post:"");
			msg = buf;
		}
		naim_send_message(conn, SN, msg, ((bwin != NULL) && (bwin->et == CHAT)), 0);
	} else if (bwin != NULL) {
		struct tm	*tmptr = NULL;

		tmptr = localtime(&now);
		assert(tmptr != NULL);
		if (strncmp(msg, "[Queued ", sizeof("[Queued ")-1) != 0) {
			snprintf(buf, sizeof(buf), "[Queued %04i-%02i-%02iT%02i:%02i] %s",
				1900+tmptr->tm_year, 1+tmptr->tm_mon, tmptr->tm_mday, 
				tmptr->tm_hour, tmptr->tm_min, msg);
			msg = buf;
		}
		bwin->pouncec++;
		bwin->pouncear = realloc(bwin->pouncear,
			bwin->pouncec*sizeof(*(bwin->pouncear)));
		bwin->pouncear[bwin->pouncec-1] = strdup(msg);
		if (bwin->pouncec == 1)
			window_echof(bwin, "Message queued. Type /close to dequeue pending messages.\n");
	}
}

void	naim_send_im_away(conn_t *conn, const char *SN, const char *msg) {
	struct tm	*tmptr;
	buddywin_t	*bwin;
	static time_t	lastauto = 0;
	const char	*pre,
			*post;

	if (lastauto < now-1)
		lastauto = now;
	else {
		echof(conn, "SEND-IM-AWAY", "Suppressing away message to %s (%s).\n", SN, msg);
		return;
	}

	pre = getvar(conn, "im_prefix"),
	post = getvar(conn, "im_suffix");
	if ((pre != NULL) || (post != NULL)) {
		static unsigned char buf[2048];

		snprintf(buf, sizeof(buf), "%s%s%s", pre?pre:"", msg, post?post:"");
		msg = buf;
	}

	tmptr = localtime(&now);
	assert(tmptr != NULL);
	if ((bwin = bgetwin(conn, SN, BUDDY)) == NULL)
		echof(conn, NULL, "Sent away IM to %s (%s)\n", SN, msg);
	else {
		WINTIME(&(bwin->nwin), IMWIN);
		hwprintf(&(bwin->nwin), C(IMWIN,SELF),
			"-<B>%s</B>-", conn->sn);
		hwprintf(&(bwin->nwin), C(IMWIN,TEXT),
			" %s<br>", msg);
	}
	naim_send_message(conn, SN, msg, 0, 1);
}

void	naim_send_act(conn_t *conn, const char *SN, const char *msg) {
	buddywin_t	*bwin = bgetanywin(conn, SN);

	updateidletime();
	if ((bwin == NULL) || (bwin->et != CHAT))
		firetalk_im_send_action(conn->conn, SN, msg, 0);
	else
		firetalk_chat_send_action(conn->conn, SN, msg, 0);
}

void	setaway(const int auto_flag) {
	conn_t	*conn = curconn;
	char	*awaymsg = secs_getvar("awaymsg");

	awaytime = now - 60*secs_getvar_int("idletime");
	do {
		status_echof(conn, "You are now away--hurry back!\n");
		firetalk_set_away(conn->conn, awaymsg, auto_flag);
		if (conn->online > 0)
			naim_set_info(conn->conn, conn->profile);
	} while ((conn = conn->next) != curconn);
}

void	unsetaway(void) {
	conn_t	*conn = curconn;

	awaytime = 0;
	updateidletime();
	do {
		status_echof(conn, "You are no longer away--welcome back =)\n");
		firetalk_set_away(conn->conn, NULL, 0);
		if (conn->online > 0)
			naim_set_info(conn->conn, conn->profile);
	} while ((conn = conn->next) != curconn);
}

void	sendaway(conn_t *conn, const char *SN) {
	char	buf[1124];

	if (awaytime == 0)
		return;
	snprintf(buf, sizeof(buf), "[Away for %s] %s", 
		dtime(now-awaytime), secs_getvar("awaymsg"));
	naim_send_im_away(conn, SN, buf);
}
