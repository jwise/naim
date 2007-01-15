/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>
#include <naim/modutil.h>
#include <ltdl.h>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "naim-int.h"
#include "snapshot.h"

#ifdef HAVE_HSTRERROR
# include <netdb.h>
#endif

#ifndef UACPP
# include "cmdar.h"
#endif

extern win_t	win_input, win_buddy, win_info;
extern conn_t	*curconn;
extern faimconf_t faimconf;
extern int	stayconnected;
extern time_t	now, awaytime;
extern double	nowf, changetime;
extern const char *home, *sty;
extern lt_dlhandle dl_self;

extern int scrollbackoff G_GNUC_INTERNAL,
	needpass G_GNUC_INTERNAL,
	consolescroll G_GNUC_INTERNAL,
	namec G_GNUC_INTERNAL;
extern char **names G_GNUC_INTERNAL,
	*lastclose G_GNUC_INTERNAL;
int	scrollbackoff = 0,
	needpass = 0,
	consolescroll = -1,
	namec = 0;
char	**names = NULL,
	*lastclose = NULL;

static const char *collist[] = {
	"CLEAR/BLACK",
	"RED",
	"GREEN",
	"YELLOW/BROWN",
	"BLUE",
	"MAGENTA/PINK",
	"CYAN",
	"WHITE/GREY"
};

static const char *forelist[] = {
	"EVENT",
	"EVENT_ALT",
	"TEXT",
	"SELF",
	"BUDDY",
	"BUDDY_WAITING",
	"BUDDY_ADDRESSED",
	"BUDDY_IDLE",
	"BUDDY_AWAY",
	"BUDDY_OFFLINE",
	"BUDDY_QUEUED",
	"BUDDY_TAGGED",
	"BUDDY_FAKEAWAY",
	"BUDDY_TYPING",
};

static const char *backlist[] = {
	"INPUT",
	"WINLIST",
	"WINLISTHIGHLIGHT",
	"CONN",
	"IMWIN",
	"STATUSBAR",
};





UAFUNC(jump) {
UADESC(Go to the specified window or the next 'active' one)
UAAOPT(window,winname)
	buddywin_t	*bwin = NULL;
	conn_t	*c = conn;

	if (argc == 0) {
		do {
			if ((bwin = c->curbwin) != NULL)
			  do {
				if (bwin->waiting == 1)
					break;
			  } while ((bwin = bwin->next) != c->curbwin);
			if (bwin != NULL) {
				if (bwin->waiting == 1)
					break;
				else
					bwin = NULL;
			}
		} while ((c = c->next) != conn);
	} else
		if ((bwin = bgetanywin(conn, args[0])) == NULL) {
			char	*colon;

			if ((colon = strchr(args[0], ':')) != NULL) {
				*colon++ = 0;
				do {
					if (strcasecmp(args[0], c->winname) == 0)
						break;
				} while ((c = c->next) != conn);
				if (strcasecmp(args[0], c->winname) != 0) {
					echof(conn, "JUMP", "No connection named %s.\n",
						args[0]);
					return;
				}
				if ((bwin = bgetanywin(c, colon)) == NULL) {
					echof(conn, "JUMP", "No window in %s named %s.\n",
						c->winname, colon);
					return;
				}
			} else {
				echof(conn, "JUMP", "No window in %s named %s.\n",
					conn->winname, args[0]);
				return;
			}
		}
	if ((bwin != NULL) && (bwin != curconn->curbwin)) {
		if (c != curconn)
			curconn = c;
		assert(curconn->curbwin != NULL);	/* no way for curconn->curbwin to be NULL if we found a valid bwin! */
		c->curbwin = bwin;
		scrollbackoff = 0;
		bupdate();
		nw_touchwin(&(bwin->nwin));
	}
}

UAFUNC(msg) {
UAALIA(m)
UAALIA(im)
UADESC(Send a message; as in /msg naimhelp naim is cool!)
UAAREQ(window,name)
UAAREQ(string,message)
	buddywin_t *bwin;
	struct tm *tmptr;

	if (args[0] == NULL) {
		bwin = conn->curbwin;
		args[0] = bwin->winname;
	} else if ((bwin = bgetwin(conn, args[0], BUDDY)) == NULL)
		bwin = bgetwin(conn, args[0], CHAT);

	tmptr = localtime(&now);
	assert(tmptr != NULL);

	if (bwin != NULL) {
		const char *format = NULL, *pre = getvar(conn, "im_prefix"), *post = getvar(conn, "im_suffix");

		switch (bwin->et) {
		  case CHAT:
			chat_flush(bwin);
			format = "&lt;<B>%s</B>&gt;";
			break;
		  case BUDDY:
			if (bwin->e.buddy->crypt != NULL)
				format = "<B>%s:</B>";
			else
				format = "<B>%s</B>:";
			break;
		  case TRANSFER:
			echof(conn, "MSG", "You can't send a message to a file transfer!\n");
			return;
		}

		WINTIME(&(bwin->nwin), IMWIN);
		hwprintf(&(bwin->nwin), C(IMWIN,SELF),
			format, (conn->sn != NULL)?conn->sn:"(me)");
		hwprintf(&(bwin->nwin), C(IMWIN,TEXT),
			" %s%s%s<br>", pre?pre:"", args[1], post?post:"");
	}
	if ((conn != curconn) || (conn->curbwin == NULL)
		|| (firetalk_compare_nicks(conn->conn, conn->curbwin->winname, args[0]) != FE_SUCCESS)) {
		const char *pre = getvar(conn, "im_prefix"), *post = getvar(conn, "im_suffix");

		WINTIME(&(conn->nwin), CONN);
		hwprintf(&(conn->nwin), C(CONN,SELF), "-&gt; *<B>%s</B>*", args[0]);
		hwprintf(&(conn->nwin), C(CONN,TEXT), " %s%s%s<br>", pre?pre:"", args[1], post?post:"");
		naim_lastupdate(conn);
	}

	logim(conn, conn->sn, args[0], args[1]);
	naim_send_im(conn, args[0], args[1], 0);
}

UAFUNC(addbuddy) {
UAALIA(add)
UAALIA(friend)
UADESC(Add someone to your buddy list or change their group membership)
UAAREQ(account,account)
UAAOPT(string,group)
UAAOPT(string,realname)
	const char *group = "Buddy", *name = NULL;
	fte_t	ret;

	switch (argc) {
	  default:
		name = args[2];
	  case 2:
		group = args[1];
	  case 1:
		break;
	}

	if ((ret = firetalk_im_add_buddy(conn->conn, args[0], group, name)) != FE_SUCCESS)
		echof(conn, "ADDBUDDY", "Unable to add buddy: %s.\n", firetalk_strerror(ret));
}

static void do_delconn(conn_t *conn) {
	bclearall(conn, 1);

	script_hook_delconn(conn);

	firetalk_disconnect(conn->conn);
	firetalk_destroy_conn(conn->conn);
	conn->conn = NULL;

	if (conn->next == conn) {
		assert(conn == curconn);
		curconn = NULL;
	} else {
		conn_t	*prev;

		for (prev = conn->next; prev->next != conn; prev = prev->next)
			;
		prev->next = conn->next;

		if (curconn == conn)
			curconn = conn->next;
		conn->next = conn;
	}

	FREESTR(conn->sn);
	FREESTR(conn->password);
	FREESTR(conn->winname);
	FREESTR(conn->server);
	FREESTR(conn->profile);

	if (conn->logfile != NULL) {
		fclose(conn->logfile);
		conn->logfile = NULL;
	}

	nw_delwin(&(conn->nwin));

	assert(conn->buddyar == NULL);
//	assert(conn->idiotar == NULL);
	assert(conn->curbwin == NULL);

	free(conn);
}

UAFUNC(exit) {
UAALIA(quit)
UADESC(Disconnect and exit naim)
	conn_t	*c;

	if (script_getvar_int("autosave"))
		ua_save(conn, 0, NULL);

	c = conn;
	do {
		firetalk_disconnect(c->conn);
	} while ((c = c->next) != conn);

	while (curconn != NULL) {
		do_delconn(curconn);
		statrefresh();
	}
	stayconnected = 0;
}

