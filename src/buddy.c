/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2005 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>

#include "naim-int.h"

extern win_t	win_info, win_buddy;
extern conn_t	*curconn;
extern faimconf_t faimconf;
extern time_t	now, awaytime;
extern double	nowf, changetime;
extern char	*lastclose;
extern int	printtitle;

extern int buddyc G_GNUC_INTERNAL,
	wbuddy_widthy G_GNUC_INTERNAL;
int	buddyc = -1,
	wbuddy_widthy = -1;

static void iupdate(void) {
	time_t	t;
	long	idletime = script_getvar_int("idletime");
	struct tm *tmptr;
	char	buf[1024];

	assert(sizeof(buf) > faimconf.winfo.widthx);

	nw_erase(&win_info);

	tmptr = localtime(&now);
	assert(tmptr != NULL);

	if (curconn->online > 0) {
		t = now - curconn->online;
		script_setvar("online", dtime(t));
	} else
		script_setvar("online", "(not connected)");

	script_setvar("SN", curconn->sn);
	script_setvar("conn", curconn->winname);

	if (inconn) {
		script_setvar("cur", curconn->curbwin->winname);
		if ((curconn->curbwin->et == BUDDY) && (curconn->curbwin->e.buddy->tag != NULL)) {
			snprintf(buf, sizeof(buf), " !%.*s!",
				(int)(sizeof(buf)-4),
				curconn->curbwin->e.buddy->tag);
			htmlstrip(buf);
			script_setvar("iftopic", buf);
		} else if (curconn->curbwin->blurb != NULL) {
			snprintf(buf, sizeof(buf), " (%.*s)",
				(int)(sizeof(buf)-4),
				curconn->curbwin->blurb);
			htmlstrip(buf);
			script_setvar("iftopic", buf);
		} else if (curconn->curbwin->status != NULL) {
			snprintf(buf, sizeof(buf), " (%.*s)",
				(int)(sizeof(buf)-4),
				curconn->curbwin->status);
			htmlstrip(buf);
			script_setvar("iftopic", buf);
		} else
			script_setvar("iftopic", "");

		if ((curconn->curbwin->et != BUDDY) || (curconn->curbwin->e.buddy->typing < now-10*60))
			script_setvar("iftyping", "");
		else
			script_setvar("iftyping", script_expand(getvar(curconn, "statusbar_typing")));

		switch (curconn->curbwin->et) {
		  case BUDDY:
			if (curconn->curbwin->e.buddy->docrypt)
				script_setvar("ifcrypt", getvar(curconn, "statusbar_crypt"));
			else
				script_setvar("ifcrypt", "");
			if (curconn->curbwin->e.buddy->tzname != NULL) {
				script_setvar("tzname", curconn->curbwin->e.buddy->tzname);
				script_setvar("iftzname", script_expand(getvar(curconn, "statusbar_tzname")));
			} else {
				script_setvar("tzname", "");
				script_setvar("iftzname", "");
			}
			script_setvar("ifoper", "");
			script_setvar("ifquery",
				script_expand(getvar(curconn, "statusbar_query")));
			script_setvar("ifchat", "");
			break;
		  case CHAT:
			if (curconn->curbwin->e.chat->isoper)
				script_setvar("ifoper",
					script_expand(getvar(curconn, "statusbar_oper")));
			else
				script_setvar("ifoper", "");
			script_setvar("ifquery", "");
			script_setvar("ifchat",
				script_expand(getvar(curconn, "statusbar_chat")));
			break;
		  case TRANSFER:
			script_setvar("ifoper", "");
			script_setvar("ifquery", "");
			script_setvar("ifchat", "");
			break;
		}
	} else {
		script_setvar("cur", "");
		script_setvar("ifoper", "");
		script_setvar("ifquery", "");
		script_setvar("iftyping", "");
		script_setvar("ifchat", "");
	}

	script_setvar("iftransfer", "");

	if (awaytime > 0)
		script_setvar("ifaway",
			script_expand(getvar(curconn, "statusbar_away")));
	else
		script_setvar("ifaway", "");

	if (idletime > 10) {
		script_setvar("idle", dtime(60*idletime));
		script_setvar("ifidle",
			script_expand(getvar(curconn, "statusbar_idle")));
	} else {
		script_setvar("idle", "");
		script_setvar("ifidle", "");
	}

	if (curconn->lag > 0.1) {
		script_setvar("lag", dtime(curconn->lag));
		script_setvar("iflag",
			script_expand(getvar(curconn, "statusbar_lag")));
	} else {
		script_setvar("lag", "0");
		script_setvar("iflag", "");
	}

	if (curconn->warnval > 0) {
		snprintf(buf, sizeof(buf), "%li", curconn->warnval);
		script_setvar("warnval", buf);
		script_setvar("ifwarn",
			script_expand(getvar(curconn, "statusbar_warn")));
	} else {
		script_setvar("warnval", "0");
		script_setvar("ifwarn", "");
	}

	if (strftime(buf, sizeof(buf), getvar(curconn, "statusbar"), tmptr) > 0) {
		char	*left, *right;

		assert(*buf != 0);
		right = script_expand(buf);
		assert(right != NULL);
		left = strdup(right);
		assert(left != NULL);
		if ((right = strrchr(left, '+')) != NULL) {
			char	*ell;
			int	leftlen, rightlen;

			*right = 0;
			right++;
			leftlen = strlen(left);
			rightlen = strlen(right);
			if (leftlen > (faimconf.winfo.widthx-rightlen)) {
				ell = "...";
				leftlen = faimconf.winfo.widthx-rightlen-3;
			} else {
				ell = "";
				leftlen = faimconf.winfo.widthx-rightlen;
			}
			if (leftlen < 0)
				leftlen = 0;
			snprintf(buf, faimconf.winfo.widthx+1, "%-*.*s%s%s",
				leftlen, leftlen, left, ell, right);
		} else
			snprintf(buf, faimconf.winfo.widthx+1, "%s", left);
		nw_printf(&win_info, CB(STATUSBAR,INPUT), 0, "%-*s", faimconf.winfo.widthx,
			buf);
		free(left);
		left = NULL;
	} else
		status_echof(curconn, "Error in strftime(): %s.\n", strerror(errno));
}

