/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2005 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>
#include <naim/modutil.h>
#include <ltdl.h>

#include <signal.h>
#include <sys/wait.h>

#include "naim-int.h"
#include "snapshot.h"

extern int wsetup_called;

extern faimconf_t faimconf G_GNUC_INTERNAL;
extern int stayconnected G_GNUC_INTERNAL,
	printtitle G_GNUC_INTERNAL;
extern time_t startuptime G_GNUC_INTERNAL,
	now G_GNUC_INTERNAL;
extern double nowf G_GNUC_INTERNAL,
	changetime G_GNUC_INTERNAL;
extern const char *home G_GNUC_INTERNAL,
	*sty G_GNUC_INTERNAL,
	*invocation G_GNUC_INTERNAL;
extern char naimrcfilename[1024] G_GNUC_INTERNAL;
extern lt_dlhandle dl_self G_GNUC_INTERNAL;

conn_t	*curconn = NULL;
faimconf_t faimconf;
int	stayconnected = 0,
	printtitle = 0;
time_t	startuptime = 0,
	now = 0;
double	nowf = 0.0,
	changetime = 0.0;
const char *home = NULL,
	*sty = NULL,
	*invocation = NULL;
char	naimrcfilename[1024] = { 0 };
lt_dlhandle dl_self = NULL;



#ifdef KEY_MAX
# ifdef SIGHUP
#  define KEY_SIGHUP (2*KEY_MAX+SIGHUP)
# endif
# ifdef SIGUSR1
#  define KEY_SIGUSR1 (2*KEY_MAX+SIGUSR1)
# endif
# ifdef SIGUSR2
#  define KEY_SIGUSR2 (2*KEY_MAX+SIGUSR2)
# endif
#endif

static void dummy(int sig) {
	signal(sig, dummy);
	switch (sig) {
#ifdef KEY_SIGHUP
	  case SIGHUP:
		gotkey(KEY_SIGHUP);
		break;
#endif
#ifdef KEY_SIGUSR1
	  case SIGUSR1:
		gotkey(KEY_SIGUSR1);
		break;
#endif
#ifdef KEY_SIGUSR2
	  case SIGUSR2:
		gotkey(KEY_SIGUSR2);
		break;
#endif
	  default:
		echof(curconn, "SIGNAL", "Got signal %i!\n", sig);
	}
}

#ifdef HAVE_BACKTRACE
void naim_faulthandler(int sig) {
	void	*bt[25];
	size_t	len;
	char	**symbols;
	int	i;

	signal(sig, SIG_DFL);
	len = backtrace(bt, sizeof(bt)/sizeof(*bt));
	symbols = backtrace_symbols(bt, len);
//	wshutitdown();
	fprintf(stderr, "\r\nnaim has crashed.  Sorry about that!\r\n\r\n");
	fprintf(stderr, "Running " PACKAGE_STRING NAIM_SNAPSHOT " for %s.\r\n", dtime(now - startuptime));
#ifdef HAVE_STRSIGNAL
	{
		char	*strsignal(int sig);

		fprintf(stderr, "%s; partial symbolic backtrace:\r\n", strsignal(sig));
	}
#else
	fprintf(stderr, "Signal %i; partial symbolic backtrace:\r\n", sig);
#endif
	for (i = 0; i < len; i++)
		fprintf(stderr, "%i: %s\r\n", i, symbols[i]);
		
	fprintf(stderr, "\r\nThis information is not a replacement for "
		"running naim in gdb.  If you are interested in debugging "
		"this problem, please re-run naim within gdb and reproduce "
		"the fault.  When you are presented with the (gdb) prompt "
		"again, type \"backtrace\" to receive the full symbolic "
		"backtrace and file a bug at "
		"<https://github.com/jwise/naim/issues>.  If you can, leave "
		"the debug session open pending further instructions."
		"\r\n\r\n");
	
	free(symbols);
	raise(sig);
}
#endif

static void childexit(int sig) {
	int	saveerrno = errno;

	signal(sig, childexit);
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
	errno = saveerrno;
}

#ifdef HAVE_GETOPT_LONG
# include <getopt.h>
#endif

