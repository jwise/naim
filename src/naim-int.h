/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2004 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#ifndef naim_int_h
#define naim_int_h	1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef IRC_SUPPORT_CHANNEL
# define IRC_SUPPORT_CHANNEL "#naim"
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_SYS_ERRNO_H
# include <sys/errno.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif
#include <stdio.h>

#if 1
# undef NCURSES_CONST
# define NCURSES_CONST const
# undef NCURSES_OPAQUE
# define NCURSES_OPAQUE 0
# ifdef HAVE_NCURSES_H
#  include <ncurses.h>
# else
#  ifdef HAVE_CURSES_H
#   include <curses.h>
#  else
#   error Unable to locate ncurses.h; please see http://naim.n.ml.org/
#  endif
# endif
# ifndef KEY_CODE_YES
#  if (KEY_MIN & ~1) > 1
#   define KEY_CODE_YES	(KEY_MIN & ~1)
#  else
#   error Unable to identify ncurses key codes
#  endif
# endif
#endif

#include <assert.h>

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#else
# warning No dirent.h
#endif

#ifdef HAVE_TIME_H
# include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_TIMEZONE
extern long int timezone;
#else
# ifdef HAVE_STRUCT_TM_TM_GMTOFF
#  define timezone	(-(tmptr->tm_gmtoff))
# else
#  define timezone	((long int)0)
# endif
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <string.h>


#define SLIDETIME	.3

#ifdef ENABLE_LUA
# define script_init		nlua_init
# define script_clean_garbage	nlua_clean_garbage
# define script_shutdown	nlua_shutdown
# define script_getvar		nlua_getvar
# define script_getvar_int	nlua_getvar_int
# define script_getvar_copy	nlua_getvar_safe
# define script_setvar		nlua_setvar
# define script_setvar_int	nlua_setvar_int
# define script_unsetvar	nlua_unsetvar
# define script_expand		nlua_expand
# define script_script_parse	nlua_script_parse
# define script_cmd		nlua_luacmd
# define script_listvars_start	nlua_listvars_start
# define script_listvars_next	nlua_listvars_next
# define script_listvars_stop	nlua_listvars_stop
# define script_hook_newconn	nlua_hook_newconn
# define script_hook_delconn	nlua_hook_delconn
# define script_hook_newwin	nlua_hook_newwin
# define script_hook_delwin	nlua_hook_delwin
# define script_hook_newbuddy	nlua_hook_newbuddy
# define script_hook_changebuddy nlua_hook_changebuddy
# define script_hook_delbuddy	nlua_hook_delbuddy
#else
# error No scripting engine set
#endif

void	script_init(void);
void	script_clean_garbage(void);
void	script_shutdown(void);
char	*script_getvar(const char *name);
long	script_getvar_int(const char *name);
char	*script_getvar_copy(const char *name, char **buf);
int	script_setvar(const char *name, const char *val);
int	script_setvar_int(const char *name, const long val);
int	script_unsetvar(const char *name);
char	*script_expand(const char *instr);
int	script_script_parse(const char *line);
void	script_cmd(conn_t *conn, char *cmd, char *arg);
void	script_listvars_start(void);
char	*script_listvars_next(void);
void	script_listvars_stop(void);
void	script_hook_newconn(conn_t *conn);
void	script_hook_delconn(conn_t *conn);
void	script_hook_newwin(buddywin_t *bwin);
void	script_hook_delwin(buddywin_t *bwin);
void	script_hook_newbuddy(buddylist_t *buddy);
void	script_hook_changebuddy(buddylist_t *buddy, const char *newaccount);
void	script_hook_delbuddy(buddylist_t *buddy);

static inline char *user_name(char *buf, int buflen, conn_t *conn, buddylist_t *user) {
	static char _buf[256];

	if (buf == NULL) {
		buf = _buf;
		buflen = sizeof(_buf);
	}

	script_setvar("user_name_name", USER_NAME(user));
	if (user->warnval > 0) {
		snprintf(_buf, sizeof(_buf), "%li", user->warnval);
		script_setvar("warnval", _buf);
		script_setvar("user_name_ifwarn",
			script_expand(script_getvar("statusbar_warn")));
	} else
		script_setvar("user_name_ifwarn", "");

	if (firetalk_compare_nicks(conn->conn, USER_ACCOUNT(user), USER_NAME(user)) == FE_SUCCESS) {
		script_setvar("user_name_account", USER_NAME(user));
		snprintf(buf, buflen, "%s", script_expand(script_getvar("nameformat")));
	} else {
		script_setvar("user_name_account", USER_ACCOUNT(user));
		snprintf(buf, buflen, "%s", script_expand(script_getvar("nameformat_named")));
	}
	script_setvar("user_name_account", "");
	script_setvar("user_name_name", "");
	return(buf);
}

static inline const char *naim_basename(const char *name) {
	const char *slash = strrchr(name, '/');

	if (slash != NULL)
		return(slash+1);
	return(name);
}

