/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>
#include <naim/modutil.h>

#include "naim-int.h"
#include "conio_keys.h"

extern win_t	win_info, win_input;
extern conn_t	*curconn;
extern faimconf_t faimconf;
extern int	scrollbackoff, quakeoff;
extern double	nowf, changetime;
extern char	*statusbar_text;

extern int doredraw G_GNUC_INTERNAL,
	inpaste G_GNUC_INTERNAL,
	withtextcomp G_GNUC_INTERNAL;
extern namescomplete_t namescomplete G_GNUC_INTERNAL;
int	doredraw = 0,
	inpaste = 0,
	withtextcomp = 0;
namescomplete_t namescomplete = { 0 };

#define CTRL(x)	((x)-'A'+1)

#define ADDTOBUF(ch)	do { \
	if (strlen(buf) < sizeof(buf)-1) { \
		if (buf[bufloc] != 0) \
			memmove(buf+bufloc+1, buf+bufloc, sizeof(buf)-bufloc-1); \
		buf[bufloc++] = (ch); \
		histpos = histc-1; \
		STRREPLACE(histar[histpos], buf); \
	} else \
		beep(); \
} while (0)

#define ADDSTOBUF(s)	do { \
	int	ADDSTOBUF_i; \
	\
	for (ADDSTOBUF_i = 0; (s)[ADDSTOBUF_i] != 0; ADDSTOBUF_i++) \
		ADDTOBUF((s)[ADDSTOBUF_i]); \
} while (0)

static struct {
	const char *key, *cmd;
} conio_bind_defaultar[] = {
	{ "up",		":INPUT_SCROLL_BACK" },
	{ "down",	":INPUT_SCROLL_FORWARD" },
	{ "left",	":INPUT_CURSOR_LEFT" },
	{ "right",	":INPUT_CURSOR_RIGHT" },
	{ "tab",	":TAB_BUDDY_NEXT" },
	{ "btab",	":BUDDY_PREV" },
	{ "M-tab",	":BUDDY_PREV" },
	{ "space",	":SPACE_OR_NBSP" },
	{ "end",	":BUDDY_NEXT" },
	{ "c1",		":BUDDY_NEXT" },
	{ "home",	":BUDDY_PREV" },
	{ "a1",		":BUDDY_PREV" },
	{ "dc",		":CONN_NEXT" },
	{ "ic",		":CONN_PREV" },
	{ "backspace",	":INPUT_BACKSPACE" },
	{ "^D",		":INPUT_DELETE" },
	{ "enter",	":INPUT_ENTER" },
	{ "ppage",	":BUDDY_SCROLL_BACK" },
	{ "a3",		":BUDDY_SCROLL_BACK" },
	{ "^R",		":BUDDY_SCROLL_BACK" },
	{ "npage",	":BUDDY_SCROLL_FORWARD" },
	{ "c3",		":BUDDY_SCROLL_FORWARD" },
	{ "^Y",		":BUDDY_SCROLL_FORWARD" },
	{ "^A",		":INPUT_CURSOR_HOME" },
	{ "^E",		":INPUT_CURSOR_END" },
	{ "b2",		":INPUT_CURSOR_HOME_END" },
	{ "^P",		":INPUT_PASTE" },
	{ "<",		":INPUT_SYM_LT" },
	{ ">",		":INPUT_SYM_GT" },
	{ "&",		":INPUT_SYM_AMP" },
	{ "F1",		":STATUS_DISPLAY" },
	{ "^K",		":INPUT_KILL_EOL" },
	{ "F2",		":INPUT_ENT_BOLD" },
	{ "^B",		":INPUT_ENT_BOLD" },
	{ "F3",		":INPUT_ENT_ITALIC" },
	{ "^V",		":INPUT_ENT_ITALIC" },
	{ "F4",		":WINLIST_HIDE" },
	{ "M-w",	":WINLIST_HIDE" },
	{ "F5",		":STATUS_POKE" },
	{ "M-s",	":STATUS_POKE" },
	{ "F6",		"/eval /clear; echo Screen cleared" },
	{ "^L",		":REDRAW" },
	{ "F7",		"/jumpback" },
	{ "M-b",	"/jumpback" },
	{ "F8",		"/jump" },
	{ "^N",		"/jump" },
	{ "M-n",	"/jump" },
	{ "F9",		"/away" },
	{ "M-A",	"/away" },
	{ "F10",	"/close" },
	{ "^W",		":INPUT_KILL_WORD" },
	{ "^T",		":INPUT_KILL" },
	{ "^U",		":INPUT_KILL" },
	{ "sighup",	"/readprofile .naimprofile" },
	{ "sigusr1",	"/echo User signal 1 received (SIGUSR1)" },
	{ "sigusr2",	"/echo User signal 2 received (SIGUSR2)" },
};
static const int conio_bind_defaultc = sizeof(conio_bind_defaultar)/sizeof(*conio_bind_defaultar);