UAFUNC(save) {
UADESC(Write current settings to ~/.naimrc to be loaded at startup)
UAAOPT(string,filename)
	conn_t	*c = conn;
	FILE	*file;
	int	i;
	extern rc_var_s_t rc_var_s_ar[];
	extern const int rc_var_s_c;
	extern rc_var_i_t rc_var_i_ar[];
	extern const int rc_var_i_c;
	extern rc_var_i_t rc_var_b_ar[];
	extern const int rc_var_b_c;
	extern char naimrcfilename[];
	const char *filename;

	if (argc == 0)
		filename = naimrcfilename;
	else
		filename = args[0];

	if ((file = fopen(filename, "w")) == NULL) {
		echof(conn, "SAVE", "Error %i opening file for write: %s",
			errno, strerror(errno));
		return;
	}

	fchmod(fileno(file), 0600);

	fprintf(file,
"# naim configuration file.\n"
"# This file was automatically generated by using the /save command from within\n"
"# naim. Feel free to modify it with any text editor.\n"
"# \n"
"# Lines beginning with a # are ignored by naim, so I will use those to document\n"
"# each line I write out. In most cases I will attempt to skip over writing out\n"
"# redundant information, such as when your settings matched the defaults, but\n"
"# in situations where I do include such information I will also place a # in\n"
"# front, to show you what the default value is without actually setting it.\n"
"# \n"
"# The structure of this file is fairly simple: Lines that start with a # are\n"
"# ignored, lines that don't are treated as if you had typed them when first\n"
"# starting naim (so anything that can go after a forward slash while running\n"
"# naim is fair game for inclusion here).\n"
"# \n"
"# If you have any questions about this file, or naim in general, feel free to\n"
"# contact Daniel Reed <n@ml.org>, or visit:\n"
"# 	http://naim.n.ml.org/\n"
"\n");

	fprintf(file, "# The window list can either be always visible, always hidden, or auto-hidden.\n");
	if (changetime == -1)
		fprintf(file, "WINLIST Hidden\n\n");
	else if (changetime == 0)
		fprintf(file, "WINLIST Visible\n\n");
	else
		fprintf(file, "#WINLIST Auto\n\n");

	for (i = 0; i < rc_var_s_c; i++) {
		const char *glob = script_getvar(rc_var_s_ar[i].var),
			*def = rc_var_s_ar[i].val,
			*use, *cm, *q;

		assert(def != NULL);
		if (glob != NULL) {
			use = glob;
			if (strcmp(glob, def) == 0)
				cm = "#";
			else
				cm = "";
		} else {
			use = def;
			cm = "#";
		}

		q = ((*use == 0) || ((strchr(use, ' ') != NULL))?"\"":"");

		if (rc_var_s_ar[i].desc != NULL)
			fprintf(file, "# %s (%s)\n", rc_var_s_ar[i].var, rc_var_s_ar[i].desc);
		else
			fprintf(file, "# %s\n", rc_var_s_ar[i].var);
		fprintf(file, "%sset %s %s%s%s\n\n", cm, rc_var_s_ar[i].var, q, use, q);
	}

	for (i = 0; i < rc_var_i_c; i++) {
		const char *glob = script_getvar(rc_var_i_ar[i].var),
			*cm;
		const int globi = atoi(glob),
			defi = rc_var_i_ar[i].val;
		int	usei;

		if (glob != NULL) {
			usei = globi;
			if (globi == defi)
				cm = "#";
			else
				cm = "";
		} else {
			usei = defi;
			cm = "#";
		}

		if (rc_var_i_ar[i].desc != NULL)
			fprintf(file, "# %s (%s)\n", rc_var_i_ar[i].var, rc_var_i_ar[i].desc);
		else
			fprintf(file, "# %s\n", rc_var_i_ar[i].var);
		fprintf(file, "%sset %s %i\n\n", cm, rc_var_i_ar[i].var, usei);
	}

	for (i = 0; i < rc_var_b_c; i++) {
		const char *glob = script_getvar(rc_var_b_ar[i].var),
			*cm;
		const int globi = atoi(glob),
			defi = rc_var_b_ar[i].val;
		int	usei;

		if (glob != NULL) {
			usei = globi;
			if (globi == defi)
				cm = "#";
			else
				cm = "";
		} else {
			usei = defi;
			cm = "#";
		}

		if (rc_var_b_ar[i].desc != NULL)
			fprintf(file, "# %s (%s)\n", rc_var_b_ar[i].var, rc_var_b_ar[i].desc);
		else
			fprintf(file, "# %s\n", rc_var_b_ar[i].var);
		fprintf(file, "%sset %s %i\n\n", cm, rc_var_b_ar[i].var, usei);
	}

	{
		faimconf_t fc;

		rc_initdefs(&fc);

		fprintf(file, "# Colors\n");
		for (i = 0; i < sizeof(forelist)/sizeof(*forelist); i++)
			fprintf(file, "%ssetcol %s %s%s\n", (fc.f[i] == faimconf.f[i])?"#":"",
				forelist[i], collist[faimconf.f[i]%(nw_COLORS*nw_COLORS)],
				(faimconf.f[i]>=2*(nw_COLORS*nw_COLORS))?" BOLD":(faimconf.f[i]>=(nw_COLORS*nw_COLORS))?" DULL":"");
		for (i = 0; i < sizeof(backlist)/sizeof(*backlist); i++)
			fprintf(file, "%ssetcol %s %s\n", (fc.b[i] == faimconf.b[i])?"#":"",
				backlist[i], collist[faimconf.b[i]]);
		fprintf(file, "\n");
	}

	conio_bind_save(file);

	if (awaytime > 0)
		fprintf(file, "# You were away when you /saved.\nAWAY\n\n");

	{
		extern alias_t *aliasar;
		extern int aliasc;
		int	i;

		if (aliasc > 0) {
			fprintf(file, "# Aliases.\n");
			for (i = 0; i < aliasc; i++)
				fprintf(file, "ALIAS %s %s\n", aliasar[i].name, aliasar[i].script);
			fprintf(file, "\n");
		}
	}

	{
		extern html_clean_t *html_cleanar;
		extern int html_cleanc;
		int	i;

		fprintf(file, "# Filters.\n");
		fprintf(file, "FILTER REPLACE :FLUSH\n");
		for (i = 0; i < html_cleanc; i++)
			if (*html_cleanar[i].from != 0)
				fprintf(file, "FILTER REPLACE %s %s\n",
					html_cleanar[i].from, html_cleanar[i].replace);
		fprintf(file, "\n");
	}

	fprintf(file, "\n");

	do {
		fprintf(file,
"\n"
"\n"
"# Create a new connection called %s of type %s.\n"
"newconn %s %s\n"
"\n",				c->winname, firetalk_strprotocol(c->proto),
				c->winname, firetalk_strprotocol(c->proto));

		fprintf(file,
"# naim will automatically attempt to read your profile from\n"
"# ~/.naimprofile, though if you prefer you can specify one\n"
"# to replace it here.\n"
"#%s:readprofile %s/.naimprofile\n"
"\n",				c->winname, home);

		{
			buddylist_t *blist = c->buddyar;

			if (blist != NULL) {
			  fprintf(file, "# Add your buddies.\n");
			  do {
				if (USER_PERMANENT(blist)) {
					char	*b = USER_ACCOUNT(blist),
 						*g = USER_GROUP(blist),
						*n = (blist->_name != NULL)?blist->_name:"",
						*bq = (strchr(b, ' ') != NULL)?"\"":"",
						*gq = ((*g == 0)
							|| (strchr(g, ' ') != NULL))?"\"":"";

					fprintf(file,
"%s:addbuddy\t%s%s%s\t%s%s%s\t%s\n",
						c->winname, bq, b, bq, gq, g, gq, n);
				}
			  } while ((blist = blist->next) != NULL);
			  fprintf(file, "\n");
			}
		}

		{
			ignorelist_t *blist = c->idiotar;

			if (blist != NULL) {
			  fprintf(file, "# Add your ignore list.\n");
			  do {
				char	*b = blist->screenname,
					*n = (blist->notes != NULL)?blist->notes:"",
					*bq = (strchr(b, ' ') != NULL)?"\"":"";

				if (strcasecmp(n, "block") == 0)
					fprintf(file,
"%s:block\t%s\n",
					c->winname, b);
				else
					fprintf(file,
"%s:ignore\t%s%s%s\t%s\n",
					c->winname, bq, b, bq, n);
			  } while ((blist = blist->next) != NULL);
			  fprintf(file, "\n");
			}
		}

		{
			buddywin_t *bwin = c->curbwin;

			if (bwin != NULL) {
			  fprintf(file, "# Rejoin your channels.\n");
			  do {
				if ((bwin->et == CHAT) && (*(bwin->winname) != ':'))
					fprintf(file,
"%s:join %s%s%s%s%s\n",
						c->winname,
						strchr(bwin->winname, ' ')?"\"":"", bwin->winname, strchr(bwin->winname, ' ')?"\"":"",
						bwin->e.chat->key?" ":"", bwin->e.chat->key?bwin->e.chat->key:"");
				else if (bwin->pouncec > 0) {
					int	i;

					fprintf(file,
"%s:open %s\n",
						c->winname, bwin->winname);
					for (i = 0; i < bwin->pouncec; i++)
						fprintf(file,
"%s:msg \"%s\" %s\n",
							c->winname, bwin->winname, bwin->pouncear[i]);
				}
			  } while ((bwin = bwin->next) != c->curbwin);
			  fprintf(file, "\n");
			}
		}

		{
			int	log = getvar_int(c, "log");

			fprintf(file,
"# Logging\n"
"%sset %s:log\t%i\n"
"%sset %s:logfile\t%s%s%s%s\n"
"\n",				log?"":"#", c->winname, log,
				log?"":"#", c->winname,
					log?getvar(c, "logfile"):home,
					log?"":"/",
					log?"":c->winname,
					log?"":".log");
		}

		for (i = 0; i < rc_var_s_c; i++) {
			const char *loc = getvar(c, rc_var_s_ar[i].var),
				*glob = script_getvar(rc_var_s_ar[i].var),
				*q;

			assert(loc != NULL);
			assert(glob != NULL);
			if ((loc != glob) && (strcmp(loc, glob) != 0)) {
				q = ((*loc == 0) || ((strchr(loc, ' ') != NULL))?"\"":"");

				if (rc_var_s_ar[i].desc != NULL)
					fprintf(file, "# %s-specific %s (see above)\n", c->winname, 
						rc_var_s_ar[i].var);
				else
					fprintf(file, "# %s-specific %s\n", c->winname,
						rc_var_s_ar[i].var);
				fprintf(file, "set %s:%s %s%s%s\n\n", c->winname,
					rc_var_s_ar[i].var, q, loc, q);
			}
		}

		for (i = 0; i < rc_var_i_c; i++) {
			const char *loc = getvar(c, rc_var_i_ar[i].var),
				*glob = script_getvar(rc_var_i_ar[i].var);

			assert(loc != NULL);
			assert(glob != NULL);
			if ((loc != glob) && (strcmp(loc, glob) != 0)) {
				if (rc_var_i_ar[i].desc != NULL)
					fprintf(file, "# %s-specific %s (see above)\n", c->winname,
						rc_var_i_ar[i].var);
				else
					fprintf(file, "# %s-specific %s\n", c->winname,
						rc_var_i_ar[i].var);
				fprintf(file, "set %s:%s %s\n\n", c->winname,
					rc_var_i_ar[i].var, loc);
			}
		}

		for (i = 0; i < rc_var_b_c; i++) {
			const char *loc = getvar(c, rc_var_b_ar[i].var),
				*glob = script_getvar(rc_var_b_ar[i].var);

			assert(loc != NULL);
			assert(glob != NULL);
			if ((loc != glob) && (strcmp(loc, glob) != 0)) {
				if (rc_var_b_ar[i].desc != NULL)
					fprintf(file, "# %s-specific %s (see above)\n", c->winname,
						rc_var_b_ar[i].var);
				else
					fprintf(file, "# %s-specific %s\n", c->winname,
						rc_var_b_ar[i].var);
				fprintf(file, "set %s:%s %s\n\n", c->winname,
					rc_var_b_ar[i].var, loc);
			}
		}

		{
			char	*name = c->sn,
				*nameq = ((name == NULL) || (strchr(name, ' ') != NULL))?"\"":"",
				*pass = getvar(c, "password");

			fprintf(file, "# Connect to %s.\n", c->winname);
			if (pass != NULL) {
				char	*passq = (strchr(pass, ' ') != NULL)?"\"":"";

				fprintf(file, "set %s:password %s%s%s\nclear\n",
					c->winname, passq, pass, passq);
			} else
				fprintf(file,	"#set %s:password mypass\n"
						"#clear\n"
						"# naim will prompt you for your password if it's not supplied here.\n",
					c->winname);
			fprintf(file, "%s%s:connect %s%s%s", ((name != NULL) && (conn->online > 0))?"":"#",
				c->winname, nameq, name?name:"myname", 
				nameq);
			if (c->server != NULL) {
				fprintf(file, " %s", c->server);
				if (c->port > 0)
					fprintf(file, ":%i", c->port);
			}
			fprintf(file, "\n");
		}
	} while ((c = c->next) != conn);

	fclose(file);
	echof(conn, NULL, "Settings saved to %s.\n", filename);
}

UAFUNC(connect) {
UADESC(Connect to a service)
UAAOPT(string,name)
UAAOPT(string,server)
UAAOPT(int,port)
	fte_t	fte;

	if (conn->online > 0) {
		echof(conn, "CONNECT", "Please <font color=\"#00FF00\">/%s:disconnect</font> first (just so there's no confusion).\n",
			conn->winname);
		return;
	}

	if ((argc == 1) && (strchr(args[0], '.') != NULL) && (strchr(args[0], '@') == NULL)) {
		if (conn->sn != NULL) {
			echof(conn, "CONNECT", "It looks like you're specifying a server, so I'll use %s as your name.\n",
				conn->sn);
			args[1] = args[0];
			args[0] = conn->sn;
			argc = 2;
		} else {
			echof(conn, "CONNECT", "It looks like you're specifying a server, but I don't have a default name to use.\n");
			echof(conn, "CONNECT", "Please specify a name to use (<font color=\"#00FF00\">/%s:connect myname %s</font>).\n",
				conn->winname, args[0]);
			return;
		}
	}

	switch(argc) {
	  case 3:
		conn->port = atoi(args[2]);
	  case 2: {
		char	*tmp;

		if ((tmp = strchr(args[1], ':')) != NULL) {
			conn->port = atoi(tmp+1);
			*tmp = 0;
		}
		STRREPLACE(conn->server, args[1]);
	  }
	  case 1:
		if ((conn->sn == NULL) || ((conn->sn != args[0]) && (strcmp(conn->sn, args[0]) != 0)))
			STRREPLACE(conn->sn, args[0]);
	}

	if (conn->sn == NULL) {
		echof(conn, "CONNECT", "Please specify a name to use (<font color=\"#00FF00\">/%s:connect myname</font>).\n",
			conn->winname);
		return;
	}

	if (conn->port == 0) {
		if (conn->server == NULL)
			echof(conn, NULL, "Connecting to %s.\n",
				firetalk_strprotocol(conn->proto));
		else
			echof(conn, NULL, "Connecting to %s on server %s.\n",
				firetalk_strprotocol(conn->proto), conn->server);
	} else {
		if (conn->server == NULL)
			echof(conn, NULL, "Connecting to %s on port %i.\n",
				firetalk_strprotocol(conn->proto), conn->port);
		else
			echof(conn, NULL, "Connecting to %s on port %i of server %s.\n",
				firetalk_strprotocol(conn->proto), conn->port, conn->server);
	}

	statrefresh();

#ifdef HAVE_HSTRERROR
	h_errno = 0;
#endif
	errno = 0;
	switch ((fte = firetalk_signon(conn->conn, conn->server, conn->port, conn->sn))) {
	  case FE_SUCCESS:
		break;
	  case FE_CONNECT:
#ifdef HAVE_HSTRERROR
		if (h_errno != 0) {
			if (firetalkerror != 0)
				echof(conn, "CONNECT", "Unable to connect: %s (%s).\n",
					firetalk_strerror(firetalkerror), hstrerror(h_errno));
			else
				echof(conn, "CONNECT", "Unable to connect: %s.\n",
					hstrerror(h_errno));
		} else
#endif
		if (errno != 0) {
			if (firetalkerror != 0)
				echof(conn, "CONNECT", "Unable to connect: %s (%s).\n",
					firetalk_strerror(firetalkerror), strerror(errno));
			else
				echof(conn, "CONNECT", "Unable to connect: %s.\n",
					strerror(errno));
		} else if (firetalkerror != 0)
			echof(conn, "CONNECT", "Unable to connect: %s.\n",
				firetalk_strerror(firetalkerror));
		else
			echof(conn, "CONNECT", "Unable to connect.\n");
		break;
	  default:
		echof(conn, "CONNECT", "Connection failed in startup, %s.\n",
			firetalk_strerror(fte));
		break;
	}
}

