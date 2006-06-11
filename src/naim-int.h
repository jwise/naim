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

#if 1
# undef NCURSES_CONST
# define NCURSES_CONST const
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
# define script_expand		nlua_expand
# define script_script_parse	nlua_script_parse
# define script_cmd		nlua_luacmd
# define script_listvars_start	nlua_listvars_start
# define script_listvars_next	nlua_listvars_next
# define script_listvars_stop	nlua_listvars_stop
# define script_hook_newconn	nlua_hook_newconn
# define script_hook_delconn	nlua_hook_delconn
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
char	*script_expand(const char *instr);
int	script_script_parse(const char *line);
int	script_cmd(char *cmd, char *arg, conn_t *conn);
void	script_listvars_start(void);
char	*script_listvars_next(void);
void	script_listvars_stop(void);
void	script_hook_newconn(conn_t *conn);
void	script_hook_delconn(conn_t *conn);

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

#endif /* naim_int_h */