static conn_t *bsort_conn = NULL;

static int bsort_alpha_winname(const void *p1, const void *p2) {
	register buddywin_t
		**bw1 = (buddywin_t **)p1,
		**bw2 = (buddywin_t **)p2;
	char	b1[256], b2[256];
	register const char
		*s1a, *s2a,
		*s1b = (*bw1)->winname,
		*s2b = (*bw2)->winname;
	int	ret;

	assert(bsort_conn != NULL);
	if ((*bw1)->et == BUDDY)
		s1a = user_name(b1, sizeof(b1), bsort_conn, (*bw1)->e.buddy);
	else
		s1a = (*bw1)->winname;
	if ((*bw2)->et == BUDDY)
		s2a = user_name(b2, sizeof(b2), bsort_conn, (*bw2)->e.buddy);
	else
		s2a = (*bw2)->winname;

	ret = strcasecmp(s1a, s2a);
	if (ret != 0)
		return(ret);
	ret = strcasecmp(s1b, s2b);
	return(ret);
}

static int bsort_alpha_group(const void *p1, const void *p2) {
	register buddywin_t
		**b1 = (buddywin_t **)p1,
		**b2 = (buddywin_t **)p2;
	register int
		ret;

	if (((*b1)->et == BUDDY) || ((*b2)->et == BUDDY)) {
		if ((*b1)->et != BUDDY)
			return(1);
		else if ((*b2)->et != BUDDY)
			return(-1);
		ret = strcasecmp(USER_GROUP((*b1)->e.buddy), USER_GROUP((*b2)->e.buddy));
		if (ret != 0)
			return(ret);
	}
	return(bsort_alpha_winname(p1, p2));
}

static void bsort(conn_t *conn) {
	buddywin_t	**ar = NULL,
			*bwin = conn->curbwin;
	int		i, c = 0,
			(*comparator)(const void *, const void *);

	switch (getvar_int(conn, "autosort")) {
	  case 0:
		return;
	  case 1:
		comparator = bsort_alpha_winname;
		break;
	  case 2:
		comparator = bsort_alpha_group;
		break;
	  default:
		abort();
	}

	do {
		ar = realloc(ar, (c+1)*sizeof(*ar));
		if (ar == NULL)
			abort();
		ar[c] = bwin;
		c++;
	} while ((bwin = bwin->next) != conn->curbwin);

	bsort_conn = conn;
	qsort(ar, c, sizeof(*ar), comparator);
	bsort_conn = NULL;

	for (i = 0; i < c; i++)
		ar[i]->next = ar[(i+1)%c];
	free(ar);
}