static void conio_bind_defaults(void) {
	int	i;

	for (i = 0; i < conio_bind_defaultc; i++) {
		extern void ua_bind(conn_t *conn, int argc, const char **args);
		const char *_args[] = { conio_bind_defaultar[i].key, conio_bind_defaultar[i].cmd };

		ua_bind(curconn, 2, _args);
	}
}

static struct {
	int	key;
	char	*script;
	void	(*func)(char *, int *);
} *conio_bind_ar = NULL;
static int conio_bind_arlen = 0;

void	*conio_bind_func(int key) {
	int	i;

	for (i = 0; i < conio_bind_arlen; i++)
		if (key == conio_bind_ar[i].key)
			return(conio_bind_ar[i].func);
	return(NULL);
}

static const char *conio_bind_get(int key) {
	int	i;

	for (i = 0; i < conio_bind_arlen; i++)
		if (key == conio_bind_ar[i].key)
			return(conio_bind_ar[i].script);
	return(NULL);
}

static const char *conio_bind_getdef(const char *key) {
	int	i;

	for (i = 0; i < conio_bind_defaultc; i++)
		if (strcasecmp(key, conio_bind_defaultar[i].key) == 0)
			return(conio_bind_defaultar[i].cmd);
	return(NULL);
}

const char *conio_bind_get_pretty(int key) {
	const char *binding = conio_bind_get(key);

	if (binding == NULL)
		return(NULL);
	if (binding[0] != ':')
		return(binding);
	return(conio_key_names[atoi(binding+1)].name);
}

const char *conio_bind_get_informative(int key) {
	static char buf[1024];
	const char *binding = conio_bind_get(key);
	int	i;

	if (binding == NULL)
		return(NULL);
	if (binding[0] == '/') {
		const char *end = strchr(binding, ' ');
		const cmdar_t *cmd;

		if (end == NULL)
			end = binding+strlen(binding);
		if ((cmd = ua_findn_cmd(binding+1, end-(binding+1))) != NULL) {
			if (cmd->desc == NULL)
				return(binding);
			else {
				snprintf(buf, sizeof(buf), "%-25s  %s", binding, cmd->desc);
				return(buf);
			}
		}
		return(binding);
	}
	if (binding[0] != ':')
		return(binding);
	i = atoi(binding+1);
	if (*(conio_key_names[i].desc) == 0)
		return(conio_key_names[i].name);
	else {
		snprintf(buf, sizeof(buf), "%-25s  %s", conio_key_names[i].name, conio_key_names[i].desc);
		return(buf);
	}
}

void	conio_bind_save(FILE *file) {
	const char *binding, *def;
	char	key[10];
	int	i, base;

	fprintf(file, "# Key bindings.\n");
	for (base = 0; base <= KEY_MAX; base += KEY_MAX) {
		for (i = 0; i < sizeof(conio_bind_names)/sizeof(*conio_bind_names); i++)
			if ((binding = conio_bind_get_pretty(base+conio_bind_names[i].code)) != NULL) {
				snprintf(key, sizeof(key), "%s%s", (base?"M-":""), conio_bind_names[i].name);

				if (((def = conio_bind_getdef(key)) == NULL)
					|| (strcmp(def, binding) != 0))
					fprintf(file, "bind %s %s\n", key, binding);
			}
		for (i = 1; i <= 12; i++)
			if ((binding = conio_bind_get_pretty(base+KEY_F(i))) != NULL) {
				snprintf(key, sizeof(key), "%sF%i", (base?"M-":""), i);

				if (((def = conio_bind_getdef(key)) == NULL)
					|| (strcmp(def, binding) != 0))
					fprintf(file, "bind %s %s\n", key, binding);
			}
		for (i = 'A'; i <= 'Z'; i++)
			if ((binding = conio_bind_get_pretty(base+CTRL(i))) != NULL) {
				snprintf(key, sizeof(key), "%s^%c", (base?"M-":""), i);

				if (((def = conio_bind_getdef(key)) == NULL)
					|| (strcmp(def, binding) != 0))
					fprintf(file, "bind %s %s\n", key, binding);
			}
	}
	for (i = 'a'; i <= 'z'; i++)
		if ((binding = conio_bind_get_pretty(KEY_MAX+i)) != NULL) {
			char	key[4] = { 'M', '-', i, 0 };

			if (((def = conio_bind_getdef(key)) == NULL)
				|| (strcmp(def, binding) != 0))
				fprintf(file, "bind %s %s\n", key, binding);
		}
	fprintf(file, "\n");
}