HOOK_DECLARE(preselect);
HOOK_DECLARE(postselect);
HOOK_DECLARE(periodic);

#ifndef FAKE_MAIN_STUB
int	main(int argc, char **args) {
#else
int	main_stub(int argc, char **args) {
#endif
	time_t	lastcycle = 0;

	{
		char	*term = getenv("TERM");

		if (term != NULL) {
			if (strcmp(term, "ansi") == 0) {
				int	i;

				printf("The environment variable $TERM is set to \"ansi\", but that does not mean anything. I am going to reset it to \"linux\", since that seems to work best in most situations. If you are using the Windows telnet client, you might want to grab PuTTy, available for free from http://www.tucows.com/\n");
				for (i = 5; i > 0; i--) {
					printf("\r%i", i);
					fflush(stdout);
					sleep(1);
				}
				putenv("TERM=linux");
			} else if ((strncmp(term, "xterm", sizeof("xterm")-1) == 0)
				|| (strncmp(term, "screen", sizeof("screen")-1) == 0)) {
				printtitle = 1;
				nw_titlef("");
			}
		}
	}

	sty = getenv("STY");
	if ((home = getenv("HOME")) == NULL)
		home = "/tmp";

#ifdef HAVE_GETOPT_LONG
	while (1) {
		int	/*i,*/ c,
			option_index = 0;
		static struct option long_options[] = {
# ifdef ALLOW_DETACH
			{ "noscreen",	0,	NULL,	0   },
# endif
			{ "help",	0,	NULL,	'h' },
			{ "version",	0,	NULL,	'V' },
			{ NULL,		0,	NULL,	0   },
		};

		c = getopt_long(argc, args, "hV", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		  case 0:
			break;
		  case 'h':
			printf("Usage:\n");
			printf("    naim [options]\n");
			printf("    nirc [nickname [server]] [options]\n");
			printf("    nicq [options]\n");
			printf("    nlily [options]\n");
			printf("\n");
			printf("Options:\n");
			printf("  -h, --help		Print this message and then exit.\n");
# ifdef ALLOW_DETACH
			printf("      --noscreen	Prevent naim from restarting inside screen.\n");
# endif
			printf("  -V, --version		Print version information and then exit.\n");
			printf("\n");
			printf("See `man naim' for more detailed help.\n");
//			printf("\n");
//			for (i = 0; about[i] != NULL; i++)
//				printf("%s\n", about[i]);
			return(0);
		  case 'V':
			if (strcmp(args[0], "naim") == 0)
				printf("naim " PACKAGE_VERSION NAIM_SNAPSHOT "\n");
			else
				printf("%s (naim) " PACKAGE_VERSION NAIM_SNAPSHOT "\n", args[0]);
			return(0);
		  default:
			printf("Try `%s --help' for more information.\n", args[0]);
			return(1);
		}
	}
#else
	if ((argc > 1) && (args[1][0] == '-') && (strcmp(args[1], "--noscreen") != 0)) {
		if (       (strcasecmp(args[1], "--version") == 0)
			|| (strcasecmp(args[1], "-V") == 0)) {
			if (strcmp(args[0], "naim") == 0)
				printf("naim " PACKAGE_VERSION NAIM_SNAPSHOT "\n");
			else
				printf("%s (naim) " PACKAGE_VERSION NAIM_SNAPSHOT "\n", args[0]);
		} else if ((strcasecmp(args[1], "--help") == 0)
			|| (strcasecmp(args[1], "-H") == 0)) {
			int	i;

			printf("Usage:\n");
			printf("    naim [options]\n");
			printf("    nirc [nickname [server]] [options]\n");
			printf("    nicq [options]\n");
			printf("    nlily [options]\n");
			printf("\n");
			printf("Options:\n");
			printf("  -H, --help		Print this message and then exit.\n");
# ifdef ALLOW_DETACH
			printf("      --noscreen	Prevent naim from restarting inside screen.\n");
# endif
			printf("  -V, --version		Print version information and then exit.\n");
			printf("\n");
			printf("See `man naim' for more detailed help.\n");
//			printf("\n");
//			for (i = 0; about[i] != NULL; i++)
//				printf("%s\n", about[i]);
		} else {
			printf("%s: unrecognized option `%s'\n", args[0], args[1]);
			printf("Try `%s --help' for more information.\n", args[0]);
			return(1);
		}
		return(0);
	}
#endif

#ifdef ALLOW_DETACH
	if ((argc < 2) || (strcmp(args[1], "--noscreen") != 0)) {
		if (sty == NULL) {
			printf("Attempting to restart from within screen (run %s --noscreen to skip this behaviour)...\n",
				args[0]);
			execlp("screen", "screen", "-e", "^Qq", args[0], "--noscreen", NULL);
			printf("Unable to start screen (%s), continuing...\n",
				strerror(errno));
		} else {
			printf("Attempting to restart from within this screen session.\n");
			execlp("screen", "screen", args[0], "--noscreen", NULL);
			printf("Unable to open a new screen window (%s), continuing...\n",
				strerror(errno));
		}
	}
#endif

	printf("Running " PACKAGE_STRING NAIM_SNAPSHOT ".\n");

	changetime = nowf = now = startuptime = time(NULL);

	naim_module_init(DLSEARCHPATH);
#ifdef DLOPEN_SELF_LIBNAIM_CORE
	dl_self = lt_dlopen("cygnaim_core-0.dll");
#else
	dl_self = lt_dlopen(NULL);
#endif

	script_init();

	initscr();
	wsetup_called = 2;
	rc_initdefs(&faimconf);
	endwin();

	wsetup_called = 0;
	wsetup();
	gotkey(0);	// initialize gotkey() buffer
	updateidletime();

	commands_hook_init();
	conio_hook_init();
	events_hook_init();
	fireio_hook_init();
	hamster_hook_init();

#ifdef HAVE_BACKTRACE
	signal(SIGSEGV, naim_faulthandler);
	signal(SIGABRT, naim_faulthandler);
#endif
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGPIPE, dummy);
	signal(SIGALRM, dummy);
#ifdef KEY_SIGHUP
	signal(SIGHUP, dummy);
#endif
#ifdef KEY_SIGUSR1
	signal(SIGUSR1, dummy);
#endif
#ifdef KEY_SIGUSR2
	signal(SIGUSR2, dummy);
#endif
	childexit(SIGCHLD);

	{
		const char *args[] = { "dummy", "IRC" };
		extern void ua_newconn(conn_t *, int, const char **);

		ua_newconn(NULL, 2, args);
	}

	{
		int	want_aim = 0,
			want_irc = 0,
			want_icq = 0,
			want_lily = 0;

		chdir(home);

		script_ready();

		invocation = naim_basename(args[0]);
		if (*invocation == 'n')
			invocation++;

		if (strcmp(invocation, "irc") == 0)
			want_irc = 1;
		else if (strcmp(invocation, "icq") == 0)
			want_icq = 1;
		else if (strcmp(invocation, "lily") == 0)
			want_lily = 1;
		else {
			if (strcmp(invocation, "aim") != 0)
				invocation = naim_basename(args[0]);
			want_aim = 1;
		}

		if (getenv("NAIMRC") != NULL) {
			strncpy(naimrcfilename, getenv("NAIMRC"),
				sizeof(naimrcfilename)-1);
			naimrcfilename[sizeof(naimrcfilename)-1] = 0;
		} else
			snprintf(naimrcfilename, sizeof(naimrcfilename), "%s/.n%src",
				home, invocation);

		echof(curconn, NULL, "Attempting to load %s\n", naimrcfilename);
		if (naim_read_config(naimrcfilename)) {
			conn_t	*conn = curconn;

			do {
				char	buf[256];

				snprintf(buf, sizeof(buf), "%s:READPROFILE %s/.n%sprofile",
					conn->winname, home, invocation);
				ua_handlecmd(buf);
			} while ((conn = conn->next) != curconn);
		} else {
			if (want_aim)
				ua_handlecmd("/newconn AIM OSCAR");
			else if (want_irc)
				ua_handlecmd("/newconn EFnet IRC");
			else if (want_icq)
				ua_handlecmd("/newconn ICQ OSCAR");
			else if (want_lily)
				ua_handlecmd("/newconn Lily SLCP");
			ua_handlecmd("/help");
			echof(curconn, NULL, "You do not have a %s file, so I am using defaults. You can use the <font color=\"#00FF00\">/save</font> command to create a new %s file.",
				naim_basename(naimrcfilename), naim_basename(naimrcfilename));
			if (want_aim || want_icq)
				ua_handlecmd("/addbuddy \"naim help\" \"naim author\" Dan Reed");
			else if (want_irc || want_lily)
				ua_handlecmd("/addbuddy n \"naim author\" Dan Reed");
			if (want_irc) {
				extern void ua_connect(conn_t *, int, char **);

				ua_handlecmd("/join " IRC_SUPPORT_CHANNEL);
				ua_connect(curconn, argc-1, args+1);
			}
		}
	}

	if (curconn == NULL)
		abort();

	if (curconn->next != curconn) {
		const char *args[] = { "dummy" };
		extern void ua_delconn(conn_t *, int, const char **);

		ua_delconn(curconn, 1, args);
	}

	echof(curconn, NULL,
	      "<font color=\"#FF0000\">Notice:</font> you are using an "
	      "*unofficial*, *experimental* version of naim, maintained by "
	      "Joshua Wise.  Bugs that you might find are likely *not* the "
	      "fault of the original author, and should not be reported to "
	      "him!  If (when?) you run into issues, please report bugs at "
	      "<font color=\"#0000FF\">https://github.com/jwise/naim/issues"
	      "</font>.  Thanks!");

	statrefresh();
	doupdate();

	stayconnected = 1;

	while (stayconnected) {
		fd_set	rfd, wfd, efd;
		double	timeout = 0;
		time_t	now60;
		int	autohide;
		uint32_t maxfd = 0;
		struct timeval tv;

		now = time(NULL);
		autohide = script_getvar_int("autohide");
		if (((nowf - changetime) > autohide) && ((nowf - curconn->lastupdate) > autohide))
			timeout = 60 - (now%60);
		else if (((nowf - curconn->lastupdate) <= SLIDETIME)
		      || ((nowf - curconn->lastupdate) >= (autohide - SLIDETIME))
		      || ((nowf - changetime) <= autohide))
			timeout = 0.05;
		else
			timeout = autohide - (nowf - curconn->lastupdate) - SLIDETIME;

		FD_ZERO(&rfd);
		FD_ZERO(&wfd);
		FD_ZERO(&efd);

		HOOK_CALL(preselect, HOOK_T_FDSET HOOK_T_FDSET HOOK_T_FDSET HOOK_T_WRUINT32 HOOK_T_WRFLOAT, &rfd, &wfd, &efd, &maxfd, &timeout);

		tv.tv_sec = timeout;
		tv.tv_usec = (timeout - tv.tv_sec)*1000000;

		if (firetalk_select_custom(maxfd, &rfd, &wfd, &efd, &tv) != FE_SUCCESS) {
			if (errno == EINTR) { // SIGWINCH
				statrefresh();
				if (rc_resize(&faimconf))
					win_resize();
				statrefresh();
				continue;
			}
			echof(curconn, "MAIN", "Main loop encountered error %i (%s).\n",
				errno, strerror(errno));
			nw_refresh(&(curconn->nwin));
			statrefresh();
			sleep(1);
			abort();
			exit(1); /* NOTREACH */
		}

		HOOK_CALL(postselect, HOOK_T_FDSET HOOK_T_FDSET HOOK_T_FDSET, &rfd, &wfd, &efd);

		now60 = now-(now%60);
		if ((now60 - lastcycle) >= 60) {
			lastcycle = now60;
			HOOK_CALL(periodic, HOOK_T_TIME HOOK_T_FLOAT, now, nowf);
		} else if (lastcycle > now60)
			lastcycle = now60;

		statrefresh();
		script_clean_garbage();
	}

	firetalk_select();
	echof(curconn, NULL, "Goodbye.\n");
	statrefresh();
	wshutitdown();
	script_shutdown();
	lt_dlclose(dl_self);
	lt_dlexit();
	return(0);
}