void	bupdate(void) {
	static win_t *lwin = NULL;
	int	waiting,
		widthx = script_getvar_int("winlistchars"),
		M = widthx,
#ifdef ENABLE_FORCEASCII
		fascii = script_getvar_int("forceascii"),
#endif
		bb, line = 0;
	conn_t	*conn = curconn;

	wbuddy_widthy = script_getvar_int("winlistheight")*faimconf.wstatus.widthy/100;
	if (wbuddy_widthy > faimconf.wstatus.widthy)
		wbuddy_widthy = faimconf.wstatus.widthy;

#ifdef ENABLE_FORCEASCII
# define LINEDRAW(ch,as)	((fascii == 1) ? as : ch)
#else
# define LINEDRAW(ch,as)	ch
#endif
#define ACS_ULCORNER_C	LINEDRAW(ACS_ULCORNER,',')
#define ACS_LLCORNER_C	LINEDRAW(ACS_LLCORNER,'`')
#define ACS_URCORNER_C	LINEDRAW(ACS_URCORNER,'.')
#define ACS_LRCORNER_C	LINEDRAW(ACS_LRCORNER,'\'')
#define ACS_LTEE_C	LINEDRAW(ACS_LTEE,'+')
#define ACS_RTEE_C	LINEDRAW(ACS_RTEE,'+')
#define ACS_BTEE_C	LINEDRAW(ACS_BTEE,'+')
#define ACS_TTEE_C	LINEDRAW(ACS_TTEE,'+')
#define ACS_HLINE_C	LINEDRAW(ACS_HLINE,'-')
#define ACS_VLINE_C	LINEDRAW(ACS_VLINE,'|')
#define ACS_PLUS_C	LINEDRAW(ACS_PLUS,'+')
#define ACS_RARROW_C	LINEDRAW(ACS_RARROW,'>')

	nw_erase(&win_buddy);
	nw_vline(&win_buddy, ACS_VLINE_C | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS), wbuddy_widthy);
	bb = buddyc;
	buddyc = 0;

	if (conn == NULL)
		return;

	waiting = 0;

	if (inconn) {
		int	autoclose = getvar_int(curconn, "autoclose");

		assert(curconn->curbwin != NULL);
		if ((autoclose > 0) && (curconn->curbwin->et == BUDDY) && !USER_PERMANENT(curconn->curbwin->e.buddy) && (curconn->curbwin->waiting != 0))
			curconn->curbwin->closetime = now + 60*autoclose;
		curconn->curbwin->waiting = 0;
		if ((curconn->curbwin->et == CHAT) && curconn->curbwin->e.chat->isaddressed)
			curconn->curbwin->e.chat->isaddressed = 0;
	}

	do {
		buddywin_t *bwin = conn->curbwin;
		char	*lastgroup = NULL;
		int	hidegroup = 0,
			autosort = getvar_int(conn, "autosort");

		assert(conn->winname != NULL);

		if (line < wbuddy_widthy) {
			nw_move(&win_buddy, line, 0);
			if (line == 0)
				nw_addch(&win_buddy, ACS_ULCORNER_C | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS));
			else
				nw_addch(&win_buddy, ACS_LTEE_C | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS));
			nw_hline(&win_buddy, ACS_HLINE_C | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS), widthx);

			nw_move(&win_buddy, line++, widthx-strlen(conn->winname));
			if (conn->online <= 0) {
				nw_printf(&win_buddy, C(WINLIST,BUDDY_OFFLINE), 1, " %s", conn->winname);
				if (line < wbuddy_widthy) {
					nw_move(&win_buddy, line++, 1);
					nw_printf(&win_buddy, C(WINLIST,TEXT), 1, "%s", " You are offline");
					buddyc++;
				}
			} else
				nw_printf(&win_buddy, C(WINLIST,TEXT), 1, " %s", conn->winname);
		}

		buddyc++;

		if (bwin == NULL)
			continue;
		if (autosort == 2) {
			if (bwin->et == BUDDY)
				STRREPLACE(lastgroup, USER_GROUP(bwin->e.buddy));
			else
				STRREPLACE(lastgroup, CHAT_GROUP);
		}

		bsort(conn);

		do {
			if ((inconn && (conn == curconn)) || bwin->waiting) {
				char	buf[256], *group;
				int	back = -1, fore = -1;

				assert(bwin->winname != NULL);
				buddyc++;

				if (bwin->et == BUDDY) {
					user_name(buf, sizeof(buf), conn, bwin->e.buddy);
					group = USER_GROUP(bwin->e.buddy);
				} else {
					snprintf(buf, sizeof(buf), "%s", bwin->winname);
					group = CHAT_GROUP;
				}

				if (autosort == 2) {
					if (strcmp(lastgroup, group) != 0) {
						if (line < wbuddy_widthy) {
							nw_move(&win_buddy, line++, widthx-strlen(group)-1);
							nw_printf(&win_buddy, C(WINLIST,TEXT), hidegroup?0:1, "%c%s%c",
								hidegroup?'<':'[', group, hidegroup?'>':']');
							buddyc++;
						}
						STRREPLACE(lastgroup, group);
					}
				}

				if (bwin->waiting && !waiting) {
					char	tmp[64];

					if (conn == curconn)
						snprintf(tmp, sizeof(tmp),
							" [Ctrl-N to %s]", buf);
					else
						snprintf(tmp, sizeof(tmp),
							" [Ctrl-N to %s:%s]", conn->winname, buf);
					script_setvar("ifpending", tmp);
					waiting = 1;
				}
				if (printtitle && bwin->waiting && (waiting < 2) && ((bwin->et == BUDDY) || ((bwin->et == CHAT) && bwin->e.chat->isaddressed))) {
					nw_titlef("[%s:%s]", conn->winname, buf);
					waiting = 2;
				}

				if (line >= wbuddy_widthy)
					continue;

				if (strlen(buf) > M) {
					buf[M-1] = '>';
					buf[M] = 0;
				}

				back = faimconf.b[cWINLIST];
				switch (bwin->et) {
				  case BUDDY:
					assert(bwin->e.buddy != NULL);
					if (bwin->e.buddy->typing >= now-10*60)
						back = faimconf.f[cBUDDY_TYPING];
					else if (bwin->e.buddy->tag != NULL)
						back = faimconf.f[cBUDDY_TAGGED];

					if (bwin->waiting)
						fore = faimconf.f[cBUDDY_ADDRESSED];
					else if (bwin->pouncec > 0)
						fore = faimconf.f[cBUDDY_QUEUED];
					else if (bwin->e.buddy->offline)
						fore = faimconf.f[cBUDDY_OFFLINE];
					else if (bwin->e.buddy->ismobile)
						fore = faimconf.f[cBUDDY_MOBILE];
					else if (bwin->e.buddy->isaway && bwin->e.buddy->isidle)
						fore = faimconf.f[cBUDDY_AWAY];
					else if (bwin->e.buddy->isaway)
						fore = faimconf.f[cBUDDY_FAKEAWAY];
					else if (bwin->e.buddy->isidle)
						fore = faimconf.f[cBUDDY_IDLE];
					else
						fore = faimconf.f[cBUDDY];
					break;
				  case CHAT:
					assert(bwin->e.chat != NULL);
					if (bwin->e.chat->isaddressed) {
						assert(bwin->waiting);
						fore = faimconf.f[cBUDDY_ADDRESSED];
					} else if (bwin->waiting)
						fore = faimconf.f[cBUDDY_WAITING];
					else if (bwin->e.chat->offline)
						fore = faimconf.f[cBUDDY_OFFLINE];
					else
						fore = faimconf.f[cBUDDY];
					break;
				  case TRANSFER:
					if (bwin->waiting)
						fore = faimconf.f[cBUDDY_WAITING];
					else
						fore = faimconf.f[cBUDDY];
					break;
				}

				if (bwin == curconn->curbwin)
					back = faimconf.b[cWINLISTHIGHLIGHT];

				assert(fore != -1);
				assert(back != -1);

				{
					int	col = nw_COLORS*back + fore;

					nw_move(&win_buddy, line, 1);
					if ((col >= 2*COLOR_PAIRS) || (col < COLOR_PAIRS))
						nw_printf(&win_buddy, col%COLOR_PAIRS, 1, "%*s", M, buf);
					else
						nw_printf(&win_buddy, col%COLOR_PAIRS, 0, "%*s", M, buf);
					if (bwin->waiting) {
						nw_move(&win_buddy, line, 0);
						nw_addch(&win_buddy, ACS_LTEE_C   | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS));
						nw_addch(&win_buddy, ACS_RARROW_C | A_BOLD | COLOR_PAIR(col%COLOR_PAIRS));
					}
				}
				line++;
			}
			assert(buddyc < 1000);
		} while ((bwin = bwin->next) != conn->curbwin);

		if (autosort == 2) {
			free(lastgroup);
			lastgroup = NULL;
		} else
			assert(lastgroup == NULL);
	} while ((conn = conn->next) != curconn);

	nw_move(&win_buddy, line-1, 0);
	if (line != 1)
		nw_addch(&win_buddy, ACS_LLCORNER_C | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS));
	else
		nw_addch(&win_buddy, ACS_HLINE_C | A_BOLD | COLOR_PAIR(C(WINLIST,TEXT)%COLOR_PAIRS));

	if (printtitle && (waiting != 2))
		nw_titlef("");
	if (waiting)
		buddyc = -buddyc;
	else
		script_setvar("ifpending", "");

	if (inconn)
		assert(curconn->curbwin != NULL);

	if ((buddyc != bb)
		||  (inconn && (&(curconn->curbwin->nwin) != lwin))
		|| (!inconn && (&(curconn->nwin) != lwin))) {
		if (inconn)
			lwin = &(curconn->curbwin->nwin);
		else
			lwin = &(curconn->nwin);
		bb = buddyc - bb;
		buddyc = buddyc - bb;
		bb = buddyc + bb;
		naim_changetime();
		buddyc = bb;
	}

	iupdate();
}