static inline int naim_strtocol(const char *str) {
	int	i, srccol = 0;

	for (i = 0; str[i] != 0; i++)
		srccol += str[i] << (8*(i%3));
	return(srccol%0xFFFFFF);
}

#define STRREPLACE(target, source) do { \
	assert((source) != NULL); \
	assert((source) != (target)); \
	if (((target) = realloc((target), strlen(source)+1)) == NULL) { \
		echof(curconn, NULL, "Fatal error %i in strdup(%s): %s\n", errno, \
			(source), strerror(errno)); \
		statrefresh(); \
		sleep(5); \
		abort(); \
	} \
	strcpy((target), (source)); \
} while (0)

#define FREESTR(x) do { \
	if ((x) != NULL) { \
		free(x); \
		(x) = NULL; \
	} \
} while (0)

#define WINTIME_NOTNOW(win, cpre, t) do { \
	struct tm	*tptr = localtime(&t); \
	unsigned char	buf[64]; \
	char		*format; \
	\
	if ((format = script_getvar("timeformat")) == NULL) \
		format = "[%H:%M:%S]&nbsp;"; \
	strftime(buf, sizeof(buf), format, tptr); \
	hwprintf(win, C(cpre,EVENT), "</B>%s", buf); \
} while (0)

#define WINTIME(win, cpre)	WINTIME_NOTNOW(win, cpre, now)

#define WINTIMENOLOG(win, cpre) do { \
	struct tm *tptr = localtime(&now); \
	unsigned char buf[64]; \
	char	*format; \
	\
	if ((format = script_getvar("timeformat")) == NULL) \
		format = "[%H:%M:%S]&nbsp;"; \
	strftime(buf, sizeof(buf), format, tptr); \
	hwprintf(win, -C(cpre,EVENT)-1, "</B>%s", buf); \
} while (0)

extern int consolescroll;
#define inconsole	(consolescroll != -1)
#define inconn_real	((curconn != NULL) && (curconn->curbwin != NULL))
#define inconn		(!inconsole && inconn_real)

#define hexdigit(c) \
	(isdigit(c)?(c - '0'):((c >= 'A') && (c <= 'F'))?(c - 'A' + 10):((c >= 'a') && (c <= 'f'))?(c - 'a' + 10):(0))
static inline int naimisprint(int c) {
	return((c >= 0) && (c <= 255) && (isprint(c) || (c >= 160)));
}

typedef struct {
	const char *c;
	void	(*func)();
	const char *aliases[UA_MAXPARMS], *desc;
	const struct {
		const char required, type, *name;
	}	args[UA_MAXPARMS];
	int	minarg,
		maxarg,
		where;
} cmdar_t;

typedef struct {
	char	*buf;
	int	len;
	unsigned char foundfirst:1,
		foundmatch:1,
		foundmult:1;
} namescomplete_t;





/*
 * Provide G_GNUC_INTERNAL that is used for marking library functions
 * as being used internally to the lib only, to not create inefficient PLT entries
 */
#if defined (__GNUC__)
# define G_GNUC_INTERNAL	__attribute((visibility("hidden")))
#else
# define G_GNUC_INTERNAL
#endif

/* atomizer.c */
char	*firstatom(char *string, char *bounds) G_GNUC_INTERNAL;
char	*firstwhite(char *string) G_GNUC_INTERNAL;
char	*atom(char *string) G_GNUC_INTERNAL;

/* buddy.c */
void	playback(conn_t *const conn, buddywin_t *const, const int) G_GNUC_INTERNAL;
void	bcoming(conn_t *conn, const char *) G_GNUC_INTERNAL;
void	bgoing(conn_t *conn, const char *) G_GNUC_INTERNAL;
void	baway(conn_t *conn, const char *, int) G_GNUC_INTERNAL;
void	verify_winlist_sanity(conn_t *const conn, const buddywin_t *const verifywin) G_GNUC_INTERNAL;
void	bclearall(conn_t *conn, int) G_GNUC_INTERNAL;
void	naim_changetime(void) G_GNUC_INTERNAL;

/* commands.c */
void	commands_hook_init(void) G_GNUC_INTERNAL;
const cmdar_t *ua_find_cmd(const char *cmd) G_GNUC_INTERNAL;
const cmdar_t *ua_findn_cmd(const char *cmd, const int len) G_GNUC_INTERNAL;
const char *conio_tabcomplete(const char *buf, const int bufloc, int *const match, const char **desc) G_GNUC_INTERNAL;

/* conio.c */
void	gotkey(int) G_GNUC_INTERNAL;
void	conio_hook_init(void) G_GNUC_INTERNAL;
void	conio_bind_save(FILE *file) G_GNUC_INTERNAL;
void	conio_bind_list(void) G_GNUC_INTERNAL;
void	conio_bind_echo(conn_t *conn, const char *name) G_GNUC_INTERNAL;
void	conio_bind_doset(conn_t *conn, const char *name, const char *binding) G_GNUC_INTERNAL;

/* events.c */
void	updateidletime(void) G_GNUC_INTERNAL;
void	events_hook_init(void) G_GNUC_INTERNAL;