void	conio_bind_list(void) {
	const char *binding;
	int	i, base;

	set_echof("    %s%-9s%s  %-25s  %s\n", "", "Key name", "  ", "Script", "Description");
	for (base = 0; base <= KEY_MAX; base += KEY_MAX) {
		for (i = 0; i < sizeof(conio_bind_names)/sizeof(*conio_bind_names); i++)
			if ((binding = conio_bind_get_informative(base+conio_bind_names[i].code)) != NULL)
				set_echof("    %s%-9s%s  %s\n", (base?"M-":""), conio_bind_names[i].name,
					(base?"":"  "), binding);
		for (i = 1; i <= 9; i++)
			if ((binding = conio_bind_get_informative(base+KEY_F(i))) != NULL)
				set_echof("    %sF%i%s         %s\n", (base?"M-":""), i, (base?"":"  "), binding);
		for (i = 10; i <= 12; i++)
			if ((binding = conio_bind_get_informative(base+KEY_F(i))) != NULL)
				set_echof("    %sF%i%s        %s\n", (base?"M-":""), i, (base?"":"  "), binding);
		for (i = 'A'; i <= 'Z'; i++)
			if ((binding = conio_bind_get_informative(base+CTRL(i))) != NULL)
				set_echof("    %s^%c%s         %s\n", (base?"M-":""), i, (base?"":"  "), binding);
	}
	for (i = 'a'; i <= 'z'; i++)
		if ((binding = conio_bind_get_informative(KEY_MAX+i)) != NULL)
			set_echof("    M-%c          %s\n", i, binding);
}

static int conio_bind_getkey(const char *name) {
	int	i, key;

	if ((key = atoi(name)) == 0) {
		int	j;

		if ((name[0] == 'M') && (name[1] == '-')) {
			key = KEY_MAX;
			j = 2;
		} else
			j = 0;
		for (i = 0; i < sizeof(conio_bind_names)/sizeof(*conio_bind_names); i++)
			if (strcasecmp(conio_bind_names[i].name, name+j) == 0) {
				key += conio_bind_names[i].code;
				break;
			}
		if (i == sizeof(conio_bind_names)/sizeof(*conio_bind_names))
			key = 0;
	}

	if (key <= 0) {
		if ((name[0] == 'M') && (name[1] == '-')) {
			key = KEY_MAX;
			i = 2;
		} else
			i = 0;

		if (name[i+1] == 0) {
			if (key == 0)
				key += name[i];
			else
				key += tolower(name[i]);
		} else if (name[i] == 'F')
			key += KEY_F(atoi(name+i+1));
		else if (name[i] == '^')
			key += CTRL(toupper(name[i+1]));
		else if ((name[i] == 'C') && (name[i+1] == '-'))
			key += CTRL(toupper(name[i+2]));
	}

	return(key);
}

void	conio_bind_echo(conn_t *conn, const char *name) {
	int	key = conio_bind_getkey(name);
	const char *binding = conio_bind_get_pretty(key);

	if (binding != NULL)
		echof(conn, NULL, "Key %i (%s) bound to ``%s''\n",
			key, name, binding);
	else
		echof(conn, NULL, "Key %i (%s) is unbound.\n",
			key, name);
}