UAFUNC(jumpback) {
UADESC(Go to the previous window)
	conn_t	*c = conn,
		*newestconn = NULL;
	buddywin_t *newestbwin = NULL;
	double	newestviewtime = 0;

	do {
		buddywin_t	*bwin;

		if ((bwin = c->curbwin) != NULL)
		  do {
			if (((c != conn) || (bwin != conn->curbwin)) && (bwin->viewtime > newestviewtime)) {
				newestconn = c;
				newestbwin = bwin;
				newestviewtime = bwin->viewtime;
			}
		  } while ((bwin = bwin->next) != c->curbwin);
	} while ((c = c->next) != conn);

	if (newestconn != NULL) {
		assert(newestbwin != NULL);
		if (newestconn != curconn)
			curconn = newestconn;
		assert(curconn->curbwin != NULL);	/* no way for curconn->curbwin to be NULL if we found a valid bwin! */
		newestconn->curbwin = newestbwin;
		scrollbackoff = 0;
		bupdate();
		nw_touchwin(&(newestbwin->nwin));
	}
}

UAFUNC(info) {
UAALIA(whois)
UAALIA(wi)
UADESC(Retrieve a user profile)
UAAOPT(entity,name)
	fte_t	ret;

	if (argc == 0) {
		if (!inconn || (conn->curbwin->et != BUDDY)) {
			if ((ret = firetalk_im_get_info(conn->conn, conn->sn)) != FE_SUCCESS)
				echof(conn, "INFO", "Unable to retrieve user information for %s: %s.\n",
					conn->sn, firetalk_strerror(ret));
		} else {
			if ((ret = firetalk_im_get_info(conn->conn, conn->curbwin->winname)) != FE_SUCCESS)
				echof(conn, "INFO", "Unable to retrieve user information for %s: %s.\n",
					conn->curbwin->winname, firetalk_strerror(ret));
		}
	} else {
		if ((ret = firetalk_im_get_info(conn->conn, args[0])) != FE_SUCCESS)
			echof(conn, "INFO", "Unable to retrieve user information for %s: %s.\n",
				args[0], firetalk_strerror(ret));
	}
}

void	naim_eval(const char *_str) {
	int	len = strlen(_str);
	char	*str = malloc(len+2), *p;

	strncpy(str, _str, len+2);
	while ((p = strrchr(str, ';')) != NULL)
		*p = 0;
	for (p = str; *p != 0; p += strlen(p)+1)
		ua_handlecmd(script_expand(p));
	free(str);
}

UAFUNC(eval) {
UADESC(Evaluate a command with $-variable substitution)
UAAREQ(string,script)
	if (args[0][0] == '/')
		naim_eval(args[0]);
	else
		script_script_parse(args[0]);
}

UAFUNC(say) {
UADESC(Send a message to the current window; as in /say I am happy)
UAAREQ(string,message)
UAWHER(NOTSTATUS)
	const char *newargs[2] = { conn->curbwin->winname, args[0] };

	ua_msg(conn, 2, newargs);
}

UAFUNC(me) {
UADESC(Send an 'action' message to the current window; as in /me is happy)
UAAREQ(string,message)
UAWHER(NOTSTATUS)
	WINTIME(&(conn->curbwin->nwin), IMWIN);
	hwprintf(&(conn->curbwin->nwin), C(IMWIN,SELF), "* <B>%s</B>", conn->sn);
	hwprintf(&(conn->curbwin->nwin), C(IMWIN,TEXT), "%s%s<br>", (strncmp(args[0], "'s ", 3) == 0)?"":" ", args[0]);
	logim(conn, conn->sn, conn->curbwin->winname, args[0]);
	naim_send_act(conn, conn->curbwin->winname, args[0]);
}

static void do_open(conn_t *conn, buddylist_t *blist, const int added) {
	buddywin_t *bwin;

	bnewwin(conn, USER_ACCOUNT(blist), BUDDY);
	bwin = bgetbuddywin(conn, blist);
	assert(bwin != NULL);
	if (added)
		window_echof(bwin, "Query window created and user added as a temporary buddy.\n");
	else
		window_echof(bwin, "Query window created.\n");
	conn->curbwin = bwin;
	nw_touchwin(&(bwin->nwin));
	scrollbackoff = 0;
	naim_changetime();
	bupdate();
}

UAFUNC(open) {
UAALIA(window)
UADESC(Open a query window for an existing or new buddy by account)
UAAREQ(account,name)
	buddylist_t *blist;
	int	added;

	if (bgetanywin(conn, args[0]) != NULL) {
		ua_jump(conn, argc, args);
		return;
	}

	if ((blist = rgetlist(conn, args[0])) == NULL) {
		blist = raddbuddy(conn, args[0], DEFAULT_GROUP, NULL);
		firetalk_im_add_buddy(conn->conn, args[0], USER_GROUP(blist), NULL);
		added = 1;
	} else
		added = 0;

	do_open(conn, blist, added);
}

UAFUNC(openbuddy) {
UADESC(Open a query window for an existing buddy by account or friendly name)
UAAREQ(buddy,name)
	buddylist_t *blist;

	if (bgetanywin(conn, args[0]) != NULL) {
		ua_jump(conn, argc, args);
		return;
	}

	if ((blist = rgetlist(conn, args[0])) == NULL)
		if ((blist = rgetlist_friendly(conn, args[0])) == NULL) {
			echof(conn, "OPENBUDDY", "Unable to find %s in your buddy list by account or name.\n", args[0]);
			return;
		}

	do_open(conn, blist, 0);
}

UAFUNC(close) {
UAALIA(endwin)
UAALIA(part)
UADESC(Close a query window or leave a discussion)
UAAOPT(window,winname)
UAWHER(NOTSTATUS)
	buddywin_t *bwin;

	if (argc == 1) {
		if ((bwin = bgetanywin(conn, args[0])) == NULL) {
			echof(conn, "CLOSE", "No window is open for <font color=\"#00FFFF\">%s</font>.\n",
				args[0]);
			return;
		}
	} else
		bwin = conn->curbwin;

	bclose(conn, bwin, 0);
	bwin = NULL;
}

UAFUNC(closeall) {
UADESC(Close stale windows for offline buddies)
	int	i, l;
	buddywin_t *bwin;

	if (conn->curbwin == NULL)
		return;

	l = 0;
	bwin = conn->curbwin;
	do {
		l++;
	} while ((bwin = bwin->next) != conn->curbwin);
	for (i = 0; i < l; i++) {
		buddywin_t *bnext = bwin->next;
		const char *_args[] = { bwin->winname };

		if ((bwin->et == BUDDY) && (bwin->e.buddy->offline > 0) && (bwin->pouncec == 0))
			ua_close(conn, 1, _args);
		else if ((bwin->et == CHAT) && (bwin->e.chat->offline > 0))
			ua_close(conn, 1, _args);
		bwin = bnext;
	}
}

UAFUNC(ctcp) {
UADESC(Send Client To Client Protocol request to someone)
UAAREQ(window,name)
UAAOPT(string,requestname)
UAAOPT(string,message)
	if (argc == 1)
		firetalk_subcode_send_request(conn->conn, args[0],
			"VERSION", NULL);
	else if (argc == 2)
		firetalk_subcode_send_request(conn->conn, args[0],
			args[1], NULL);
	else
		firetalk_subcode_send_request(conn->conn, args[0],
			args[1], args[2]);
}

UAFUNC(clear) {
UADESC(Temporarily blank the scrollback for the current window)
	win_t	*win;
	int	i;

	if (inconn) {
		assert(conn->curbwin != NULL);
		win = &(conn->curbwin->nwin);
	} else
		win = &(conn->nwin);

	nw_erase(win);
	for (i = 0; i < faimconf.wstatus.pady; i++)
		nw_printf(win, 0, 0, "\n");
}

UAFUNC(clearall) {
UADESC(Perform a /clear on all open windows)
	conn_t	*c = conn;

	do {
		buddywin_t *bwin = c->curbwin;

		if (bwin != NULL)
			do {
				int	i;

				nw_erase(&(bwin->nwin));
				for (i = 0; i < faimconf.wstatus.pady; i++)
					nw_printf(&(bwin->nwin), 0, 0, "\n");
				bwin->nwin.dirty = 0;
			} while ((bwin = bwin->next) != c->curbwin);
	} while ((c = c->next) != conn);
}

UAFUNC(load) {
UADESC(Load a command file (such as .naimrc))
UAAREQ(filename,filename)
	naim_read_config(args[0]);
}

UAFUNC(away) {
UADESC(Set or unset away status)
UAAOPT(string,message)
	if (argc == 0) {
		if (awaytime > 0)
			unsetaway();
		else
			setaway(0);
	} else {
		script_setvar("awaymsg", script_expand(args[0]));
		setaway(0);
	}
}

UAFUNC(buddylist) {
UADESC(Display buddy list)
UAAOPT(string,type)
	buddylist_t *blist;
	int	showon = 1, showoff = 1,
		maxname = strlen("Account"), maxgroup = strlen("Group"), maxnotes = strlen("Name"), max;
	char	*spaces;

	if ((argc > 0) && (strcasecmp(args[0], "ON") == 0))
		showoff = 0;
	else if ((argc > 0) && (strcasecmp(args[0], "OFF") == 0))
		showon = 0;

	if (conn->buddyar == NULL) {
		echof(conn, NULL, "Your buddy list is empty, try <font color=\"#00FF00\">/addbuddy buddyname</font>.\n");
		return;
	}

	echof(conn, NULL, "Buddy list:");

	blist = conn->buddyar;
	do {
		const char	*name = USER_ACCOUNT(blist),
				*nameq = ((*name == 0) || strchr(name, ' '))?"\"":"",
				*group = USER_GROUP(blist),
				*groupq = ((*group == 0) || strchr(group, ' '))?"\"":"",
				*notes = USER_NAME(blist),
				*notesq = ((*notes == 0) || strchr(notes, ' '))?"\"":"";
		int		namelen = strlen(name)+2*strlen(nameq),
				grouplen = strlen(group)+2*strlen(groupq),
				noteslen = strlen(notes)+2*strlen(notesq);

		if (namelen > maxname)
			maxname = namelen;
		if (grouplen > maxgroup)
			maxgroup = grouplen;
		if (noteslen > maxnotes)
			maxnotes = noteslen;
	} while ((blist = blist->next) != NULL);

	if (maxname > maxgroup)
		max = maxname;
	else
		max = maxgroup;
	if (maxnotes > max)
		max = maxnotes;

	if ((spaces = malloc(max*6 + 1)) == NULL)
		return;
	*spaces = 0;
	while (max > 0) {
		strcat(spaces, "&nbsp;");
		max--;
	}

	echof(conn, NULL, "</B>&nbsp; %s"
		" <font color=\"#800000\">%s<B>%s</B>%s</font>%.*s"
		" <font color=\"#008000\">%s<B>%s</B>%s</font>%.*s"
		" <font color=\"#000080\">%s<B>%s</B>%s</font>%.*s<B>%s\n",
		"&nbsp; &nbsp;",
		"", "Group", "",
		6*(maxgroup - strlen("Group")), spaces,
		"", "Account", "",
		6*(maxname - strlen("Account")), spaces,
		"", "Name", "",
		6*(maxnotes - strlen("Name")), spaces,
		" flags");

	blist = conn->buddyar;
	do {
		const char	*name = USER_ACCOUNT(blist),
				*nameq = ((*name == 0) || strchr(name, ' '))?"\"":"",
				*group = USER_GROUP(blist),
				*groupq = ((*group == 0) || strchr(group, ' '))?"\"":"",
				*notes = USER_NAME(blist),
				*notesq = ((*notes == 0) || strchr(notes, ' '))?"\"":"";
		int		namelen = strlen(name)+2*strlen(nameq),
				grouplen = strlen(group)+2*strlen(groupq),
				noteslen = strlen(notes)+2*strlen(notesq);

		if ((blist->offline == 0) && !showon)
			continue;
		else if ((blist->offline != 0) && !showoff)
			continue;
		echof(conn, NULL, "</B>&nbsp; %s"
			" <font color=\"#800000\">%s<B>%s</B>%s</font>%.*s"
			" <font color=\"#008000\">%s<B>%s</B>%s</font>%.*s"
			" <font color=\"#000080\">%s<B>%s</B>%s</font>%.*s<B>%s%s%s%s%s%s%s\n",
			blist->offline?"OFF":"<B>ON</B>&nbsp;",
			groupq, group, groupq,
			6*(maxgroup - grouplen), spaces,
			nameq, name, nameq,
			6*(maxname - namelen), spaces,
			notesq, notes, notesq,
			6*(maxnotes - noteslen), spaces,
			USER_PERMANENT(blist)?"":" NON-PERMANENT",
			blist->crypt?" CRYPT":"",
			blist->tzname?" TZNAME":"",
			blist->tag?" TAGGED":"",
			blist->isaway?" AWAY":"",
			blist->isidle?" IDLE":"",
			blist->warnval?" WARNED":"",
			blist->typing?" TYPING":"",
			(blist->peer > 0)?" PEER":"");
	} while ((blist = blist->next) != NULL);
	free(spaces);
	spaces = NULL;
	echof(conn, NULL, "Use the <font color=\"#00FF00\">/namebuddy</font> command to change a buddy's name, or <font color=\"#00FF00\">/groupbuddy</font> to change a buddy's group.");
}