buddywin_t *bgetwin(conn_t *conn, const char *buddy, et_t et) {
	buddywin_t *bwin = conn->curbwin;

	assert(buddy != NULL);
	if (bwin == NULL)
		return(NULL);
	do {
		if ((bwin->et == et) && (firetalk_compare_nicks(conn->conn, buddy, bwin->winname) == FE_SUCCESS))
			return(bwin);
	} while ((bwin = bwin->next) != conn->curbwin);

	return(NULL);
}

buddywin_t *bgetanywin(conn_t *conn, const char *buddy) {
	buddywin_t *bwin = conn->curbwin;

	assert(buddy != NULL);
	if (bwin == NULL)
		return(NULL);
	do {
		if (firetalk_compare_nicks(conn->conn, buddy, bwin->winname) == FE_SUCCESS)
			return(bwin);
		if ((bwin->et == BUDDY) && (firetalk_compare_nicks(conn->conn, buddy, USER_NAME(bwin->e.buddy)) == FE_SUCCESS))
			return(bwin);
	} while ((bwin = bwin->next) != conn->curbwin);

	return(NULL);
}

buddywin_t *bgetbuddywin(conn_t *conn, const buddylist_t *blist) {
	buddywin_t *bwin = conn->curbwin;

	assert(blist != NULL);
	if (bwin == NULL)
		return(NULL);
	do {
		if ((bwin->et == BUDDY) && (bwin->e.buddy == blist))
			return(bwin);
	} while ((bwin = bwin->next) != conn->curbwin);

	return(NULL);
}