static void conio_bind_set(int key, const char *script, void (*func)(char *, int *)) {
	int	i;

	for (i = 0; (i < conio_bind_arlen) && (conio_bind_ar[i].key != key); i++);
	if (i == conio_bind_arlen) {
		conio_bind_arlen++;
		conio_bind_ar = realloc(conio_bind_ar,
			conio_bind_arlen*sizeof(*conio_bind_ar));
		conio_bind_ar[i].key = key;
		conio_bind_ar[i].script = NULL;
		conio_bind_ar[i].func = NULL;
	}
	free(conio_bind_ar[i].script);
	if (script != NULL)
		conio_bind_ar[i].script = strdup(script);
	else
		conio_bind_ar[i].script = NULL;
	if (func != NULL)
		conio_bind_ar[i].func = func;
}

void	conio_bind_doset(conn_t *conn, const char *name, const char *binding) {
	int	key = conio_bind_getkey(name);

	if (binding[0] == ':') {
		int	i;
		char	buf[20];

		for (i = 0; i < sizeof(conio_key_names)/sizeof(*conio_key_names); i++)
			if (strcasecmp(binding+1, conio_key_names[i].name+1) == 0)
				break;
		snprintf(buf, sizeof(buf), ":%i", i);
		conio_bind_set(key, buf, NULL);
	} else
		conio_bind_set(key, binding, NULL);

	echof(conn, NULL, "Key %i (%s) now bound to ``%s''\n",
		key, name, binding);
}