UAFUNC(join) {
UADESC(Participate in a chat)
UAAREQ(string,chat)
UAAOPT(string,key)
	buddywin_t *cwin;

	if (((cwin = bgetwin(conn, firetalk_chat_normalize(conn->conn, args[0]), CHAT)) == NULL) || (cwin->e.chat->offline != 0)) {
		char	buf[1024];

		cwin = cgetwin(conn, args[0]);
		if (argc > 1) {
			window_echof(cwin, "Entering chat \"%s\" with a key of \"%s\"; if you intended to join chat \"%s %s\", please use <font color=\"#00FF00\">/join \"%s %s\"</font> in the future.\n",
				args[0], args[1], args[0], args[1], args[0], args[1]);
			STRREPLACE(cwin->e.chat->key, args[1]);
		}
		if (cwin->e.chat->key != NULL) {
			snprintf(buf, sizeof(buf), "%s %s", args[0], cwin->e.chat->key);
			args[0] = buf;
		}

		if (conn->online > 0) {
			fte_t	ret;

			if ((ret = firetalk_chat_join(conn->conn, args[0])) != FE_SUCCESS)
				echof(conn, "JOIN", "Unable to join %s: %s.\n", args[0], firetalk_strerror(ret));
		}
	}
}

UAFUNC(namebuddy) {
UADESC(Change the real name for a buddy)
UAAREQ(account,name)
UAAOPT(string,realname)
	if (argc == 1)
		ua_addbuddy(conn, 1, args);
	else {
		buddylist_t *blist = rgetlist(conn, args[0]);
		const char *_args[] = { args[0], (blist == NULL)?"Buddy":USER_GROUP(blist), args[1] };

		ua_addbuddy(conn, 3, _args);
	}
}

UAFUNC(groupbuddy) {
UADESC(Change the group membership for a buddy)
UAAREQ(account,account)
UAAOPT(string,group)
	if (argc == 1)
		ua_addbuddy(conn, 1, args);
	else {
		buddylist_t *blist = rgetlist(conn, args[0]);
		const char *_args[] = { args[0], args[1], (blist == NULL)?NULL:USER_NAME(blist) };

		ua_addbuddy(conn, 3, _args);
	}
}

UAFUNC(tagbuddy) {
UAALIA(tag)
UADESC(Mark a buddy with a reminder message)
UAAREQ(account,name)
UAAOPT(string,note)
	buddylist_t *blist = rgetlist(conn, args[0]);

	if (blist == NULL) {
		echof(conn, "TAGBUDDY", "<font color=\"#00FFFF\">%s</font> is not in your buddy list.\n",
			args[0]);
		return;
	}
	if (argc > 1) {
		STRREPLACE(blist->tag, args[1]);
		echof(conn, NULL, "<font color=\"#00FFFF\">%s</font> is now tagged (%s).\n",
			user_name(NULL, 0, conn, blist), blist->tag);
	} else if (blist->tag != NULL) {
		free(blist->tag);
		blist->tag = NULL;
		echof(conn, NULL, "<font color=\"#00FFFF\">%s</font> is no longer tagged.\n",
			user_name(NULL, 0, conn, blist));
	} else
		echof(conn, "TAGBUDDY", "<font color=\"#00FFFF\">%s</font> is not tagged.\n",
			user_name(NULL, 0, conn, blist));
}

UAFUNC(delbuddy) {
UADESC(Remove someone from your buddy list)
UAAOPT(account,name)
	buddylist_t *blist;
	const char *name;

	if (argc == 0)
		name = lastclose;
	else
		name = args[0];

	if (name == NULL) {
		echof(conn, "DELBUDDY", "No buddy specified.\n");
		return;
	}

	if (bgetwin(conn, name, BUDDY) != NULL) {
		echof(conn, "DELBUDDY", "You can not delete people from your tracking list with a window open. Please <font color=\"#00FF00\">/close %s</font> first.\n",
			name);
		return;
	}

	if ((blist = rgetlist(conn, name)) != NULL) {
		if (firetalk_im_remove_buddy(conn->conn, name) != FE_SUCCESS)
			rdelbuddy(conn, name);
		blist = NULL;
	} else if (firetalk_im_remove_buddy(conn->conn, name) == FE_SUCCESS)
		status_echof(conn, "Removed <font color=\"#00FFFF\">%s</font> from your session buddy list, but <font color=\"#00FFFF\">%s</font> isn't in naim's buddy list.\n",
			name, name);
	else
		status_echof(conn, "<font color=\"#00FFFF\">%s</font> is not in your buddy list.\n",
			name);

	assert(rgetlist(conn, name) == NULL);

	if ((argc == 0) && (lastclose != NULL)) {
		free(lastclose);
		lastclose = NULL;
	}
}

UAFUNC(op) {
UADESC(Give operator privilege)
UAAREQ(cmember,name)
UAWHER(INCHAT)
	fte_t	ret;

	if ((ret = firetalk_chat_op(conn->conn, conn->curbwin->winname, args[0])) != FE_SUCCESS)
		echof(conn, "OP", "Unable to op %s: %s.\n", args[0], firetalk_strerror(ret));
}

UAFUNC(deop) {
UADESC(Remove operator privilege)
UAAREQ(cmember,name)
UAWHER(INCHAT)
	fte_t	ret;

	if ((ret = firetalk_chat_deop(conn->conn, conn->curbwin->winname, args[0])) != FE_SUCCESS)
		echof(conn, "DEOP", "Unable to deop %s: %s.\n", args[0], firetalk_strerror(ret));
}

UAFUNC(topic) {
UADESC(View or change current chat topic)
UAAOPT(string,topic)
UAWHER(INCHAT)
	if (argc == 0) {
		if (conn->curbwin->blurb == NULL)
			echof(conn, NULL, "No topic set.\n");
		else
			echof(conn, NULL, "Topic for %s: </B><body>%s</body><B>.\n",
				conn->curbwin->winname,
				conn->curbwin->blurb);
	} else {
		fte_t	ret;

		if ((ret = firetalk_chat_set_topic(conn->conn, conn->curbwin->winname, args[0])) != FE_SUCCESS)
			echof(conn, "TOPIC", "Unable to change topic: %s.\n", firetalk_strerror(ret));
	}
}

UAFUNC(kick) {
UADESC(Temporarily remove someone from a chat)
UAAREQ(cmember,name)
UAAOPT(string,reason)
UAWHER(INCHAT)
	fte_t	ret;

	if ((ret = firetalk_chat_kick(conn->conn, conn->curbwin->winname, args[0], (argc == 2)?args[1]:conn->sn)) != FE_SUCCESS)
		echof(conn, "KICK", "Unable to kick %s: %s.\n", args[0], firetalk_strerror(ret));
}

UAFUNC(invite) {
UADESC(Invite someone to a chat)
UAAREQ(account,name)
UAAOPT(string,chat)
UAWHER(INCHAT)
	fte_t	ret;

	if ((ret = firetalk_chat_invite(conn->conn, conn->curbwin->winname, args[0],
		(argc == 2)?args[1]:"Join me in this Buddy Chat.")) != FE_SUCCESS)
		echof(conn, "INVITE", "Unable to invite %s: %s.\n", args[0], firetalk_strerror(ret));
}

#if 0
UAFUNC(help) {
UAALIA(about)
UADESC(Display topical help on using naim)
UAAOPT(string,topic)
	if (argc == 0)
		help_printhelp(NULL);
	else
		help_printhelp(args[0]);
}
#endif

UAFUNC(unblock) {
UAALIA(unignore)
UADESC(Remove someone from the ignore list)
UAAREQ(idiot,name)
	echof(conn, NULL, "No longer blocking <font color=\"#00FFFF\">%s</font>.\n", args[0]);
	if (conn->online > 0)
		if (firetalk_im_remove_deny(conn->conn, args[0]) != FE_SUCCESS)
			status_echof(conn, "Removed <font color=\"#00FFFF\">%s</font> from naim's block list, but the server wouldn't remove %s from your session block list.\n",
				args[0], args[0]);
	rdelidiot(conn, args[0]);
}

UAFUNC(block) {
UADESC(Server-enforced /ignore)
UAAREQ(account,name)
UAAOPT(string,reason)
	fte_t	ret;

	if ((ret = firetalk_im_add_deny(conn->conn, args[0])) != FE_SUCCESS)
		echof(conn, "BLOCK", "Unable to block %s: %s.\n", args[0], firetalk_strerror(ret));
}

UAFUNC(ignore) {
UADESC(Ignore all private/public messages)
UAAOPT(account,name)
UAAOPT(string,reason)
	if (argc == 0) {
		ignorelist_t *idiotar = conn->idiotar;

		if (idiotar == NULL)
			echof(conn, NULL, "Ignore list is empty.\n");
		else {
			echof(conn, NULL, "Ignore list:\n");
			do {
				if (idiotar->notes != NULL)
					echof(conn, NULL, "&nbsp; %s (%s)\n", idiotar->screenname, idiotar->notes);
				else
					echof(conn, NULL, "&nbsp; %s\n", idiotar->screenname);
			} while ((idiotar = idiotar->next) != NULL);
		}
	} else if (argc == 1) {
		if (args[0][0] == '-') {
			const char
				*newargs[] = { args[0]+1 };

			ua_unblock(conn, 1, newargs);
		} else {
			echof(conn, NULL, "Now ignoring <font color=\"#00FFFF\">%s</font>.\n", args[0]);
			raddidiot(conn, args[0], NULL);
		}
	} else if (strcasecmp(args[1], "block") == 0)
		ua_block(conn, 1, args);
	else {
		echof(conn, NULL, "Now ignoring <font color=\"#00FFFF\">%s</font> (%s).\n", args[0], args[1]);
		raddidiot(conn, args[0], args[1]);
	}
}

//extern void	(*ua_chains)();
UAFUNC(chains) {
UAALIA(tables)
UADESC(Manipulate data control tables)
UAAOPT(string,chain)
	char	buf[1024];
	chain_t	*chain;
	int	i;

	if (argc == 0) {
		const char *chains[] = { "preselect", "postselect", "periodic", "sendto",
			"proto_doinit", "proto_connected", "proto_connectfailed", "proto_nickchanged", "proto_buddy_nickchanged", "proto_warned", "proto_error_msg", "proto_disconnected", "proto_userinfo",
			"proto_buddyadded", "proto_buddyremoved", "proto_buddy_coming", "proto_buddy_going", "proto_buddy_away", "proto_buddy_unaway", "proto_buddy_idle", "proto_buddy_eviled", "proto_buddy_capschanged", "proto_buddy_typing",
			"proto_denyadded", "proto_denyremoved",
			"proto_recvfrom",
			"proto_chat_joined", "proto_chat_synched", "proto_chat_left", "proto_chat_oped", "proto_chat_deoped", "proto_chat_kicked", "proto_chat_invited",
			"proto_chat_user_joined", "proto_chat_user_left", "proto_chat_user_oped", "proto_chat_user_deoped", "proto_chat_user_kicked", "proto_chat_user_nickchanged",
			"proto_chat_topicchanged", "proto_chat_keychanged",
			"proto_file_offer", "proto_file_start", "proto_file_progress", "proto_file_finish", "proto_file_error",
		};

		for (i = 0; i < sizeof(chains)/sizeof(*chains); i++)
			ua_chains(conn, 1, chains+i);
		echof(conn, NULL, "See <font color=\"#00FF00\">/help chains</font> for more information.\n");
		return;
	}
	if (dl_self == NULL) {
		echof(conn, "TABLES", "Unable to perform self-symbol lookup.\n");
		return;
	}
	snprintf(buf, sizeof(buf), "chain_%s", args[0]);
	if ((chain = lt_dlsym(dl_self, buf)) == NULL) {
		echof(conn, "TABLES", "Unable to find chain %s (%s): %s.\n", 
			args[0], buf, lt_dlerror());
		return;
	}

	if (chain->count == 0)
		echof(conn, NULL, "No handler registered for chain %s.\n", args[0]);
	else if (chain->count == 1) {
		const char *modname, *hookname;

		if (chain->hooks[0].mod == NULL)
			modname = "core";
		else {
			const lt_dlinfo *dlinfo = lt_dlgetinfo(chain->hooks[0].mod);

			modname = dlinfo->name;
		}
		hookname = chain->hooks[0].name;
		if (*hookname == '_')
			hookname++;
		if ((strncmp(hookname, modname, strlen(modname)) == 0) && (hookname[strlen(modname)] == '_'))
			hookname += strlen(modname)+1;

		echof(conn, NULL, "Chain %s handled by <font color=\"#808080\"><font color=\"#FF0000\">%s</font>:<font color=\"#00FFFF\">%s</font>(%#p) weight <B>%i</B> at <B>%#p</B> (%lu passes, %lu stops)</font>\n",
			args[0], modname, hookname, chain->hooks[0].userdata, chain->hooks[0].weight, chain->hooks[0].func, chain->hooks[0].passes, chain->hooks[0].hits);
	} else {
		echof(conn, NULL, "Chain %s, containing %i hooks:\n", args[0], chain->count);
		for (i = 0; i < chain->count; i++) {
			const char *modname, *hookname;

			if (chain->hooks[i].mod == NULL)
				modname = "core";
			else {
				const lt_dlinfo *dlinfo = lt_dlgetinfo(chain->hooks[i].mod);

				modname = dlinfo->name;
			}
			hookname = chain->hooks[i].name;
			if (*hookname == '_')
				hookname++;
			if ((strncmp(hookname, modname, strlen(modname)) == 0) && (hookname[strlen(modname)] == '_'))
				hookname += strlen(modname)+1;
			echof(conn, NULL, " <font color=\"#808080\">%i: <font color=\"#FF0000\">%s</font>:<font color=\"#00FFFF\">%s</font>(%#p) weight <B>%i</B> at <B>%#p</B> (%lu passes, %lu stops)</font>\n",
				i, modname, hookname, chain->hooks[i].userdata, chain->hooks[i].weight, chain->hooks[i].func, chain->hooks[i].passes, chain->hooks[i].hits);
		}
	}
}