/* fireio.c */
void	chat_flush(buddywin_t *bwin) G_GNUC_INTERNAL;
void	naim_set_info(conn_t *conn, const char *) G_GNUC_INTERNAL;
void	naim_lastupdate(conn_t *conn) G_GNUC_INTERNAL;
void	naim_chat_listmembers(conn_t *conn, const char *const chat) G_GNUC_INTERNAL;
void	fremove(transfer_t *) G_GNUC_INTERNAL;
transfer_t *fnewtransfer(struct firetalk_transfer_t *handle, buddywin_t *bwin, const char *filename,
		const char *from, long size) G_GNUC_INTERNAL;
void	fireio_hook_init(void) G_GNUC_INTERNAL;
void	naim_awaylog(conn_t *conn, const char *src, const char *msg) G_GNUC_INTERNAL;
void	naim_setversion(conn_t *conn) G_GNUC_INTERNAL;

/* hamster.c */
void	logim(conn_t *conn, const char *source, const char *target, const unsigned char *message) G_GNUC_INTERNAL;
void	hamster_hook_init(void) G_GNUC_INTERNAL;

/* helpcmd.c */
void	help_printhelp(const char *) G_GNUC_INTERNAL;

/* logging.c */
FILE    *logging_open(conn_t *const conn, buddywin_t *const bwin) G_GNUC_INTERNAL;

/* rc.c */
const char *account_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) G_GNUC_INTERNAL;
const char *buddy_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) G_GNUC_INTERNAL;
const char *idiot_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) G_GNUC_INTERNAL;
int	rc_resize(faimconf_t *) G_GNUC_INTERNAL;
void	rc_initdefs(faimconf_t *) G_GNUC_INTERNAL;
int	naim_read_config(const char *) G_GNUC_INTERNAL;

/* rodents.c */
int	aimcmp(const unsigned char *, const unsigned char *) G_GNUC_INTERNAL;
int	aimncmp(const unsigned char *, const unsigned char *, int len) G_GNUC_INTERNAL;
const char *dtime(double t) G_GNUC_INTERNAL;
const char *dsize(double b) G_GNUC_INTERNAL;

/* set.c */
const char *set_tabcomplete(conn_t *const conn, const char *start, const char *buf, const int bufloc, int *const match, const char **desc) G_GNUC_INTERNAL;
void	set_setvar(const char *, const char *) G_GNUC_INTERNAL;

/* win.c */
void	do_resize(conn_t *conn, buddywin_t *bwin) G_GNUC_INTERNAL;
void	statrefresh(void) G_GNUC_INTERNAL;
void	whidecursor(void) G_GNUC_INTERNAL;
void	wsetup(void) G_GNUC_INTERNAL;
void	wshutitdown(void) G_GNUC_INTERNAL;
void	win_resize(void) G_GNUC_INTERNAL;
int	nw_printf(win_t *win, int, int, const unsigned char *, ...) G_GNUC_INTERNAL;
int	nw_titlef(const unsigned char *, ...) G_GNUC_INTERNAL;
int	nw_statusbarf(const unsigned char *format, ...) G_GNUC_INTERNAL;
void	nw_initwin(win_t *win, int bg) G_GNUC_INTERNAL;
void	nw_erase(win_t *win) G_GNUC_INTERNAL;
void	nw_refresh(win_t *win) G_GNUC_INTERNAL;
void	nw_attr(win_t *win, char B, char I, char U, char EM,
		char STRONG, char CODE) G_GNUC_INTERNAL;
void	nw_color(win_t *win, int pair) G_GNUC_INTERNAL;
void	nw_flood(win_t *win, int pair) G_GNUC_INTERNAL;
void	nw_addch(win_t *win, const unsigned long ch) G_GNUC_INTERNAL;
void	nw_addstr(win_t *win, const unsigned char *) G_GNUC_INTERNAL;
void	nw_move(win_t *win, int row, int col) G_GNUC_INTERNAL;
void	nw_delwin(win_t *win) G_GNUC_INTERNAL;
void	nw_touchwin(win_t *win) G_GNUC_INTERNAL;
void	nw_newwin(win_t *win, const int height, const int width) G_GNUC_INTERNAL;
void	nw_hline(win_t *win, unsigned long ch, int row) G_GNUC_INTERNAL;
void	nw_vline(win_t *win, unsigned long ch, int col) G_GNUC_INTERNAL;
void	nw_mvwin(win_t *win, int row, int col) G_GNUC_INTERNAL;
void	nw_resize(win_t *win, int row, int col) G_GNUC_INTERNAL;
int	nw_getcol(win_t *win) G_GNUC_INTERNAL;
int	nw_getrow(win_t *win) G_GNUC_INTERNAL;
void	nw_getline(win_t *win, char *buf, int buflen) G_GNUC_INTERNAL;
int	nw_getch(void) G_GNUC_INTERNAL;
void	nw_getpass(win_t *win, char *pass, int len) G_GNUC_INTERNAL;

#endif /* naim_int_h */