static void gotkey_real(int c) {
	static char	**histar = NULL;
	static int	histc = 0,
			histpos = 0;
	static int	bufloc = 0,
			bufmatchpos = -1;
	static char	buf[1024] = { 0 },
			inwhite = 0;
	const char	*binding;
	void	(*bindfunc)(char *, int *);

	if (histar == NULL) {
		histpos = 0;
		histc = 1;
		histar = realloc(histar, histc*sizeof(*histar));
		histar[histpos] = NULL;

		conio_bind_defaults();
		return;
	}
	if (c == 0)
		return;

	if ((c == '\b') || (c == 0x7F))
		c = KEY_BACKSPACE;
	if ((c == '\r') || (c == '\n'))
		c = KEY_ENTER;
	if (c == CTRL('[')) {
		int	k;

		k = getch();
		if (k != ERR) {
			char	buf[256];

			snprintf(buf, sizeof(buf), "KEY_MAX plus %s", keyname(k));
			script_setvar("lastkey", buf);
			c = KEY_MAX + k;
		} else
			script_setvar("lastkey", keyname(c));
	} else
		script_setvar("lastkey", keyname(c));

	binding = conio_bind_get(c);
	bindfunc = conio_bind_func(c);
	if ((binding != NULL) || (bindfunc != NULL)) {
		if ((binding != NULL) && (binding[0] == ':')) {
			int	bindid = atoi(binding+1);

			if (bindid != CONIO_KEY_SPACE_OR_NBSP)
				inwhite = 0;
			if ((bindid != CONIO_KEY_INPUT_SCROLL_BACK) && (bindid != CONIO_KEY_INPUT_SCROLL_FORWARD))
				bufmatchpos = -1;
			switch(bindid) {
			  case CONIO_KEY_REDRAW: /* Redraw the screen */
				doredraw = 1;
				break;
			  case CONIO_KEY_INPUT_SCROLL_BACK: /* Search back through your input history */
				if (histpos > 0) {
					int	tmp;

					if (bufmatchpos == -1)
						bufmatchpos = bufloc;
					for (tmp = histpos-1; tmp >= 0; tmp--)
						if (strncasecmp(histar[tmp], buf, bufmatchpos) == 0) {
							assert(histar[tmp] != NULL);
							if (buf[bufloc] == 0)
								bufloc = strlen(histar[tmp]);
							strncpy(buf, histar[tmp], sizeof(buf)-1);
							histpos = tmp;
							break;
						}
				}
				break;
			  case CONIO_KEY_INPUT_SCROLL_FORWARD: /* Search forward through your input history */
				if (histpos < histc) {
					int	tmp;

					if (bufmatchpos == -1)
						bufmatchpos = bufloc;
					for (tmp = histpos+1; tmp < histc; tmp++)
						if (histar[tmp] == NULL) {
							assert(tmp == histc-1);
							memset(buf, 0, sizeof(buf));
							bufloc = 0;
							histpos = tmp;
							break;
						} else if (strncasecmp(histar[tmp], buf, bufmatchpos) == 0) {
							if (buf[bufloc] == 0)
								bufloc = strlen(histar[tmp]);
							strncpy(buf, histar[tmp], sizeof(buf)-1);
							histpos = tmp;
							break;
						}
				}
				break;
			  case CONIO_KEY_INPUT_CURSOR_LEFT: /* Move left in the input line */
				if (bufloc > 0)
					bufloc--;
				break;
			  case CONIO_KEY_INPUT_CURSOR_RIGHT: /* Move right in the input line */
				if (buf[bufloc] != 0)
					bufloc++;
				break;
			  case CONIO_KEY_SPACE_OR_NBSP: /* In paste mode, an HTML hard space, otherwise a literal space */
				if ((inpaste == 0) || (inwhite == 0)) {
					ADDTOBUF(' ');
					inwhite = 1;
				} else {
					ADDSTOBUF("&nbsp;");
					inwhite = 0;
				}
				break;
			  case CONIO_KEY_TAB: /* An HTML tab (8 hard spaces) */
				ADDSTOBUF("&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;");
				break;
			  case CONIO_KEY_TAB_BUDDY_NEXT: { /* In paste mode, an HTML tab (8 hard spaces), otherwise perform command completion or advance to the next window */
					int	temppaste = inpaste;
					char	*ptr;

					if (!temppaste && (buf[0] != 0) && (buf[0] != '/')) {
						fd_set	rfd;
						struct timeval	timeout;

						FD_ZERO(&rfd);
						FD_SET(STDIN_FILENO, &rfd);
						timeout.tv_sec = 0;
						timeout.tv_usec = 1;
						select(STDIN_FILENO+1, &rfd, NULL, NULL, &timeout);
						if (FD_ISSET(STDIN_FILENO, &rfd))
							temppaste = 1;
					}
					if (temppaste) {
						ADDSTOBUF("&nbsp; &nbsp; &nbsp; &nbsp;&nbsp;");
						break;
					} else if ((bufloc > 0) && (buf[0] == '/')
						&& (buf[bufloc-1] != ' ')) {
						int	match;
						const char
							*str;

						if ((str = conio_tabcomplete(buf, bufloc, &match, NULL)) != NULL) {
							int	i;

							bufloc -= match;
							for (i = 0; i < match; i++)
								buf[bufloc++] = str[i];
							for (; str[i] != 0; i++)
								ADDTOBUF(str[i]);
							if (isalnum(str[i-1]))
								ADDTOBUF(' ');
						} else
							beep();

						break;
					} else if ((bufloc > 0) && (buf[0] != '/')
						&& inconn && (curconn->curbwin->et == CHAT)
						&& (*buf != ',')
						&& (((ptr = strchr(buf, ' ')) == NULL) || (*(ptr+1) == 0))) {
						int	i;

						namescomplete.buf = strdup(buf);
						namescomplete.len = bufloc;
						namescomplete.foundfirst = namescomplete.foundmatch = namescomplete.foundmult = 0;
						firetalk_chat_listmembers(curconn->conn, curconn->curbwin->winname);
						if (namescomplete.foundfirst && !namescomplete.foundmatch)
							firetalk_chat_listmembers(curconn->conn, curconn->curbwin->winname);
						if (namescomplete.foundfirst && !namescomplete.foundmatch)
							bufloc = strlen(buf);
						else if (namescomplete.foundmatch) {

                            memset(buf, 0, sizeof(buf));
                            inwhite = inpaste = bufloc = 0;
                            for (i = 0; namescomplete.buf[i] != 0; i++)
                                ADDTOBUF(namescomplete.buf[i]);
                            if (namescomplete.foundmatch) {
                                char	*delim = getvar(curconn, "addressdelim");

                                if ((delim != NULL) && (*delim != 0))
                                    ADDTOBUF(*delim);
                                else
                                    ADDTOBUF(',');
                                ADDTOBUF(' ');
                            }
                            if (namescomplete.foundmult)
                                bufloc = namescomplete.len;
                            else
                                bufloc = strlen(buf);
						}
						free(namescomplete.buf);
						namescomplete.buf = NULL;
						namescomplete.foundfirst = namescomplete.foundmatch = namescomplete.foundmult = namescomplete.len = 0;
						break;
					}
				}
			  case CONIO_KEY_BUDDY_NEXT: /* Advance to the next window */
				if (inconn_real) {
					assert(curconn->curbwin != NULL);
					curconn->curbwin = curconn->curbwin->next;
					nw_touchwin(&(curconn->curbwin->nwin));
					scrollbackoff = 0;
				}
				naim_changetime();
				bupdate();
				break;
			  case CONIO_KEY_BUDDY_PREV: /* Go to the previous window */
				if (inconn_real) {
					buddywin_t	*bbefore = curconn->curbwin->next;
	
					while (bbefore->next != curconn->curbwin)
						bbefore = bbefore->next;
					curconn->curbwin = bbefore;
					nw_touchwin(&(bbefore->nwin));
					scrollbackoff = 0;
				}
				naim_changetime();
				bupdate();
				break;
			  case CONIO_KEY_CONN_NEXT: /* Go to the next connection window (AIM, EFnet, etc.) */
				curconn = curconn->next;
				if (curconn->curbwin != NULL)
					nw_touchwin(&(curconn->curbwin->nwin));
				scrollbackoff = 0;
				naim_changetime();
				bupdate();
				break;
			  case CONIO_KEY_CONN_PREV: { /* Go to the previous connection window */
					conn_t	*cbefore = curconn->next;
	
					while (cbefore->next != curconn)
						cbefore = cbefore->next;
					curconn = cbefore;
					scrollbackoff = 0;
					if (cbefore->curbwin != NULL)
						nw_touchwin(&(cbefore->curbwin->nwin));
				}
				naim_changetime();
				bupdate();
				break;
			  case CONIO_KEY_INPUT_BACKSPACE: /* Delete the previous character in the input line */
				if (bufloc < 1)
					break;
				memmove(buf+bufloc-1, buf+bufloc,
					sizeof(buf)-bufloc);
				bufloc--;
				break;
			  case CONIO_KEY_INPUT_DELETE: /* Delete the current character in the input line */
				memmove(buf+bufloc, buf+bufloc+1,
					sizeof(buf)-bufloc-1);
				break;
			  case CONIO_KEY_INPUT_ENTER: /* In paste mode, an HTML newline, otherwise sends the current IM or executes the current command */
				{
					int	temppaste = inpaste;

					if (!temppaste && (buf[0] != 0) && (buf[0] != '/') && (getvar_int(curconn, "autopaste") != 0)) {
						fd_set	rfd;
						struct timeval	timeout;

						FD_ZERO(&rfd);
						FD_SET(STDIN_FILENO, &rfd);
						timeout.tv_sec = 0;
						timeout.tv_usec = 1;
						select(STDIN_FILENO+1, &rfd, NULL, NULL, &timeout);
						if (FD_ISSET(STDIN_FILENO, &rfd))
							temppaste = 1;
					}
					if (temppaste) {
						ADDSTOBUF("<br>");
						inwhite = 1;
						break;
					}
				}
				if (buf[0] == 0)
					break;

				histpos = histc-1;
				STRREPLACE(histar[histpos], buf);
				histpos = histc++;
				histar = realloc(histar, histc*sizeof(*histar));
				histar[histpos] = NULL;
				ua_handleline(buf);
			  case CONIO_KEY_INPUT_KILL: /* Delete the entire input line */
				memset(buf, 0, sizeof(buf));
				inwhite = inpaste =
					bufloc = 0;
				break;
			  case CONIO_KEY_INPUT_KILL_WORD: /* Delete the input line from the current character to the beginning of the previous word */
				if (bufloc > 0) {
					int	end = bufloc;

					bufloc--;
					while ((bufloc > 0) && isspace(buf[bufloc]))
						bufloc--;
					while ((bufloc > 0) && !isspace(buf[bufloc]))
						bufloc--;
					if (isspace(buf[bufloc]))
						bufloc++;
					//memmove(buf+bufloc, buf+end, strlen(buf+end)+1);
					memmove(buf+bufloc, buf+end, strlen(buf+end));
					memset(buf+strlen(buf)-(end-bufloc), 0, end-bufloc);
				}
				break;
			  case CONIO_KEY_INPUT_KILL_EOL: /* Delete the input line from the current character to the end of the line */
				memset(buf+bufloc, 0, strlen(buf+bufloc));
				break;
			  case CONIO_KEY_INPUT_PASTE: /* Alter the input handler to handle pasted text better */
				inpaste = (inpaste == 0);
				break;
			  case CONIO_KEY_INPUT_SYM_LT: /* In paste mode, an HTML less-than, otherwise a literal less-than */
				if (inpaste)
					ADDTOBUF('<');
				else
					ADDSTOBUF("&lt;");
				break;
			  case CONIO_KEY_INPUT_SYM_GT: /* In paste mode, an HTML greater-than, otherwise a literal greater-than */
				if (inpaste)
					ADDTOBUF('>');
				else
					ADDSTOBUF("&gt;");
				break;
			  case CONIO_KEY_INPUT_SYM_AMP: /* In paste mode, an HTML ampersand, otherwise a literal ampersand */
				if (inpaste)
					ADDTOBUF('&');
				else
					ADDSTOBUF("&amp;");
				break;
			  case CONIO_KEY_INPUT_ENT_BOLD: /* Toggle HTML bold mode */
				if (strncasecmp(buf+bufloc, "</B>", 4) == 0)
					bufloc += 4;
				else if (bufloc+sizeof("<I></I>")-1 < sizeof(buf)-1) {
					ADDSTOBUF("<B></B>");
					bufloc -= 4;
				} else
					beep();
				break;
			  case CONIO_KEY_INPUT_ENT_ITALIC: /* Toggle HTML italic (inverse) mode */
				if (strncasecmp(buf+bufloc, "</I>", 4) == 0)
					bufloc += 4;
				else if (bufloc+sizeof("<I></I>")-1 < sizeof(buf)-1) {
					ADDSTOBUF("<I></I>");
					bufloc -= 4;
				} else
					beep();
				break;
			  case CONIO_KEY_WINLIST_HIDE: /* Cycle between always visible, always hidden, or auto-hidden */
				if (changetime == 0)
					changetime = nowf;
				else if (changetime == -1)
					changetime = 0;
				else
					changetime = -1;
				break;
			  case CONIO_KEY_STATUS_DISPLAY: /* Display or hide the status console */
				if (consolescroll == -1) {
					if (script_getvar_int("quakestyle") == 1) {
						quakeoff = 2;
						nw_mvwin(&win_info, faimconf.wstatus.starty+2*faimconf.wstatus.widthy/3+1,
							faimconf.winfo.startx);
						nw_mvwin(&win_input, faimconf.wstatus.starty+2*faimconf.wstatus.widthy/3,
							faimconf.winput.startx);
					}
					consolescroll = 0;
				} else {
					if (script_getvar_int("quakestyle") == 1) {
						nw_mvwin(&win_info, faimconf.winfo.starty,
							faimconf.winfo.startx);
						nw_mvwin(&win_input, faimconf.winput.starty,
							faimconf.winput.startx);
					}
					quakeoff = 0;
					consolescroll = -1;
				}
				bupdate();
				break;
			  case CONIO_KEY_STATUS_POKE: /* Bring the status console down for $autohide seconds */
				naim_lastupdate(curconn);
				bupdate();
				break;
			  case CONIO_KEY_BUDDY_SCROLL_BACK: /* Scroll the current window backwards (up) */
				if (consolescroll == -1) {
					scrollbackoff += faimconf.wstatus.widthy-2;
					if (scrollbackoff >= faimconf.wstatus.pady-faimconf.wstatus.widthy)
						scrollbackoff = faimconf.wstatus.pady-faimconf.wstatus.widthy-1;
				} else {
					consolescroll += 2*faimconf.wstatus.widthy/3-2;
					if (consolescroll >= faimconf.wstatus.pady-2*faimconf.wstatus.widthy/3)
						consolescroll = faimconf.wstatus.pady-2*faimconf.wstatus.widthy/3-1;
				}
				break;
			  case CONIO_KEY_BUDDY_SCROLL_FORWARD: /* Scroll the current window forwards in time */
				if (consolescroll == -1) {
					scrollbackoff -= faimconf.wstatus.widthy-2;
					if (scrollbackoff < 0)
						scrollbackoff = 0;
				} else {
					consolescroll -= 2*faimconf.wstatus.widthy/3-2;
					if (consolescroll < 0)
						consolescroll = 0;
				}
				break;
			  case CONIO_KEY_INPUT_CURSOR_HOME: /* Jump to the beginning of the input line */
				bufloc = 0;
				break;
			  case CONIO_KEY_INPUT_CURSOR_END: /* Jump to the end of the input line */
				bufloc = strlen(buf);
				break;
			  case CONIO_KEY_INPUT_CURSOR_HOME_END: /* Jump between the beginning and end of the input line */
				if (bufloc == 0)
					bufloc = strlen(buf);
				else
					bufloc = 0;
				break;
			}
		} else if (binding != NULL)
			naim_eval(binding);
		if (bindfunc != NULL)
			bindfunc(buf, &bufloc);
	} else if (naimisprint(c)) {
		ADDTOBUF(c);
		inwhite = 0;
	} else {
		char	numbuf[20];

		snprintf(numbuf, sizeof(numbuf), "&#%i;", c);
		ADDSTOBUF(numbuf);
	}

	withtextcomp = 0;
	nw_erase(&win_input);
	{
		int	off = (bufloc < faimconf.winput.widthx)?0:
				faimconf.winput.widthx+(faimconf.winput.widthx-10)*((bufloc-faimconf.winput.widthx)/(faimconf.winput.widthx-10))-10;

		if (buf[bufloc] == 0) {
			int	match;
			const char
				*str,
				*desc;

			nw_printf(&win_input, C(INPUT,TEXT), 1, "%s", buf+off);
			if ((bufloc > 1) && (buf[0] == '/') && (buf[bufloc-1] != ' ')
				&& ((str = conio_tabcomplete(buf, bufloc, &match, &desc)) != NULL)
				&& (str[match] != 0)) {
				nw_printf(&win_input, CI(INPUT,EVENT), 0, "%c", 
					str[match]);
				nw_printf(&win_input, C(INPUT,EVENT), 0, "%s", 
					str+match+1);
				if (desc != NULL)
					nw_printf(&win_input, C(INPUT,TEXT), 0, " %s", desc);
				withtextcomp = 1;
			} else if ((bufloc > 5) && (buf[0] != '/')) {
				static int	tmp = -1;

				if ((tmp == -1) || (strncasecmp(histar[tmp], buf, bufloc) != 0))
					for (tmp = histpos-1; tmp >= 0; tmp--)
						if (strncasecmp(histar[tmp], buf, bufloc) == 0)
							break;

				if ((tmp >= 0) && (histar[tmp][bufloc] != 0)) {
					withtextcomp = 2;
					nw_printf(&win_input, CI(INPUT,EVENT), 0, "%c", 
						histar[tmp][bufloc]);
					nw_printf(&win_input, C(INPUT,EVENT), 0, "%s", 
						histar[tmp]+bufloc+1);
				} else
					nw_printf(&win_input, CI(INPUT,TEXT), 0, " ");
			} else
				nw_printf(&win_input, CI(INPUT,TEXT), 0, " ");
		} else {
			int	len = bufloc-off;

			nw_printf(&win_input, C(INPUT,TEXT), 1, "%.*s", len,
				buf+off);
			nw_printf(&win_input, CI(INPUT,TEXT), 0, "%c",
				buf[bufloc]);
			if (len < faimconf.winput.widthx-1)
				nw_printf(&win_input, C(INPUT,TEXT), 1, "%s", 
					buf+off+len+1);
		}
	}
	nw_touchwin(&win_input);
	nw_touchwin(&win_info);
}