UAFUNC(filter) {
UADESC(Manipulate content filters)
UAAOPT(string,table)
UAAOPT(string,target)
UAAOPT(string,action)
	if (argc == 0) {
		echof(conn, NULL, "Current filter tables: REPLACE.\n");
		return;
	}

	if (strcasecmp(args[0], "REPLACE") == 0) {
		extern html_clean_t *html_cleanar;
		extern int html_cleanc;
		int	i;

		if (argc == 1) {
//			echof("REPLACE: Table commands: :FLUSH :LIST :APPEND :DELETE");
			if (html_cleanc > 0) {
				for (i = 0; i < html_cleanc; i++)
					if (*html_cleanar[i].from != 0)
						echof(conn, "FILTER REPLACE", "= %s -> %s\n",
							html_cleanar[i].from, html_cleanar[i].replace);
			} else
				echof(conn, "FILTER REPLACE", "Table empty.\n");
		} else if (argc == 2) {
			const char	*arg;
			char	action;

			switch (args[1][0]) {
			  case '-':
			  case '+':
			  case '=':
			  case ':':
				action = args[1][0];
				arg = args[1]+1;
				break;
			  default:
				action = '=';
				arg = args[1];
				break;
			}

			if (action == ':') {
				if (strcasecmp(arg, "FLUSH") == 0) {
					for (i = 0; i < html_cleanc; i++) {
						free(html_cleanar[i].from);
						html_cleanar[i].from = NULL;
						free(html_cleanar[i].replace);
						html_cleanar[i].replace = NULL;
					}
					free(html_cleanar);
					html_cleanar = NULL;
					html_cleanc = 0;
				} else
					echof(conn, "FILTER REPLACE", "Unknown action ``%s''.\n",
						arg);
			} else if (action == '+')
				echof(conn, "FILTER REPLACE", "Must specify rewrite rule.\n");
			else if ((action == '=') || (action == '-')) {
				for (i = 0; i < html_cleanc; i++)
					if (strcasecmp(html_cleanar[i].from, arg) == 0) {
						if (args[1][0] == '-') {
							echof(conn, "FILTER REPLACE", "- %s -> %s\n",
								html_cleanar[i].from, html_cleanar[i].replace);
							STRREPLACE(html_cleanar[i].from, "");
							STRREPLACE(html_cleanar[i].replace, "");
						} else
							echof(conn, "FILTER REPLACE", "= %s -> %s\n",
								html_cleanar[i].from, html_cleanar[i].replace);
						return;
					}
				echof(conn, "FILTER REPLACE", "%s is not in the table.\n",
					arg);
			} else
				echof(conn, "FILTER REPLACE", "Unknown modifier %c (%s).\n",
					action, arg);
		} else {
			for (i = 0; i < html_cleanc; i++)
				if (strcasecmp(html_cleanar[i].from, args[1]) == 0) {
					echof(conn, "FILTER REPLACE", "- %s -> %s\n",
						html_cleanar[i].from, html_cleanar[i].replace);
					break;
				}
			if (i == html_cleanc)
				for (i = 0; i < html_cleanc; i++)
					if (*html_cleanar[i].from == 0)
						break;
			if (i == html_cleanc) {
				html_cleanc++;
				html_cleanar = realloc(html_cleanar, html_cleanc*sizeof(*html_cleanar));
				html_cleanar[i].from = html_cleanar[i].replace = NULL;
			}
			STRREPLACE(html_cleanar[i].from, args[1]);
			STRREPLACE(html_cleanar[i].replace, args[2]);
			echof(conn, "FILTER REPLACE", "+ %s -> %s\n",
				html_cleanar[i].from, html_cleanar[i].replace);
		}
	} else
		echof(conn, "FILTER", "Table %s does not exist.",
			args[0]);
}

static html_clean_t ua_filter_defaultar[] = {
	{ "u",		"you"		},
	{ "ur",		"your"		},
	{ "lol",	"<grin>"	},
	{ "lawlz",	"<grin>"	},
	{ "lolz",	"<grin>"	},
	{ "r",		"are"		},
	{ "ru",		"are you"	},
	{ "some1",	"someone"	},
	{ "sum1",	"someone"	},
	{ "ne",		"any"		},
	{ "ne1",	"anyone"	},
	{ "im",		"I'm"		},
	{ "b4",		"before"	},
};

static void ua_filter_defaults(void) {
	int	i;

	for (i = 0; i < sizeof(ua_filter_defaultar)/sizeof(*ua_filter_defaultar); i++) {
		char	buf[1024];

		snprintf(buf, sizeof(buf), "filter replace %s %s", ua_filter_defaultar[i].from,
			ua_filter_defaultar[i].replace);
		ua_handlecmd(buf);
	}
}

UAFUNC(warn) {
UADESC(Send a warning about someone)
UAAREQ(account,name)
	fte_t	ret;

	if ((ret = firetalk_im_evil(conn->conn, args[0])) == FE_SUCCESS)
		echof(conn, NULL, "Eek, stay away, <font color=\"#00FFFF\">%s</font> is EVIL!\n", args[0]);
	else
		echof(conn, "WARN", "Unable to warn %s: %s.\n", args[0], firetalk_strerror(ret));
}

UAFUNC(nick) {
UADESC(Change or reformat your name)
UAAREQ(string,name)
	if (conn->online > 0) {
		fte_t	ret;

		if ((ret = firetalk_set_nickname(conn->conn, args[0])) != FE_SUCCESS)
			echof(conn, "NICK", "Unable to change names: %s.\n", firetalk_strerror(ret));
	} else
		echof(conn, "NICK", "Try <font color=\"#00FF00\">/connect %s</font>.\n", args[0]);
}

UAFUNC(echo) {
UADESC(Display something on the screen with $-variable expansion)
UAAREQ(string,script)
	echof(conn, NULL, "%s\n", script_expand(args[0]));
}

UAFUNC(readprofile) {
UADESC(Read your profile from disk)
UAAREQ(filename,filename)
	const char *filename = args[0];
	struct stat statbuf;
	size_t	len;
	int	pfd;

	if ((len = strlen(filename)) == 0) {
		echof(conn, "READPROFILE", "Please specify a real file.\n");
		return;
	}
	if (stat(filename, &statbuf) == -1) {
		echof(conn, "READPROFILE", "Can't read %s: %s.\n",
			filename, strerror(errno));
		return;
	}
	if ((len = statbuf.st_size) < 1) {
		echof(conn, "READPROFILE", "Profile file too small.\n");
		return;
	}
	if ((pfd = open(filename, O_RDONLY)) < 0) {
		echof(conn, "READPROFILE", "Unable to open %s.\n",
			filename);
		return;
	}
	free(conn->profile);
	if ((conn->profile = malloc(len+1)) == NULL) {
		echof(conn, "READPROFILE", "Fatal error in malloc(%i): %s\n",
			len+1, strerror(errno));
		return;
	}
	if (read(pfd, conn->profile, len) < len) {
		echof(conn, "READPROFILE", "Fatal error in read(%i): %s\n",
			pfd, strerror(errno));
		free(conn->profile);
		conn->profile = NULL;
		close(pfd);
		return;
	}
	close(pfd);
	conn->profile[len] = 0;
	if (conn->online > 0)
		naim_set_info(conn, conn->profile);
}

UAFUNC(accept) {
UADESC(EXPERIMENTAL Accept a file transfer request in the current window)
UAAREQ(window,filename)
UAWHER(NOTSTATUS)
	if (conn->curbwin->et != TRANSFER) {
		echof(conn, "ACCEPT", "You must be in a file transfer window.\n");
		return;
	}
	firetalk_file_accept(conn->conn, conn->curbwin->e.transfer->handle,
		conn->curbwin->e.transfer, args[0]);
	STRREPLACE(conn->curbwin->e.transfer->local, args[0]);
}

UAFUNC(offer) {
UADESC(EXPERIMENTAL Offer a file transfer request to someone)
UAAREQ(account,name)
UAAREQ(filename,filename)
	const char *from = args[0],
		*filename = args[1];

	if (bgetwin(conn, filename, TRANSFER) == NULL) {
		buddywin_t *bwin;

		bnewwin(conn, filename, TRANSFER);
		bwin = bgetwin(conn, filename, TRANSFER);
		assert(bwin != NULL);
		bwin->e.transfer = fnewtransfer(NULL, bwin, filename, from, -1);
		echof(conn, "OFFER", "Offering file transfer request to <font color=\"#00FFFF\">%s</font> (%s).\n",
			from, filename);
		firetalk_file_offer(conn->conn, from, filename, bwin->e.transfer);
	} else
		echof(conn, "OFFER", "Ignoring duplicate file transfer request to <font color=\"#00FFFF\">%s</font> (%s).\n",
			from, filename);
}

UAFUNC(setcol) {
UADESC(View or change display colors)
UAAOPT(string,eventname)
UAAOPT(string,colorname)
UAAOPT(string,colormodifier)
	int	i, col;

	if (argc == 0) {
		echof(conn, NULL, "Available colors include:\n");
		for (i = 0; i < sizeof(collist)/sizeof(*collist); i++)
			echof(conn, NULL, "&nbsp;%-2i&nbsp;%s\n", i, collist[i]);
		echof(conn, NULL, "Foregrounds:\n");
		for (i = 0; i < sizeof(forelist)/sizeof(*forelist); i++)
			echof(conn, NULL, "&nbsp;%-2i&nbsp;%s text is %s.\n", i, forelist[i],
				collist[faimconf.f[i]%(nw_COLORS*nw_COLORS)]);
		echof(conn, NULL, "Backgrounds:\n");
		for (i = 0; i < sizeof(backlist)/sizeof(*backlist); i++)
			echof(conn, NULL, "&nbsp;%-2i&nbsp;%s window is %s.\n", i, backlist[i],
				collist[faimconf.b[i]]);
		return;
	} else if (argc == 1) {
		echof(conn, "SETCOL", "Please see <font color=\"#00FF00\">/help setcol</font>.\n");
		return;
	}

	if (args[1][0] == '0')
		col = 0;
	else if ((col = atoi(args[1])) == 0) {
		for (i = 0; i < sizeof(collist)/sizeof(*collist); i++)
			if (strncasecmp(collist[i], args[1], strlen(args[1])) == 0)
				break;
		if (i < sizeof(collist)/sizeof(*collist))
			col = i;
	}

	for (i = 0; i < sizeof(forelist)/sizeof(*forelist); i++)
		if (strcasecmp(forelist[i], args[0]) == 0)
			break;
	if (i < sizeof(forelist)/sizeof(*forelist)) {
		if (argc >= 3) {
			if (strcasecmp(args[2], "BOLD") == 0)
				col += 2*(nw_COLORS*nw_COLORS);
			else if (strcasecmp(args[2], "DULL") == 0)
				col += (nw_COLORS*nw_COLORS);
			else {
				echof(conn, "SETCOL", "Unknown modifier %s.\n",
					args[2]);
				return;
			}
		}

		if (faimconf.f[i] != col) {
			faimconf.f[i] = col;
			echof(conn, NULL, "Foreground %s set to %s.\n", forelist[i],
				collist[col%(nw_COLORS*nw_COLORS)]);
		}
	} else {
		for (i = 0; i < sizeof(backlist)/sizeof(*backlist); i++)
			if (strcasecmp(backlist[i], args[0]) == 0)
				break;
		if (i < sizeof(backlist)/sizeof(*backlist)) {
			if (argc >= 3) {
				echof(conn, "SETCOL", "You can not force bold/dull for backgrounds.\n");
				return;
			}

			if (faimconf.b[i] != col) {
				conn_t	*c = conn;

				faimconf.b[i] = col;
				if ((i == cINPUT) || (i == cSTATUSBAR))
					nw_flood(&win_info, CB(STATUSBAR,INPUT));

				if (i == cINPUT)
					nw_flood(&win_input, (COLORS*col + faimconf.f[cTEXT]%(nw_COLORS*nw_COLORS)));
				else if (i == cWINLIST)
					nw_flood(&win_buddy, (COLORS*col + faimconf.f[cBUDDY]%(nw_COLORS*nw_COLORS)));
				else if ((i != cWINLISTHIGHLIGHT) && (i != cSTATUSBAR))
				  do {
					if (i == cCONN)
						nw_flood(&(conn->nwin), (COLORS*col + faimconf.f[cTEXT]%(nw_COLORS*nw_COLORS)));
					else if (c->curbwin != NULL) {
						buddywin_t	*bwin = c->curbwin;

						do {
							nw_flood(&(bwin->nwin), (COLORS*col + faimconf.f[cTEXT]%(nw_COLORS*nw_COLORS)));
							bwin->nwin.dirty = 1;
						} while ((bwin = bwin->next) != c->curbwin);
					}
				  } while ((c = c->next) != conn);
				echof(conn, NULL, "Background %s set to %s.\n", backlist[i],
					collist[col]);
			}
		} else
			echof(conn, "SETCOL", "Unknown window/event: %s.\n",
				args[0]);
	}
}