static void bremove(buddywin_t *bwin) {
	int	i;

	assert(bwin != NULL);
	nw_delwin(&(bwin->nwin));
	for (i = 0; i < bwin->pouncec; i++) {
		free(bwin->pouncear[i]);
		bwin->pouncear[i] = NULL;
	}
	free(bwin->pouncear);
	bwin->pouncear = NULL;
	FREESTR(bwin->winname);
	FREESTR(bwin->blurb);
	FREESTR(bwin->status);

	if (bwin->nwin.logfile != NULL) {
		struct tm *tmptr;

		tmptr = localtime(&now);
		fprintf(bwin->nwin.logfile, "<I>-----</I> <font color=\"#FFFFFF\">Log file closed %04i-%02i-%02iT%02i:%02i</font> <I>-----</I><br>\n",
			1900+tmptr->tm_year, 1+tmptr->tm_mon, tmptr->tm_mday, tmptr->tm_hour, tmptr->tm_min);
		fclose(bwin->nwin.logfile);
		bwin->nwin.logfile = NULL;
	}

	free(bwin);
}

void	verify_winlist_sanity(conn_t *const conn, const buddywin_t *const verifywin) {
	buddywin_t *bwin;
	int	i = 0, found = 0;

	assert(conn != NULL);
	assert(conn->curbwin != NULL);
	bwin = conn->curbwin;
	do {
		if (bwin == verifywin)
			found = 1;
		assert((bwin->et == CHAT) || (bwin->et == BUDDY) || (bwin->et == TRANSFER));
		if (bwin->et != BUDDY) {
			assert(bwin->informed == 0);
			assert(bwin->closetime == 0);
		}
		if (bwin->et == CHAT)
			assert(bwin->keepafterso == 1);
		assert(strlen(bwin->winname) > 0);
		assert(i++ < 10000);
	} while ((bwin = bwin->next) != conn->curbwin);
	if (verifywin != NULL)
		assert(found == 1);
	else
		assert(found == 0);
}

void	bclose(conn_t *conn, buddywin_t *bwin, int _auto) {
	if (bwin == NULL)
		return;

	assert(bwin->conn == conn);

	verify_winlist_sanity(conn, bwin);

	script_hook_delwin(bwin);

	switch (bwin->et) {
	  case BUDDY:
		if (_auto == 0) {
			assert(bwin->e.buddy != NULL);
			status_echof(conn, "Type /delbuddy to remove %s from your buddy list.\n",
				user_name(NULL, 0, conn, bwin->e.buddy));
			STRREPLACE(lastclose, bwin->winname);
		}
		break;
	  case CHAT:
		if (bwin->winname[0] != ':')
			firetalk_chat_part(conn->conn, bwin->winname);
		free(bwin->e.chat->key);
		bwin->e.chat->key = NULL;
		free(bwin->e.chat->last.line);
		bwin->e.chat->last.line = NULL;
		free(bwin->e.chat->last.name);
		bwin->e.chat->last.name = NULL;
		free(bwin->e.chat);
		bwin->e.chat = NULL;
		break;
	  case TRANSFER:
		assert(bwin->e.transfer != NULL);
		firetalk_file_cancel(conn->conn, bwin->e.transfer->handle);
		echof(conn, NULL, "File transfer aborted.\n");
		fremove(bwin->e.transfer);
		bwin->e.transfer = NULL;
		break;
	}

	if (bwin == bwin->next)
		conn->curbwin = NULL;
	else {
		int	i = 0;
		buddywin_t *bbefore;

		bbefore = bwin->next;
		while (bbefore->next != bwin) {
			bbefore = bbefore->next;
			assert(i++ < 10000);
		}
		bbefore->next = bwin->next;

		if (bwin == conn->curbwin)
			conn->curbwin = bwin->next;
	}

	if (conn->curbwin != NULL)
		verify_winlist_sanity(conn, NULL);

	bremove(bwin);
	bwin = NULL;
	bupdate();

	if (conn->curbwin != NULL)
		verify_winlist_sanity(conn, NULL);

	if (conn->curbwin != NULL)
		nw_touchwin(&(conn->curbwin->nwin));
	else
		nw_touchwin(&(conn->nwin));
}

