/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#define _GNU_SOURCE
#include <string.h>

#include <naim/naim.h>
#include <naim/modutil.h>

#include "naim-int.h"
#include "snapshot.h"
#include "cmdar.h"

#ifdef ENABLE_DNSUPDATE
# include <netdb.h>
#endif

extern conn_t	*curconn;
extern time_t	startuptime, awaytime;
extern faimconf_t faimconf;

void	updateidletime(void) {
	if ((awaytime > 0) && (script_getvar_int("autounaway") != 0))
		unsetaway();
	if (script_getvar_int("autounidle") > 0)
		script_setvar("idletime", "0");
	bupdate();
}

static int events_idleaway(void *userdata, const char *signature, time_t now, double nowf) {
	long	idletime = script_getvar_int("idletime");
	int	autoaway;

	if (script_getvar_int("autoidle") > 0) {
		char	buf[1024];

		idletime++;
		snprintf(buf, sizeof(buf), "%lu", idletime);
		script_setvar("idletime", buf);
	}

	if (awaytime > 0)
		autoaway = 0;
	else
		autoaway = script_getvar_int("autoaway");

	if ((autoaway > 0) && (idletime >= autoaway)) {
		char	*autoawaymsg = script_getvar("autoawaymsg");

		echof(curconn, "TIMER", "You have been idle for more than %i minutes, so I'm going to mark you away. If you don't want me to do this in the future, just type ``/set autoaway 0'' (you can put that in your .naimrc).\n",
			autoaway);
		if (autoawaymsg != NULL)
			script_setvar("awaymsg", autoawaymsg);
		setaway(1);
	}

	return(HOOK_CONTINUE);
}

static int events_autoreconnect(void *userdata, const char *signature, time_t now, double nowf) {
	conn_t	*conn = curconn;

	do {
		if ((conn->online == -1) && (getvar_int(conn, "autoreconnect") != 0)) {
			echof(conn, NULL, "Attempting to reconnect...\n");
			ua_connect(conn, 0, NULL);
		}
	} while ((conn = conn->next) != curconn);

	return(HOOK_CONTINUE);
}

static int events_lagcheck(void *userdata, const char *signature, time_t now, double nowf) {
	int	lagcheck = script_getvar_int("lagcheck");
	conn_t	*conn = curconn;

	do {
		if ((conn->online > 0) && (lagcheck != 0)) {
			char    pingbuf[100];

			snprintf(pingbuf, sizeof(pingbuf), "%lu.%05i", now, (int)(100000*(nowf-((int)nowf))));
			firetalk_subcode_send_request(conn->conn, conn->sn, "LC", pingbuf);
		}
	} while ((conn = conn->next) != curconn);

	return(HOOK_CONTINUE);
}