UAFUNC(setpriv) {
UADESC(Change your privacy mode)
UAAREQ(string,mode)
	fte_t	ret;

	if ((ret = firetalk_set_privacy(conn->conn, args[0])) == FE_SUCCESS)
		echof(conn, NULL, "Privacy mode changed.\n");
	else
		echof(conn, "SETPRIV", "Privacy mode not changed: %s.\n", firetalk_strerror(ret));
}

UAFUNC(bind) {
UADESC(View or change key bindings)
UAAOPT(string,keyname)
UAAOPT(string,script)
	if (argc == 0)
		conio_bind_list();
	else if (argc == 1)
		conio_bind_echo(conn, args[0]);
	else
		conio_bind_doset(conn, args[0], args[1]);
}

UAFUNC(alias) {
UADESC(Create a new command alias)
UAAREQ(string,commandname)
UAAREQ(string,script)
	alias_makealias(args[0], args[1]);
	echof(conn, NULL, "Aliased <font color=\"#00FF00\">/%s</font> to: %s\n", args[0], args[1]);
}

UAFUNC(set) {
UADESC(View or change the value of a configuration or session variable; see /help settings)
UAAOPT(varname,varname)
UAAOPT(string,value)
UAAOPT(string,dummy)
	if (argc == 0)
		set_setvar(NULL, NULL);
	else if (argc == 1)
		set_setvar(args[0], NULL);
	else if (argc == 2)
		set_setvar(args[0], args[1]);
	else
		echof(conn, "SET", "Try <font color=\"#00FF00\">/set %s \"%s %s\"</font>.\n",
			args[0], args[1], args[2]);
}

static void ua_listprotocols(conn_t *conn, const char *prefix) {
	int	i;
	const char *str;

	echof(conn, prefix, "Protocol drivers are currently loaded for:\n");
	for (i = 0; (str = firetalk_strprotocol(i)) != NULL; i++)
		echof(conn, prefix, "&nbsp; %s\n", str);
}

UAFUNC(newconn) {
UADESC(Open a new connection window)
UAAOPT(string,label)
UAAOPT(string,protocol)
	int	i, proto;
	conn_t	*newconn = curconn;
	const char *protostr;

	if (argc == 0) {
		ua_listprotocols(conn, NULL);
		echof(conn, NULL, "See <font color=\"#00FF00\">/help newconn</font> for more help.\n");
		return;
	} else if (argc == 1) {
		if ((strcasecmp(args[0], "Undernet") == 0)
			|| (strcasecmp(args[0], "EFnet") == 0))
			protostr = "IRC";
		else if ((strcasecmp(args[0], "AIM") == 0)
			|| (strcasecmp(args[0], "ICQ") == 0))
			protostr = "TOC2";
		else if (strcasecmp(args[0], "lily") == 0)
			protostr = "SLCP";
		else {
			echof(conn, "NEWCONN", "Please supply a protocol name:");
			ua_listprotocols(conn, "NEWCONN");
			echof(conn, "NEWCONN", "See <font color=\"#00FF00\">/help newconn</font> for more help.\n");
			return;
		}
	} else {
		if ((strcasecmp(args[1], "AIM") == 0)
			|| (strcasecmp(args[1], "AIM/TOC") == 0)
			|| (strcasecmp(args[1], "AIM/TOC2") == 0)
			|| (strcasecmp(args[1], "ICQ") == 0)
			|| (strcasecmp(args[1], "ICQ/TOC") == 0)
			|| (strcasecmp(args[1], "ICQ/TOC2") == 0)
			|| (strcasecmp(args[1], "TOC") == 0))
			protostr = "TOC2";
		else if (strcasecmp(args[1], "Lily") == 0)
			protostr = "SLCP";
		else
			protostr = args[1];
	}

	if ((proto = firetalk_find_protocol(protostr)) == -1) {
		echof(conn, "NEWCONN", "Invalid protocol %s.", protostr);
		ua_listprotocols(conn, "NEWCONN");
		echof(conn, "NEWCONN", "See <font color=\"#00FF00\">/help newconn</font> for more help.\n");
		return;
	}

	if (newconn != NULL)
		do {
			if (strcasecmp(args[0], newconn->winname) == 0) {
				echof(conn, "NEWCONN", "A window for connection %s (%s) is already open.\n",
					newconn->winname, firetalk_strprotocol(newconn->proto));
				return;
			}
		} while ((newconn = newconn->next) != curconn);

	newconn = naim_newconn(proto);
	assert(newconn != NULL);

	nw_newwin(&(newconn->nwin), faimconf.wstatus.pady, faimconf.wstatus.widthx);
	nw_initwin(&(newconn->nwin), cCONN);
	nw_erase(&(newconn->nwin));
	for (i = 0; i < faimconf.wstatus.pady; i++)
		nw_printf(&(newconn->nwin), 0, 0, "\n");
	if (curconn == NULL)
		newconn->next = newconn;
	else {
		newconn->next = curconn->next;
		curconn->next = newconn;
	}
	curconn = newconn;
	STRREPLACE(newconn->winname, args[0]);
	if (newconn->next != newconn) {
		echof(newconn, NULL, "A new connection of type %s has been created.\n",
			firetalk_strprotocol(proto));
		echof(newconn, NULL, "Ins and Del will switch between connections, and /jump (^N, M-n, F8) will jump across connections if there are no active windows in the current one.\n");
		echof(newconn, NULL, "You can now <font color=\"#00FF00\">/connect &lt;name&gt; [&lt;server&gt;]</font> to log on.\n");
	}
	bupdate();

	script_hook_newconn(newconn);
}

UAFUNC(delconn) {
UADESC(Close a connection window)
UAAOPT(string,label)
	if (curconn->next == curconn) {
		echof(conn, "DELCONN", "You must always have at least one connection open at all times.\n");
		return;
	}

	if (argc > 0) {
		int	found = 0;

		do {
			if (strcasecmp(args[0], conn->winname) == 0) {
				found = 1;
				break;
			}
		} while ((conn = conn->next) != curconn);

		if (found == 0) {
			echof(conn, "DELCONN", "Unable to find connection %s.\n",
				args[0]);
			return;
		}
	}

	do_delconn(conn);
}

UAFUNC(server) {
UADESC(Connect to a service)
UAAOPT(string,server)
UAAOPT(int,port)
	const char *na[3];

	if (argc == 0) {
		if (conn->port != 0)
			echof(conn, NULL, "Current server: %s %i\n",
				(conn->server == NULL)?"(default host)":conn->server,
				conn->port);
		else
			echof(conn, NULL, "Current server: %s (default port)\n",
				(conn->server == NULL)?"(default host)":conn->server);
		return;
	}
	if (conn->sn == NULL) {
		echof(conn, "SERVER", "Please try to <font color=\"#00FF00\">/%s:connect</font> first.\n",
			conn->winname);
		return;
	}
	na[0] = conn->sn;
	na[1] = args[0];
	if (argc == 2);
		na[2] = args[1];
	ua_connect(conn, argc+1, na);
}

UAFUNC(disconnect) {
UADESC(Disconnect from a server)
	if (conn->online <= 0)
		echof(conn, "DISCONNECT", "You aren't connected.\n");
	else if (firetalk_disconnect(conn->conn) == FE_SUCCESS) {
		bclearall(conn, 0);
		echof(conn, NULL, "You are now disconnected.\n");
	}
	conn->online = 0;
}

UAFUNC(winlist) {
UADESC(Switch the window list window between always visible or always hidden or auto-hidden)
UAAOPT(string,visibility)
	if (argc == 0) {
		if (changetime == -1)
			echof(conn, NULL, "Window list window is always hidden (possible values are HIDDEN, VISIBLE, or AUTO).\n");
		else if (changetime == 0)
			echof(conn, NULL, "Window list window is always visible (possible values are HIDDEN, VISIBLE, or AUTO).\n");
		else
			echof(conn, NULL, "Window list window is auto-hidden (possible values are HIDDEN, VISIBLE, or AUTO).\n");
	} else {
		if (strncasecmp(args[0], "hid", sizeof("hid")-1) == 0) {
			if (changetime != -1) {
				echof(conn, NULL, "Window list window is now always hidden.\n");
				changetime = -1;
			}
		} else if (strncasecmp(args[0], "vis", sizeof("vis")-1) == 0) {
			if (changetime != 0) {
				echof(conn, NULL, "Window list window is now always visible.\n");
				changetime = 0;
			}
		} else if ((changetime == -1) || (changetime == 0)) {
			echof(conn, NULL, "Window list window is now auto-hidden.\n");
			changetime = nowf;
		} else
			echof(conn, "WINLIST", "Hmm? Possible values are HIDDEN, VISIBLE, or AUTO.\n");
	}
}

#ifdef HAVE_WORKING_FORK
typedef struct {
	int	fd, sayit;
	conn_t	*conn;
} execstub_t;

static int exec_preselect(void *userdata, const char *signature, fd_set *rfd, fd_set *wfd, fd_set *efd, int *maxfd) {
	execstub_t *execstub = (execstub_t *)userdata;

	if (*maxfd <= execstub->fd)
		*maxfd = execstub->fd+1;
	FD_SET(execstub->fd, rfd);
	FD_SET(execstub->fd, efd);
	return(HOOK_CONTINUE);
}

static int exec_read(int fd, int sayit, conn_t *conn) {
	char	buf[1024], *ptr, *n;
	int	i, buflen = sizeof(buf);

	i = read(fd, buf, buflen-1);
	if (i == 0) {
		close(fd);
		return(-1);
	}
	buf[i] = 0;
	ptr = buf;
	while ((n = strchr(ptr, '\n')) != NULL) {
		*n = 0;
		if (*(n-1) == '\r')
			*(n-1) = 0;
		if (!sayit || (conn->curbwin == NULL))
			echof(conn, "_", "%s\n", ptr);
		else {
			char	buf2[1024];

			snprintf(buf2, sizeof(buf2), "/m \"%s\" %s",
				conn->curbwin->winname, ptr);
			ua_handleline(buf2);
		}
		ptr = n+1;
	}
	if (*ptr != 0) {
		if (!sayit || (conn->curbwin == NULL))
			echof(conn, "_", "%s\n", ptr);
		else {
			char	buf2[1024];

			snprintf(buf2, sizeof(buf2), "/m \"%s\" %s",
				conn->curbwin->winname, ptr);
			ua_handleline(buf2);
		}
	}
	return(0);
}

static int exec_postselect(void *userdata, const char *signature, fd_set *rfd, fd_set *wfd, fd_set *efd) {
	execstub_t *execstub = (execstub_t *)userdata;

	if (FD_ISSET(execstub->fd, rfd))
		if (exec_read(execstub->fd, execstub->sayit, execstub->conn) != 0) {
			void	*mod = NULL;

			HOOK_DEL(preselect, mod, exec_preselect, execstub);
			HOOK_DEL(postselect, mod, exec_postselect, execstub);
			free(execstub);
		}
	return(HOOK_CONTINUE);
}