void	bnewwin(conn_t *conn, const char *name, et_t et) {
	buddywin_t *bwin;
	int	i;
	struct tm *tmptr;

	assert(name != NULL);

	if (bgetwin(conn, name, et) != NULL)
		return;

	tmptr = localtime(&now);

	bwin = calloc(1, sizeof(buddywin_t));
	assert(bwin != NULL);

	bwin->winname = strdup(name);
	assert(bwin->winname != NULL);

	bwin->keepafterso = bwin->waiting = 0;
	bwin->et = et;
	switch (et) {
	  case BUDDY:
		bwin->e.buddy = rgetlist(conn, name);
		assert(bwin->e.buddy != NULL);
		break;
	  case CHAT:
		bwin->e.chat = calloc(1, sizeof(*(bwin->e.chat)));
		assert(bwin->e.chat != NULL);
		bwin->e.chat->offline = 1;
		break;
	  case TRANSFER:
		break;
	}

	if (conn->curbwin == NULL) {
		bwin->next = bwin;
		conn->curbwin = bwin;
	} else {
		buddywin_t *lastbwin = conn->curbwin,
			*srchbwin = conn->curbwin;

		do {
			if (aimcmp(lastbwin->winname, lastbwin->next->winname) == 1)
				break;
		} while ((lastbwin = lastbwin->next) != srchbwin);

		srchbwin = lastbwin;

		do {
			if (aimcmp(srchbwin->next->winname, bwin->winname) == 1)
				break;
		} while ((srchbwin = srchbwin->next) != lastbwin);

		bwin->next = srchbwin->next;
		srchbwin->next = bwin;
	}

	if ((bwin->nwin.logfile = logging_open(conn, bwin)) == NULL) {
		nw_newwin(&(bwin->nwin), faimconf.wstatus.pady, faimconf.wstatus.widthx);
		nw_initwin(&(bwin->nwin), cIMWIN);
		for (i = 0; i < faimconf.wstatus.pady; i++)
			nw_printf(&(bwin->nwin), 0, 0, "\n");
		hwprintf(&(bwin->nwin), C(IMWIN,TEXT), "<I>-----</I> <font color=\"#FFFFFF\">Session started %04i-%02i-%02iT%02i:%02i</font> <I>-----</I><br>\n",
			1900+tmptr->tm_year, 1+tmptr->tm_mon, tmptr->tm_mday, tmptr->tm_hour, tmptr->tm_min);
	} else {
		fchmod(fileno(bwin->nwin.logfile), 0600);
		fprintf(bwin->nwin.logfile, "&nbsp;<br>\n<I>-----</I> <font color=\"#FFFFFF\">Log file opened %04i-%02i-%02iT%02i:%02i</font> <I>-----</I><br>\n",
			1900+tmptr->tm_year, 1+tmptr->tm_mon, tmptr->tm_mday, tmptr->tm_hour, tmptr->tm_min);
		if (firetalk_compare_nicks(conn->conn, name, "naim help") == FE_SUCCESS) {
			fprintf(bwin->nwin.logfile, "<I>*****</I> <font color=\"#808080\">Once you have signed on, anything you type that does not start with a slash is sent as a private message to whoever's window you are in.</font><br>\n");
			fprintf(bwin->nwin.logfile, "<I>*****</I> <font color=\"#808080\">Right now you are \"in\" a window for <font color=\"#00FF00\">naim help</font>, which is the screen name of naim's maintainer, <font color=\"#00FFFF\">Daniel Reed</font>.</font><br>\n");
			fprintf(bwin->nwin.logfile, "<I>*****</I> <font color=\"#808080\">If you would like help, first try using naim's online help by typing <font color=\"#00FF00\">/help</font>. If you need further help, visit the naim documentation site online at http://naimdoc.net/. If all else fails, feel free to ask your question here and wait patiently :).</font><br>\n");
			fprintf(bwin->nwin.logfile, "<I>*****</I> <font color=\"#800000\">If you are using Windows telnet to connect to a shell account to run naim, you may notice severe screen corruption. You may wish to try PuTTy, available for free from www.tucows.com. PuTTy handles both telnet and SSH.</font><br>\n");
		}
		nw_newwin(&(bwin->nwin), 1, 1);
		nw_initwin(&(bwin->nwin), cIMWIN);
		bwin->nwin.dirty = 1;
		bwin->nwin.logfilelines = 0;
	}

	bwin->conn = conn;
	script_hook_newwin(bwin);
}

