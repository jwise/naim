/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#ifndef naim_h
#define naim_h	1

#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>

#include <firetalk.h>

#define UA_MAXPARMS	10

enum {
	cEVENT,
	cEVENT_ALT,
	cTEXT,
	cSELF,
	cBUDDY,
	cBUDDY_WAITING,
	cBUDDY_ADDRESSED,
	cBUDDY_IDLE,
	cBUDDY_AWAY,
	cBUDDY_OFFLINE,
	cBUDDY_QUEUED,
	cBUDDY_TAGGED,
	cBUDDY_FAKEAWAY,
	cBUDDY_TYPING,
	cBUDDY_MOBILE,
	NUMEVENTS
};

enum {
	cINPUT,
	cWINLIST,
	cWINLISTHIGHLIGHT,
	cCONN,
	cIMWIN,
	cSTATUSBAR,
	NUMWINS
};

enum {
	CH_NONE = 0,
	IM_MESSAGE = (1 << 0),
	CH_ATTACKED = (1 << 1),
	CH_ATTACKS = (1 << 2),
	CH_MESSAGE = (1 << 3),
	CH_MISC = (1 << 4),
	CH_USERS = (1 << 5),
	CH_UNKNOWN6 = (1 << 6),
	CH_UNKNOWN7 = (1 << 7),
};

enum {
	ECHOSTATUS_NONE = 0,
	ALSO_STATUS = (1 << 0),
	ALWAYS_STATUS = (1 << 1),
	NOLOG = (1 << 2),
};

enum {
	RF_NONE = 0,
	RF_AUTOMATIC = (1 << 0),
	RF_ACTION = (1 << 1),
	RF_NOLOG = (1 << 2),
	RF_ENCRYPTED = (1 << 3),
	RF_CHAT = (1 << 4),
};

enum {
	COLOR_HONOR_USER,
	COLOR_FORCE_OFF,
	COLOR_FORCE_ON,
};

typedef struct {
	struct winwin_t *_win;
	FILE	*logfile;
	int	logfilelines, height;
	unsigned char dirty:1,
		small:1,
		curbold:1;
} win_t;

typedef struct firetalk_useragent_connection_t {
	char	*sn,
		*password,
		*winname,
		*server,
		*profile;
	long	warnval;
	int	port, proto;
	time_t	online;
	double	lastupdate, lag;
	struct firetalk_connection_t *conn;
	FILE	*logfile;
	win_t	nwin;
	struct buddylist_t *buddyar;
	struct ignorelist_t *idiotar;
	struct buddywin_t *curbwin;
	struct firetalk_useragent_connection_t *next;
} conn_t;

typedef struct buddylist_t {
	char	*_account,
		*_group,
		*_name,
		*crypt,
		*tzname,
		*tag,
		*caps;
	struct buddylist_t *next;
	long	warnval;
	int	peer;
	time_t	typing;
	unsigned long offline:1,
		isaway:1,
		isidle:1,	// is the buddy idle for more than some threshhold?
		docrypt:1,
		isadmin:1,
		ismobile:1;
	conn_t	*conn;
} buddylist_t;
#define DEFAULT_GROUP	"User"
#define CHAT_GROUP	"Chat"
#define USER_ACCOUNT(x)	((x)->_account)
#define USER_NAME(x)	(((x)->_name != NULL)?(x)->_name:(x)->_account)
#define USER_GROUP(x)	((x)->_group)
#define USER_PERMANENT(x) (strcasecmp((x)->_group, DEFAULT_GROUP) != 0)

typedef struct {
	char	*key;
	struct {
		unsigned char *line;
		char	*name;
		int	reps;
		time_t	lasttime;
		int	flags;
		unsigned char istome:1;
	} last;
	unsigned char isoper:1,	// chat operator
		isaddressed:1,	// message addressed sent to me
		offline:1;
} chatlist_t;

typedef struct firetalk_useragent_transfer_t {
	struct firetalk_transfer_t *handle;
	char	*from,
		*remote,
		*local;
	long	size,
		bytes;
	double	started;
	time_t	lastupdate;
	struct buddywin_t *bwin;
	conn_t	*conn;
} transfer_t;

typedef enum {
	CHAT,
	BUDDY,
	TRANSFER,
} et_t;