UAFUNC(exec) {
UADESC(Execute a shell command; as in /exec -o uname -a)
UAAREQ(string,command)
	int	sayit = (strncmp(args[0], "-o ", 3) == 0),
		pi[2];
	pid_t	pid;

	if (pipe(pi) != 0) {
		echof(conn, "EXEC", "Error creating pipe: %s.\n", strerror(errno));
		return;
	}
	if ((pid = fork()) == -1) {
		echof(conn, "EXEC", "Error in fork: %s, closing pipe.\n", strerror(errno));
		close(pi[0]);
		close(pi[1]);
	} else if (pid > 0) {
		void	*mod = NULL;
		execstub_t *execstub;

		close(pi[1]);
		if ((execstub = calloc(1, sizeof(*execstub))) == NULL)
			abort();
		execstub->fd = pi[0];
		execstub->sayit = sayit;
		execstub->conn = conn;
		HOOK_ADD(preselect, mod, exec_preselect, 100, execstub);
		HOOK_ADD(postselect, mod, exec_postselect, 100, execstub);
	} else {
		char	*exec_args[] = { "/bin/sh", "-c", NULL, NULL };

		close(pi[0]);
		close(STDIN_FILENO);
		dup2(pi[1], STDOUT_FILENO);
		dup2(pi[1], STDERR_FILENO);
		if (sayit)
			exec_args[2] = strdup(args[0]+3);
		else
			exec_args[2] = strdup(args[0]);
		execvp(exec_args[0], exec_args);
		/* NOTREACHED */
		printf("%s\n", strerror(errno));
		free(exec_args[2]);
		exit(0);
	}
}
#endif

#ifdef ALLOW_DETACH
UAFUNC(detach) {
UADESC(Disconnect from the current session)
	if (sty == NULL)
		echof(conn, "DETACH", "You can only <font color=\"#00FF00\">/detach</font> when naim is run under screen.\n");
	else {
		echof(conn, NULL, "Type ``screen -r'' to re-attach.\n");
		statrefresh();
		doupdate();
		kill(getppid(), SIGHUP);
		echof(conn, NULL, "Welcome back to naim.\n");
	}
}
#endif


static int modlist_filehelper(const char *path, lt_ptr data) {
	conn_t	*conn = (conn_t *)data;
	const char *filename = naim_basename(path);

	if (strncmp(filename, "lib", 3) == 0)
		return(0);
	if (strstr(path, "/naim/") == NULL)
		return(0);
	echof(conn, NULL, "&nbsp; <font color=\"#FF0000\">%s</font> (<font color=\"#808080\">%s</font>)\n", filename, path);
	return(0);
}

static int modlist_helper(lt_dlhandle mod, lt_ptr data) {
	conn_t	*conn = (conn_t *)data;
	const lt_dlinfo *dlinfo = lt_dlgetinfo(mod);
	char	*tmp;
	double	*ver;

	if (dlinfo->ref_count > 0) {
		assert(dlinfo->ref_count == 1);
		echof(conn, NULL, "&nbsp; <font color=\"#FF0000\">%s</font> (<font color=\"#808080\">%s</font>)\n",
			dlinfo->name, dlinfo->filename);
		if ((tmp = lt_dlsym(mod, "module_category")) != NULL)
			echof(conn, NULL, "&nbsp; &nbsp; &nbsp; &nbsp;Category: <font color=\"#FFFFFF\">%s</font>\n", tmp);
		if ((tmp = lt_dlsym(mod, "module_description")) != NULL)
			echof(conn, NULL, "&nbsp; &nbsp; Description: <font color=\"#FFFFFF\">%s</font>\n", tmp);
		if ((tmp = lt_dlsym(mod, "module_license")) != NULL)
			echof(conn, NULL, "&nbsp; &nbsp; &nbsp; &nbsp; License: <font color=\"#FFFFFF\">%s</font>\n", tmp);
		if ((tmp = lt_dlsym(mod, "module_author")) != NULL)
			echof(conn, NULL, "&nbsp; &nbsp; &nbsp; &nbsp; &nbsp;Author: <font color=\"#FFFFFF\">%s</font>\n", tmp);
		if (((ver = lt_dlsym(mod, "module_version")) != NULL) && (*ver >= 1.0))
			echof(conn, NULL, "&nbsp; &nbsp; &nbsp; &nbsp;Code Age: <font color=\"#FFFFFF\">%s</font>\n", dtime(nowf - *ver));
	}

	return(0);
}

UAFUNC(dlsym) {
UAAREQ(string,symbol)
	lt_ptr	ptr;

	ptr = lt_dlsym(dl_self, args[0]);
	if (ptr != NULL)
		echof(conn, NULL, "Symbol %s found at %#p.\n", args[0], ptr);
	else
		echof(conn, "DLSYM", "Symbol %s not found.\n", args[0]);
}

UAFUNC(modlist) {
UADESC(Search for and list all potential and resident naim modules)
	echof(conn, NULL, "Modules found in the default search path:\n");
	lt_dlforeachfile(NULL, modlist_filehelper, conn);
	echof(conn, NULL, "Additional modules can be loaded using their explicit paths, as in <font color=\"#00FF00\">/modload %s/mods/mymod.la</font>.\n",
		home);
	echof(conn, NULL, "Modules currently resident:\n");
	lt_dlforeach(modlist_helper, conn);
	echof(conn, NULL, "See <font color=\"#00FF00\">/help modload</font> for more information.\n");
}

UAFUNC(modload) {
UADESC(Load and initialize a dynamic module)
UAAREQ(filename,module)
UAAOPT(string,options)
	lt_dlhandle mod;
	const lt_dlinfo *dlinfo;
	int	(*naim_init)(lt_dlhandle mod, const char *str);
	const char *options;

	if (argc > 1)
		options = args[1];
	else
		options = NULL;

	mod = lt_dlopenext(args[0]);
	if (mod == NULL) {
		echof(conn, "MODLOAD", "Unable to load module <font color=\"#FF0000\">%s</font>: %s.\n",
			args[0], lt_dlerror());
		return;
	}
	dlinfo = lt_dlgetinfo(mod);
	assert(dlinfo->ref_count > 0);
	if (dlinfo->ref_count > 1) {
		echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> (<font color=\"#808080\">%s</font>) already loaded.\n",
			dlinfo->name, dlinfo->filename);
		lt_dlclose(mod);
		return;
	}
	echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> (<font color=\"#808080\">%s</font>) loaded.\n",
		dlinfo->name, dlinfo->filename);

	naim_init = lt_dlsym(mod, "naim_init");
	if (naim_init != NULL) {
		if (naim_init(mod, options) != MOD_FINISHED)
			echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> initialized, leaving resident.\n",
				dlinfo->name);
		else {
			echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> initialized, unloading.\n",
				dlinfo->name);
			echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> (<font color=\"#808080\">%s</font>) unloaded.\n",
				dlinfo->name, dlinfo->filename);
			lt_dlclose(mod);
		}
	} else
		echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> has no initializer (<font color=\"#FF0000\">%s</font>:naim_init()): %s, leaving resident.\n",
			dlinfo->name, args[0], lt_dlerror());
}

UAFUNC(modunload) {
UADESC(Deinitialize and unload a resident module)
UAAREQ(string,module)
	lt_dlhandle mod;

	for (mod = lt_dlhandle_next(NULL); mod != NULL; mod = lt_dlhandle_next(mod)) {
		const lt_dlinfo *dlinfo = lt_dlgetinfo(mod);

		if ((dlinfo->name != NULL) && (strcasecmp(args[0], dlinfo->name) == 0)) {
			int	(*naim_exit)(lt_dlhandle mod, const char *str);

			naim_exit = lt_dlsym(mod, "naim_exit");
			if (naim_exit != NULL) {
				if (naim_exit(mod, args[0]) != MOD_FINISHED) {
					echof(conn, "MODUNLOAD", "Module <font color=\"#FF0000\">%s</font> is busy (<font color=\"#FF0000\">%s</font>:naim_exit() != MOD_FINISHED), leaving resident.\n",
						dlinfo->name, dlinfo->name);
					return;
				} else
					echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> deinitialized, unloading.\n",
						dlinfo->name);
			}
			echof(conn, NULL, "Module <font color=\"#FF0000\">%s</font> (<font color=\"#808080\">%s</font>) unloaded.\n",
				dlinfo->name, dlinfo->filename);
			lt_dlclose(mod);
			return;
		}
	}
	echof(conn, "MODUNLOAD", "No module named %s loaded.\n", args[0]);
}

UAFUNC(resize) {
UADESC(Resize all windows)
UAAOPT(int,height)
	char	buf[20];
	int	scrollback;

	if (argc == 0)
		scrollback = script_getvar_int("scrollback");
	else
		scrollback = atoi(args[0]);

	if (scrollback < (LINES-1)) {
		echof(conn, "RESIZE", "Window height (%i) must be greater than %i.", scrollback, LINES-2);
		scrollback = LINES-1;
	} else if (scrollback > 5000) {
		echof(conn, "RESIZE", "Window height (%i) must be less than 5001.", scrollback);
		scrollback = 5000;
	}

	snprintf(buf, sizeof(buf), "%i", scrollback);
	script_setvar("scrollback", buf);

	faimconf.wstatus.pady = scrollback;
	win_resize();
	echof(conn, NULL, "Windows resized.");
}

//extern void	(*ua_status)();
UAFUNC(status) {
UADESC(Connection status report)
UAAOPT(string,connection)
	buddywin_t *bwin;
	conn_t	*c;
	int	discussions = 0,
		users = 0,
		files = 0;

	if (argc >= 0) {
		extern char naimrcfilename[];
		extern time_t startuptime;

		echof(conn, NULL, "Running " PACKAGE_STRING NAIM_SNAPSHOT " for %s.\n",
			dtime(now - startuptime));
		echof(conn, NULL, "Config file: %s\n", naimrcfilename);
	}

	if (argc == 0) {
		const char *args[] = { conn->winname };

		ua_status(conn, -1, args);
		for (c = conn->next; c != conn; c = c->next) {
			args[0] = c->winname;
			echof(conn, NULL, "-");
			ua_status(conn, -1, args);
		}
		echof(conn, NULL, "See <font color=\"#00FF00\">/help status</font> for more information.\n");
		return;
	}
	if (strcasecmp(args[0], conn->winname) != 0) {
		for (c = conn->next; c != conn; c = c->next)
			if (strcasecmp(args[0], c->winname) == 0)
				break;
		if (c == conn) {
			echof(conn, "STATUS", "Unknown connection %s.\n", args[0]);
			return;
		}
	} else
		c = conn;

	echof(conn, NULL, "Information for %s:\n", c->winname);
	if (c->online > 0) {
		assert(c->sn != NULL);
		echof(conn, NULL, "Online as %s for %s.\n", c->sn, dtime(now - c->online));
	} else {
		if (c->sn != NULL)
			echof(conn, NULL, "Not online (want to be %s)\n", c->sn);
		else
			echof(conn, NULL, "Not online.\n");
	}

	if ((bwin = c->curbwin) != NULL) {
	  echof(conn, NULL, "Windows:\n");
	  do {
		char	*type = NULL,
			*close;

		if (bwin->keepafterso)
			close = " OPEN_INDEFINITELY";
		else
			close = " QUICK_CLOSE";
		switch (bwin->et) {
		  case CHAT:
			discussions++;
			type = "DISCUSSION_WINDOW";
			break;
		  case BUDDY:
			users++;
			type = "USER_WINDOW";
			if (!USER_PERMANENT(bwin->e.buddy))
				close = " SLOW_CLOSE";
			break;
		  case TRANSFER:
			files++;
			type = "TRANSFER_WINDOW";
			break;
		}
		if ((bwin->viewtime > 0.0) && ((nowf - bwin->viewtime) > 2.0))
			echof(conn, NULL, "&nbsp; <font color=\"#00FFFF\">%s</font> (%s%s%s%s) [last viewed %s]\n", bwin->winname,
				type, close,
				bwin->waiting?" LIVE_MESSAGES_WAITING":"",
				bwin->pouncec?" QUEUED_MESSAGES_WAITING":"",
				dtime(nowf - bwin->viewtime));
		else
			echof(conn, NULL, "&nbsp; <font color=\"#00FFFF\">%s</font> (%s%s%s%s)\n", bwin->winname,
				type, close,
				bwin->waiting?" LIVE_MESSAGES_WAITING":"",
				bwin->pouncec?" QUEUED_MESSAGES_WAITING":"");
	  } while ((bwin = bwin->next) != c->curbwin);
	}
	echof(conn, NULL, "%i discussions, %i users, %i file transfers.\n", discussions, users, files);
	if (c == conn)
		echof(conn, NULL, "See <font color=\"#00FF00\">/names</font> for buddy list information.\n");
	else
		echof(conn, NULL, "See <font color=\"#00FF00\">/%s:names</font> for buddy list information.\n", c->winname);
}


static const char *ua_valid_where(const cmdar_t *cmd, conn_t *conn) {
	if ((cmd->where != C_STATUS) && (cmd->where != C_ANYWHERE) && !inconn) {
		if (cmd->where == C_INCHAT)
			return("You must be in a chat.");
		else if (cmd->where == C_INUSER)
			return("You must be in a query.");
		else
			return("You can not be in status.");
	} else if ((cmd->where == C_STATUS) && inconn) {
		assert(curconn->curbwin != NULL);
		return("You must be in status.");
	} else if ((cmd->where == C_INUSER) && (conn->curbwin->et == CHAT))
		return("You must be in a query.");
	else if ((cmd->where == C_INCHAT) && (conn->curbwin->et != CHAT))
		return("You must be in a chat.");
	return(NULL);
}