void	bcoming(conn_t *conn, const char *buddy) {
	buddywin_t *bwin = NULL;
	buddylist_t *blist = NULL;

	assert(buddy != NULL);

	blist = rgetlist(conn, buddy);
	assert(blist != NULL);
	if (strcmp(blist->_account, buddy) != 0) {
		script_hook_changebuddy(blist, buddy);
		STRREPLACE(blist->_account, buddy);
	}
	if ((bwin = bgetbuddywin(conn, blist)) == NULL) {
		if (getvar_int(conn, "autoquery") != 0) {
			bnewwin(conn, buddy, BUDDY);
			bwin = bgetbuddywin(conn, blist);
			assert(bwin != NULL);
		}
	}
	assert((bwin == NULL) || (bwin->e.buddy == blist));

	if (blist->offline == 1) {
		blist->isadmin = blist->ismobile = blist->isidle = blist->isaway = blist->offline = 0;
		status_echof(conn, "<font color=\"#00FFFF\">%s</font> <font color=\"#800000\">[<B>%s</B>]</font> is now online =)\n",
			user_name(NULL, 0, conn, blist), USER_GROUP(blist));
		if (bwin != NULL) {
			window_echof(bwin, "<font color=\"#00FFFF\">%s</font> <font color=\"#800000\">[<B>%s</B>]</font> is now online =)\n",
				user_name(NULL, 0, conn, blist), USER_GROUP(blist));
			if (bwin->pouncec > 0) {
				int	i, pc = bwin->pouncec;

				for (i = 0; i < pc; i++) {
					window_echof(bwin, "Sending queued IM %i/%i [%s].\n",
						i+1, pc, bwin->pouncear[i]);
					naim_send_im(conn, bwin->winname,
						bwin->pouncear[i], 1);
				}
				bwin->pouncec -= pc;
				memmove(bwin->pouncear, bwin->pouncear+pc,
					bwin->pouncec*sizeof(*(bwin->pouncear)));
				bwin->pouncear = realloc(bwin->pouncear,
					bwin->pouncec*sizeof(*(bwin->pouncear)));
			}
		}

		{
			int	beeponsignon = getvar_int(conn, "beeponsignon");

			if ((beeponsignon > 1) || ((awaytime == 0) && (beeponsignon == 1)))
				beep();
		}
	}
	bupdate();
}

void	bgoing(conn_t *conn, const char *buddy) {
	buddywin_t *bwin = conn->curbwin;
	buddylist_t *blist = NULL;

	assert(buddy != NULL);

	if (bwin == NULL)
		return;

	if ((blist = rgetlist(conn, buddy)) != NULL) {
		if ((blist->peer <= 0) && (blist->crypt != NULL))
			echof(conn, NULL, "Strangeness while marking %s offline: no autopeer negotiated, but autocrypt set!\n",
				buddy);
		blist->docrypt = blist->peer = 0;
		FREESTR(blist->crypt);
		FREESTR(blist->tzname);
		FREESTR(blist->caps);

		status_echof(conn, "<font color=\"#00FFFF\">%s</font> <font color=\"#800000\">[<B>%s</B>]</font> has just logged off :(\n",
			user_name(NULL, 0, conn, blist), USER_GROUP(blist));
		blist->offline = 1;
		blist->warnval = blist->typing = blist->isadmin = blist->ismobile = blist->isidle = blist->isaway = 0;
	} else
		return;

	do {
		if ((bwin->et == BUDDY) && (firetalk_compare_nicks(conn->conn, buddy, bwin->winname) == FE_SUCCESS)) {
			int	autoclose = getvar_int(conn, "autoclose"),
				beeponsignon = getvar_int(conn, "beeponsignon");

			assert(bwin->e.buddy == blist);
			window_echof(bwin, "<font color=\"#00FFFF\">%s</font> <font color=\"#800000\">[<B>%s</B>]</font> has just logged off :(\n",
				user_name(NULL, 0, conn, blist), USER_GROUP(blist));
			if ((beeponsignon > 1) || ((awaytime == 0) && (beeponsignon == 1)))
				beep();
			FREESTR(bwin->blurb);
			FREESTR(bwin->status);

			if (bwin->keepafterso == 1) {
				if ((autoclose > 0) && !USER_PERMANENT(bwin->e.buddy) && (bwin->waiting == 0))
					bwin->closetime = now + 60*autoclose;
			} else {
				/* assert(bwin->waiting == 0); */
				bclose(conn, bwin, 1);
				bwin = NULL;
				if ((autoclose > 0) && !USER_PERMANENT(blist))
					if (firetalk_im_remove_buddy(conn->conn, buddy) != FE_SUCCESS)
						rdelbuddy(conn, buddy);
			}
			bupdate();
			return;
		}
	} while ((bwin = bwin->next) != conn->curbwin);
}