typedef struct buddywin_t {
	char	*winname,
		*blurb,
		*status;
	unsigned char waiting:1,/* text waiting to be read (overrides
				** offline and isbuddy in bupdate())
				*/
		keepafterso:1;	/* keep window open after buddy signs off? */
	win_t	nwin;
	char	**pouncear;
	int	pouncec;
	time_t	informed,
		closetime;
	double	viewtime;
	union {
		buddylist_t	*buddy;
		chatlist_t	*chat;
		transfer_t	*transfer;
	} e;
	et_t	et;
	struct buddywin_t *next;
	conn_t	*conn;
} buddywin_t;

typedef struct ignorelist_t {
	char	*screenname,
		*notes;
	struct ignorelist_t *next;
	time_t	informed;
} ignorelist_t;

typedef struct {
	int	f[NUMEVENTS],
		b[NUMWINS];
	struct {
		int	startx, starty,
			widthx, widthy, pady;
	} wstatus, winput, winfo, waway, wtextedit;
} faimconf_t;
#define nw_COLORS	8
#define C(back, fore)	(nw_COLORS*faimconf.b[c ## back] +            faimconf.f[c ## fore])
#define CI(back, fore)	(          faimconf.b[c ## back] + nw_COLORS*(faimconf.f[c ## fore]%COLOR_PAIRS))
#define CB(back, fore)	(nw_COLORS*faimconf.b[c ## back] +            faimconf.b[c ## fore])

typedef struct {
	char	*name,
		*script;
} alias_t;

typedef struct {
	const char *var,
		*val,
		*desc;
} rc_var_s_t;

typedef struct {
	const char *var;
	long	val;
	const char *desc;
} rc_var_i_t;

typedef struct {
	char	*from,
		*replace;
} html_clean_t;




/* alias.c */
void	alias_makealias(const char *, const char *);
int	alias_doalias(const char *, const char *);

/* buddy.c */
void	bnewwin(conn_t *conn, const char *, et_t);
void	bupdate(void);
void	bclose(conn_t *conn, buddywin_t *bwin, int _auto);
buddywin_t *bgetwin(conn_t *conn, const char *, et_t);
buddywin_t *bgetanywin(conn_t *conn, const char *);
buddywin_t *bgetbuddywin(conn_t *conn, const buddylist_t *);

/* commands.c */
void   naim_module_init(const char *);

/* conio.c */
void	naim_eval(const char *);
const char *ua_valid(const char *cmd, conn_t *conn, const int argc);
void	ua_handlecmd(const char *);
void	ua_handleline(const char *line);


/* echof.c */
void	status_echof(conn_t *conn, const unsigned char *format, ...);
void	window_echof(buddywin_t *bwin, const unsigned char *format, ...);
void	echof(conn_t *conn, const unsigned char *where, const unsigned char *format, ...);

/* fireio.c */
conn_t	*naim_newconn(int);
buddywin_t *cgetwin(conn_t *, const char *);

/* hamster.c */
void	naim_send_im(conn_t *conn, const char *, const char *, const int _auto);
void	naim_send_im_away(conn_t *conn, const char *, const char *, int force);
void	naim_send_act(conn_t *const conn, const char *const, const unsigned char *const);
void	setaway(const int auto_flag);
void	unsetaway(void);
void	updavail(void);
void	sendaway(conn_t *conn, const char *);
int	getvar_int(conn_t *conn, const char *);
char	*getvar(conn_t *conn, const char *);

/* hwprintf.c */
struct h_t;
int	hhprint(struct h_t *, const unsigned char *, const size_t);
struct h_t *hhandle(win_t *win);
void	hendblock(struct h_t *h);
int	vhhprintf(struct h_t *, const int, const unsigned char *, va_list);
int	vhwprintf(win_t *, int, const unsigned char *, va_list);
int	hwprintf(win_t *, int, const unsigned char *, ...);

/* rc.c */
buddylist_t *rgetlist(conn_t *, const char *);
buddylist_t *rgetlist_friendly(conn_t *conn, const char *friendly);
buddylist_t *raddbuddy(conn_t *, const char *, const char *, const char *);
void	do_delbuddy(buddylist_t *b);
void	rdelbuddy(conn_t *, const char *);
void	raddidiot(conn_t *, const char *, const char *);
void	rdelidiot(conn_t *, const char *);

/* rodents.c */
void	htmlstrip(char *bb);
void	htmlreplace(char *bb, char what);

/* set.c */
void	set_echof(const char *const format, ...);

#endif /* naim_h */