static const char *ua_valid_args(const cmdar_t *cmd, const int argc) {
	static char buf[1024];

	if (argc < cmd->minarg) {
		snprintf(buf, sizeof(buf), "Command requires at least %i arguments.", cmd->minarg);
		return(buf);
	}
	if (argc > cmd->maxarg) {
		snprintf(buf, sizeof(buf), "Command requires at most %i arguments.", cmd->maxarg);
		return(buf);
	}
	return(NULL);
}

const cmdar_t *ua_find_cmd(const char *cmd) {
	int	i;

	for (i = 0; i < cmdc; i++)
		if (strcasecmp(cmdar[i].c, cmd) == 0)
			break;
		else {
			int	j;

			for (j = 0; cmdar[i].aliases[j] != NULL; j++)
				if (strcasecmp(cmdar[i].aliases[j], cmd) == 0)
					break;
			if (cmdar[i].aliases[j] != NULL)
				break;
		}
	if (i < cmdc)
		return(&(cmdar[i]));
	return(NULL);
}

const cmdar_t *ua_findn_cmd(const char *cmd, const int len) {
	int	i;

	for (i = 0; i < cmdc; i++)
		if (strncasecmp(cmdar[i].c, cmd, len) == 0)
			break;
		else {
			int	j;

			for (j = 0; cmdar[i].aliases[j] != NULL; j++)
				if (strncasecmp(cmdar[i].aliases[j], cmd, len) == 0)
					break;
			if (cmdar[i].aliases[j] != NULL)
				break;
		}
	if (i < cmdc)
		return(&(cmdar[i]));
	return(NULL);
}

const char *ua_valid(const char *cmd, conn_t *conn, const int argc) {
	const char *error;
	const cmdar_t *com;

	if ((com = ua_find_cmd(cmd)) == NULL)
		return("Unknown command.");
	if ((error = ua_valid_where(com, conn)) != NULL)
		return(error);
	if ((error = ua_valid_args(com, argc)) != NULL)
		return(error);
	return(NULL);
}

void	ua_handlecmd(const char *buf) {
	conn_t	*c = NULL;
	char	line[1024], *cmd, *arg, *tmp;
	int	builtinonly = 0;

	assert(buf != NULL);

	while (isspace(*buf))
		buf++;
	if (*buf == '/')
		buf++;
	if (*buf == '/') {
		builtinonly = 1;
		buf++;
	}

	if ((*buf == 0) || isspace(*buf) || (*buf == '/'))
		return;

	strncpy(line, buf, sizeof(line)-1);
	line[sizeof(line)-1] = 0;

	while ((strlen(line) > 0) && isspace(line[strlen(line)-1]))
		line[strlen(line)-1] = 0;

	if (*line == 0)
		return;

	cmd = atom(line);
	arg = firstwhite(line);

	if ((tmp = strchr(cmd, ':')) != NULL) {
		conn_t	*conn = curconn;

		*tmp = 0;
		if (conn != NULL)
			do {
				if (strcasecmp(cmd, conn->winname) == 0) {
					c = conn;
					break;
				}
			} while ((conn = conn->next) != curconn);
		if (c == NULL) {
			echof(curconn, NULL, "[%s:%s] Unknown connection.\n", cmd, tmp+1);
			return;
		}
		cmd = tmp+1;
	} else
		c = curconn;

	if (!builtinonly) {
		if (alias_doalias(cmd, arg))
			return;
	}

	script_cmd(c, cmd, arg);
}

void	(*script_client_cmdhandler)(const char *) = ua_handlecmd;

void	ua_handleline(const char *line) {
	if (*line == '/')
		ua_handlecmd(line);
	else if (!inconn)
		ua_handlecmd(line);
	else {
		const char *args[] = { NULL, line };

		curconn->curbwin->keepafterso = 1;
		ua_msg(curconn, 2, args);
	}
}

static const char *n_strnrchr(const char *const str, const char c, const int len) {
	const char *s = str+len;

	while (s >= str) {
		if (*s == c)
			return(s);
		s--;
	}
	return(NULL);
}

static const char *window_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) {
	static char str[1024];
	buddywin_t *bwin;
	size_t	startlen = strlen(start);

	if ((bwin = conn->curbwin) == NULL)
		return(NULL);
	bwin = bwin->next;
	do {
		if (aimncmp(bwin->winname, start, startlen) == 0) {
			char	*name = bwin->winname;
			int	j;

			if (match != NULL)
				*match = bufloc - (start-buf);
			if (desc != NULL) {
				if (bwin->et == BUDDY)
					*desc = bwin->e.buddy->_name;
				else
					*desc = NULL;
			}

			for (j = 0; (j < sizeof(str)-1) && (*name != 0); j++) {
				while (*name == ' ')
					name++;
				str[j] = *(name++);
			}
			str[j] = 0;
			return(str);
		}
		if ((bwin->et == BUDDY) && (bwin->e.buddy->_name != NULL) && (aimncmp(bwin->e.buddy->_name, start, startlen) == 0)) {
			char	*name = bwin->e.buddy->_name;
			int	j;

			if (match != NULL)
				*match = bufloc - (start-buf);
			if (desc != NULL)
				*desc = bwin->winname;

			for (j = 0; (j < sizeof(str)-1) && (*name != 0); j++) {
				while (*name == ' ')
					name++;
				str[j] = *(name++);
			}
			str[j] = 0;
			return(str);
		}
	} while ((bwin = bwin->next) != conn->curbwin->next);

	return(NULL);
}

static const char *chat_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) {
	static char str[1024];
	buddywin_t *bwin;
	size_t	startlen = strlen(start);

	if ((bwin = conn->curbwin) == NULL)
		return(NULL);
	bwin = bwin->next;
	do {
		if ((bwin->et == CHAT) && (aimncmp(bwin->winname, start, startlen) == 0)) {
			char	*name = bwin->winname;
			int	j;

			if (match != NULL)
				*match = bufloc - (start-buf);
			if (desc != NULL)
				*desc = NULL;

			for (j = 0; (j < sizeof(str)-1) && (*name != 0); j++) {
				while (*name == ' ')
					name++;
				str[j] = *(name++);
			}
			str[j] = 0;
			return(str);
		}
	} while ((bwin = bwin->next) != conn->curbwin->next);

	return(NULL);
}

static const char *cmember_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) {
	static char str[1024];
	size_t	startlen = strlen(start);
	int	i;

	if (!inconn || (conn->curbwin->et != CHAT))
		return(NULL);
	naim_chat_listmembers(conn, conn->curbwin->winname);
	if (names == NULL)
		return(NULL);
	for (i = 0; i < namec; i++)
		if (aimncmp(names[i], start, startlen) == 0) {
			char	*name = names[i];
			int	j;

			if (match != NULL)
				*match = bufloc - (start-buf);
			if (desc != NULL)
				*desc = NULL;

			for (j = 0; (j < sizeof(str)-1) && (*name != 0); j++) {
				while (*name == ' ')
					name++;
				str[j] = *(name++);
			}
			str[j] = 0;
			free(names);
			names = NULL;
			namec = 0;
			return(str);
		}

	free(names);
	names = NULL;
	namec = 0;
	return(NULL);
}

static const char *filename_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) {
	static char str[1024];
	struct dirent *dire;
	DIR	*dir;
	const char *end;
	size_t	startlen = strlen(start),
		endlen;

	if ((dir = opendir(start)) == NULL) {
		if ((end = strrchr(start, '/')) != NULL) {
			char	buf[1024];

			snprintf(buf, sizeof(buf), "%.*s", end-start+1, start);
			dir = opendir(buf);
		} else {
			end = start-1;
			dir = opendir(".");
		}
	} else
		end = start+startlen;

	if (dir == NULL)
		return(NULL);

	endlen = strlen(end+1);
	while ((dire = readdir(dir)) != NULL) {
		if ((strcmp(dire->d_name, ".") == 0) || (strcmp(dire->d_name, "..") == 0))
			continue;
		if ((*end == 0) || (strncmp(dire->d_name, end+1, endlen) == 0)) {
			struct stat statbuf;

			if (((end-start) > 0) && (start[end-start-1] == '/'))
				snprintf(str, sizeof(str)-1, "%.*s%s", end-start, start, dire->d_name);
			else if (end == (start-1))
				snprintf(str, sizeof(str)-1, "%s", dire->d_name);
			else
				snprintf(str, sizeof(str)-1, "%.*s/%s", end-start, start, dire->d_name);
			if (stat(str, &statbuf) == -1)
				continue;
			if (S_ISDIR(statbuf.st_mode))
				strcat(str, "/");
			if (match != NULL)
				*match = bufloc-(start-buf);
			if (desc != NULL)
				*desc = NULL;
			closedir(dir);
			return(str);
		}
	}
	closedir(dir);
	return(NULL);
}

const char *conio_tabcomplete(const char *buf, const int bufloc, int *const match, const char **desc) {
	char	*sp = memchr(buf, ' ', bufloc);

	assert(*buf == '/');

	if (sp == NULL) {
		extern alias_t *aliasar;
		extern int aliasc;
		conn_t	*conn;
		const char *co = memchr(buf, ':', bufloc);
		int	i;

		if (co == NULL)
			co = buf+1;
		else
			co++;

		for (i = 0; i < aliasc; i++)
			if (strncasecmp(aliasar[i].name, co, bufloc-(co-buf)) == 0) {
				if (match != NULL)
					*match = bufloc-(co-buf);
				if (desc != NULL)
					*desc = aliasar[i].script;
				return(aliasar[i].name);
			}

		for (i = 0; i < cmdc; i++)
			if (strncasecmp(cmdar[i].c, co, bufloc-(co-buf)) == 0) {
				if (match != NULL)
					*match = bufloc-(co-buf);
				if (desc != NULL)
					*desc = cmdar[i].desc;
				return(cmdar[i].c);
			} else {
				int	j;

				for (j = 0; cmdar[i].aliases[j] != NULL; j++)
					if (strncasecmp(cmdar[i].aliases[j], co, bufloc-(co-buf)) == 0) {
						if (match != NULL)
							*match = bufloc-(co-buf);
						if (desc != NULL)
							*desc = cmdar[i].desc;
						return(cmdar[i].aliases[j]);
					}
			}

		conn = curconn->next;
		do {
			if (strncasecmp(conn->winname, co, bufloc-(co-buf)) == 0) {
				static char
					str[1024];

				if (match != NULL)
					*match = bufloc-(co-buf);
				if (desc != NULL)
					*desc = firetalk_strprotocol(conn->proto);
				strncpy(str, conn->winname, sizeof(str)-2);
				str[sizeof(str)-2] = 0;
				strcat(str, ":");
				return(str);
			}
		} while ((conn = conn->next) != curconn->next);

		return(NULL);
	} else {
		conn_t	*conn = curconn;
		const char *sp = memchr(buf, ' ', bufloc),
			*co = memchr(buf, ':', bufloc),
			*start = n_strnrchr(buf, ' ', bufloc)+1;
		char	type;
		int	cmd;

		if ((co != NULL) && (co < sp)) {
			size_t	connlen = (co - buf)-1;

			do {
				if ((strncasecmp(buf+1, conn->winname, connlen) == 0) && (conn->winname[connlen] == 0))
					break;
			} while ((conn = conn->next) != curconn);
			co++;
		} else
			co = buf+1;

		for (cmd = 0; cmd < cmdc; cmd++)
			if ((strncasecmp(cmdar[cmd].c, co, sp-co) == 0) && (cmdar[cmd].c[sp-co] == 0))
				break;
			else {
				int	alias;

				for (alias = 0; cmdar[cmd].aliases[alias] != NULL; alias++)
					if ((strncasecmp(cmdar[cmd].aliases[alias], co, sp-co) == 0) && (cmdar[cmd].aliases[alias][sp-co] == 0))
						break;
				if (cmdar[cmd].aliases[alias] != NULL)
					break;
			}

		if (cmd < cmdc)
			type = cmdar[cmd].args[0].type;
		else
			type = 's';

		switch (type) {
		  case 'W':
			return(window_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'B':
			return(buddy_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'A':
			return(account_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'M':
			return(cmember_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'I':
			return(idiot_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'C':
			return(chat_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'F':
			return(filename_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'V':
			return(set_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 'E': {
				const char *ret = account_tabcomplete(conn, start, buf, bufloc, match, desc);

				if (ret != NULL)
					return(ret);
			}
			return(chat_tabcomplete(conn, start, buf, bufloc, match, desc));
		  case 's': case 'i': case 'b': case '?':
		  case -1:
			return(NULL);
		  default:
			abort();
		}
	}
}

void	commands_hook_init(void) {
	ua_filter_defaults();
}