void	gotkey(int c) {
	fd_set	rfd;
	struct timeval timeout = { 0, 0 };

	if (statusbar_text != NULL) {
		free(statusbar_text);
		statusbar_text = NULL;
	}

	gotkey_real(c);

	FD_ZERO(&rfd);
	FD_SET(STDIN_FILENO, &rfd);
	while (select(STDIN_FILENO+1, &rfd, NULL, NULL, &timeout) > 0)
		gotkey_real(nw_getch());
}

static int conio_preselect(void *userdata, const char *signature, fd_set *rfd, fd_set *wfd, fd_set *efd, int *maxfd) {
	if (*maxfd <= STDIN_FILENO)
		*maxfd = STDIN_FILENO+1;
	FD_SET(STDIN_FILENO, rfd);
	return(HOOK_CONTINUE);
}

static int conio_postselect(void *userdata, const char *signature, fd_set *rfd, fd_set *wfd, fd_set *efd) {
	if (FD_ISSET(STDIN_FILENO, rfd)) {
		int	k = nw_getch();

		if (k != 0)
			gotkey(k);
	}
	return(HOOK_CONTINUE);
}

void	conio_hook_init(void) {
	void	*mod = NULL;

	HOOK_ADD(preselect, mod, conio_preselect, 100, NULL);
	HOOK_ADD(postselect, mod, conio_postselect, 100, NULL);
}