void	bidle(conn_t *conn, const char *buddy, int isidle) {
	buddywin_t *bwin = NULL;
	buddylist_t *blist = NULL;

	assert(buddy != NULL);
	bwin = bgetwin(conn, buddy, BUDDY);
	if (bwin == NULL)
		blist = rgetlist(conn, buddy);
	else
		blist = bwin->e.buddy;
	assert(blist != NULL);

	if (bwin != NULL) {
		if ((isidle == 1) && (blist->isidle == 0))
			window_echof(bwin, "<font color=\"#00FFFF\">%s</font> is now idle.\n",
				user_name(NULL, 0, conn, blist));
		else if ((isidle == 0) && (blist->isidle == 1))
			window_echof(bwin, "<font color=\"#00FFFF\">%s</font> is no longer idle!\n",
				user_name(NULL, 0, conn, blist));
	}

	blist->isidle = isidle;
}

void	baway(conn_t *conn, const char *buddy, int isaway) {
	buddywin_t *bwin = NULL;
	buddylist_t *blist = NULL;

	assert(buddy != NULL);
	bwin = bgetwin(conn, buddy, BUDDY);
	if (bwin == NULL)
		blist = rgetlist(conn, buddy);
	else
		blist = bwin->e.buddy;
	assert(blist != NULL);

	/* XXX need to pass to chain available message: bwin->status */
	if (bwin != NULL) {
		if ((isaway == 0) && (bwin->blurb != NULL)) {
			free(bwin->blurb);
			bwin->blurb = NULL;
		}
	}

	blist->isaway = isaway;
}

static void
	bclearall_bwin(conn_t *conn, buddywin_t *bwin, int force) {
	FREESTR(bwin->blurb);
	FREESTR(bwin->status);
	switch (bwin->et) {
	  case BUDDY:
		assert(bwin->e.buddy != NULL);
		if (bwin->e.buddy->offline == 0) {
			bwin->e.buddy->offline = 1;
			window_echof(bwin, "<font color=\"#00FFFF\">%s</font> <font color=\"#800000\">[<B>%s</B>]</font> is no longer available :/\n",
				user_name(NULL, 0, conn, bwin->e.buddy), USER_GROUP(bwin->e.buddy));
		}
		if (bwin->keepafterso == 0) {
			bclose(conn, bwin, 1);
			bwin = NULL;
			return;
		}
		break;
	  case CHAT:
		bwin->e.chat->isoper = 0;
		if (bwin->e.chat->offline == 0) {
			bwin->e.chat->offline = 1;
			window_echof(bwin, "Chat <font color=\"#00FFFF\">%s</font> is no longer available :/\n",
				bwin->winname);
		}
		break;
	  case TRANSFER:
		break;
	}
	if (force) {
		bclose(conn, bwin, 1);
		bwin = NULL;
	}
}

static void bclearall_buddy(buddylist_t *buddy) {
	FREESTR(buddy->crypt);
	FREESTR(buddy->tzname);
	FREESTR(buddy->caps);
	buddy->docrypt = buddy->warnval = buddy->typing = buddy->peer = buddy->isaway = buddy->isidle = buddy->isadmin = buddy->ismobile = 0;
	buddy->offline = 1;
}

void	bclearall(conn_t *conn, int force) {
	if (conn->curbwin != NULL) {
		buddywin_t *bwin = conn->curbwin;
		int	i, l = 0;

		do {
			l++;
		} while ((bwin = bwin->next) != conn->curbwin);

		for (i = 0; i < l; i++) {
			buddywin_t *bnext = bwin->next;

			bclearall_bwin(conn, bwin, force);
			bwin = bnext;
		}
		if (force)
			assert(conn->curbwin == NULL);
	}

	if (conn->buddyar != NULL) {
		buddylist_t *blist = conn->buddyar;

		if (force) {
			buddylist_t *bnext;

			do {
				bnext = blist->next;
				if (firetalk_im_remove_buddy(conn->conn, blist->_account) != FE_SUCCESS)
					do_delbuddy(blist);
			} while ((blist = bnext) != NULL);
			conn->buddyar = NULL;
		} else
			do {
				bclearall_buddy(blist);
			} while ((blist = blist->next) != NULL);
	}

	bupdate();
}

void	naim_changetime(void) {
	int	autohide = script_getvar_int("autohide");

	if (changetime > 0) {
		if (buddyc < 0)
			changetime = nowf - SLIDETIME - SLIDETIME/autohide;
		else if ((changetime + autohide) < nowf)
			changetime = nowf;
		else if (((changetime + SLIDETIME) < nowf) || (buddyc < 0))
			changetime = nowf - SLIDETIME - SLIDETIME/autohide;
	}
}