static int events_winlistmaint(void *userdata, const char *signature, time_t now, double nowf) {
	int	tprint, logtprint, dailycol, regularcol;
	conn_t	*conn;
	struct tm *tmptr;
	int	cleanedone = 0;

	whidecursor();

	tprint = script_getvar_int("tprint");
	logtprint = script_getvar_int("logtprint");

	tmptr = localtime(&now);
	assert(tmptr != NULL);

	if (logtprint > 1) {
		dailycol = C(IMWIN,TEXT);
		regularcol = C(IMWIN,TEXT);
	} else if (logtprint > 0) {
		dailycol = C(IMWIN,TEXT);
		regularcol = -C(IMWIN,TEXT)-1;
	} else {
		dailycol = -C(IMWIN,TEXT)-1;
		regularcol = -C(IMWIN,TEXT)-1;
	}

	conn = curconn;
	do {
		buddywin_t *bwin = conn->curbwin;

		if ((conn->online > 0) && (awaytime > 0))
			naim_set_info(conn, conn->profile);

		if (bwin != NULL) {
			buddywin_t *bnext;
			time_t	nowm = now/60;

			verify_winlist_sanity(conn, NULL);

			do {
				verify_winlist_sanity(conn, bwin);

				if (bwin != bwin->next)
					bnext = bwin->next;
				else
					bnext = NULL;

				if (!cleanedone && bwin->nwin.dirty) {
					do_resize(conn, bwin);
					cleanedone = 1;
				}

				verify_winlist_sanity(conn, bwin);

				if ((tmptr->tm_hour == 0) && (tmptr->tm_min == 0)) {
					hwprintf(&(bwin->nwin), dailycol, "<I>-----</I>"
						" [<B>%04i</B>-<B>%02i</B>-<B>%02i</B>] <I>-----</I><br>",
						1900+tmptr->tm_year, 1+tmptr->tm_mon, tmptr->tm_mday);
					if (bwin->nwin.logfile != NULL) {
						nw_statusbarf("Flushing log file for %s.", bwin->winname);
						fflush(bwin->nwin.logfile);
					}
				} else if ((tprint != 0) && (nowm%tprint == 0) && ((bwin->et != CHAT) || (*(bwin->winname) != ':')))
					hwprintf(&(bwin->nwin), regularcol, "<I>-----</I>"
						" [<B>%02i</B>:<B>%02i</B>] <I>-----</I><br>",
						tmptr->tm_hour, tmptr->tm_min);

				if ((bwin->closetime > 0) && (bwin->closetime <= now)) {
					assert(bwin->et == BUDDY);
					if ((bwin->e.buddy->offline == 0) || USER_PERMANENT(bwin->e.buddy) || (bwin->pouncec > 0))
						bwin->closetime = 0;
					else {
						char	*name = strdup(bwin->winname);

						status_echof(conn, "Cleaning up auto-added buddy %s.\n", name);
						bclose(conn, bwin, 1);
						bwin = NULL;
						if (firetalk_im_remove_buddy(conn->conn, name) != FE_SUCCESS)
							rdelbuddy(conn, name);
						free(name);
					}
				}
			} while ((bnext != NULL) && ((bwin = bnext) != conn->curbwin));
		}

		if (conn->curbwin != NULL)
			verify_winlist_sanity(conn, NULL);
	} while ((conn = conn->next) != curconn);

	bupdate();

	return(HOOK_CONTINUE);
}

#ifdef ENABLE_DNSUPDATE

#define TOPDOMAIN "naim.joshuawise.com"
#define WEBURL "https://github.com/jwise/naim/downloads"

static int events_dnsupdate(void *userdata, const char *signature, time_t now, double nowf) {
	int	updatecheck = script_getvar_int("updatecheck");

	if ((updatecheck > 0) && (((now-startuptime)/60)%updatecheck == 0) && strcmp(NAIM_SNAPSHOT, "-git")) {
		struct hostent *ent;

		nw_statusbarf("Anonymously checking for the latest version of naim...");
		if ((ent = gethostbyname("latest." TOPDOMAIN)) != NULL) {
			int	i;

			for (i = 0; ent->h_aliases[i] != NULL; i++)
				;
			if ((i > 0) && (strlen(ent->h_aliases[i-1]) > strlen("." TOPDOMAIN))) {
				char	buf[64];

				snprintf(buf, sizeof(buf), "%.*s", (int)(strlen(ent->h_aliases[i-1])-strlen("." TOPDOMAIN)), ent->h_aliases[i-1]);
# ifdef HAVE_STRVERSCMP
				if (strverscmp(PACKAGE_VERSION NAIM_SNAPSHOT, buf) < 0) {
# else
				if (strcmp(PACKAGE_VERSION NAIM_SNAPSHOT, buf) < 0) {
# endif
					echof(curconn, NULL, "Current version: <font color=\"#FF0000\">" PACKAGE_VERSION NAIM_SNAPSHOT "</font>\n");
					echof(curconn, NULL, "&nbsp;Latest version: <font color=\"#00FF00\">%s</font> (reported by " TOPDOMAIN ")\n", buf);
					echof(curconn, NULL,
# ifdef DNSUPDATE_MESSAGE
						DNSUPDATE_MESSAGE
# else
						"You may be using an old version of naim. Please visit <font color=\"#0000FF\">" WEBURL "</font> or contact your system vendor for more information.\n"
# endif
					);
				}
			}
		}
	}

	return(HOOK_CONTINUE);
}
#endif

void	events_hook_init(void) {
	void	*mod = NULL;

	HOOK_ADD(periodic, mod, events_idleaway, 100, NULL);
	HOOK_ADD(periodic, mod, events_autoreconnect, 100, NULL);
	HOOK_ADD(periodic, mod, events_lagcheck, 100, NULL);
	HOOK_ADD(periodic, mod, events_winlistmaint, 100, NULL);
#ifdef ENABLE_DNSUPDATE
	HOOK_ADD(periodic, mod, events_dnsupdate, 100, NULL);
#endif
}
