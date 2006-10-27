/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>
#include <naim/modutil.h>

#include "naim-int.h"

extern faimconf_t faimconf;
extern conn_t	*curconn;
extern int	colormode;

typedef struct h_t {
	win_t	*win;
	struct {
		int	len,
			lastwhite,
			firstwhite,
			secondwhite;
		char	buf[1024];
	} addch;
	struct {
		int	pair;
		unsigned char
			inbold:1,
			initalic:1,
			inunderline:1,
			bodytag:1;
	} fontstack[20];
	int	fontstacklen, newlines, pair;
	unsigned char
		white:1,
		newline:1,
		inbold:1,
		initalic:1,
		inunderline:1,
		last_inunderline:1;
} h_t;

static void h_init_addch(h_t *h) {
	int	i, max;

	max = h->addch.len = nw_getcol(h->win);
	if (max >= sizeof(h->addch.buf))
		max = sizeof(h->addch.buf)-1;
	nw_getline(h->win, h->addch.buf, sizeof(h->addch.buf));
	assert(strlen(h->addch.buf) == h->addch.len);
	h->addch.lastwhite = -1;
	h->addch.firstwhite = -1;
	h->addch.secondwhite = -1;
	for (i = 0; i < max; i++)
		if (isspace(h->addch.buf[i])) {
			h->addch.firstwhite = i;
			break;
		}
	for (i++; i < max; i++)
		if (isspace(h->addch.buf[i])) {
			h->addch.secondwhite = i;
			break;
		}
	for (i++; i < max; i++)
		if (isspace(h->addch.buf[i]))
			h->addch.lastwhite = i;
}

static void h_init(h_t *h, win_t *win) {
	h->win = win;
	h_init_addch(h);
	h->fontstacklen = h->pair = 0;
	h->newline = h->white = h->inbold = h->initalic = h->inunderline = h->last_inunderline = 0;
}

static void nw_decode_addch(win_t *win, unsigned char c) {
	if (c == '\1')
		c = ' ';

	nw_addch(win, c);
}

static void nw_wrap_addch(h_t *h, unsigned char c) {
	if (h->addch.len >= (faimconf.wstatus.widthx-1)) {
		int	i;

		if ((h->addch.lastwhite > -1) && (h->addch.lastwhite > h->addch.firstwhite) && (h->addch.lastwhite > h->addch.secondwhite)) {
			for (i = h->addch.len; i > h->addch.lastwhite; i--)
				nw_addstr(h->win, "\b \b");
			nw_decode_addch(h->win, '\n');
			for (i = 0; i <= h->addch.secondwhite; i++)
				nw_decode_addch(h->win, ' ');
			for (i = h->addch.lastwhite+1; i < h->addch.len; i++)
				nw_decode_addch(h->win, h->addch.buf[i]);
			h->addch.len -= h->addch.lastwhite-1;
		} else {
			for (i = 0; i <= (h->addch.secondwhite+1); i++)
				nw_decode_addch(h->win, ' ');
			h->addch.len = 0;
		}

		h->addch.len += h->addch.secondwhite+1;
		h->addch.lastwhite = -1;
	}

	nw_decode_addch(h->win, c);

	if (c == '\n') {
		h->addch.lastwhite = h->addch.firstwhite = h->addch.secondwhite = -1;
		h->addch.len = 0;
	} else if (c == '\b') {
		if (h->addch.len > 0)
			h->addch.len--;
		if (h->addch.firstwhite == h->addch.len)
			h->addch.firstwhite = -1;
		if (h->addch.secondwhite == h->addch.len)
			h->addch.secondwhite = -1;
	} else {
		if (isspace(c) || (c == '\1')) {
			if (h->addch.firstwhite == -1)
				h->addch.firstwhite = h->addch.len;
			else if (h->addch.secondwhite == -1)
				h->addch.secondwhite = h->addch.len;
			else if (c != '\1')
				h->addch.lastwhite = h->addch.len;
		}
		h->addch.buf[h->addch.len++] = c;
	}
}

/* this is a terrible way of doing this */
static void nw_wrap_addstr(h_t *h, const unsigned char *str) {
	if (str == NULL)
		nw_decode_addch(h->win, '.');
	else {
		int	i;

		for (i = 0; str[i] != 0; i++)
			nw_wrap_addch(h, str[i]);
	}
}

static const struct {
	int	pair,
		R, G, B;
} colar[] = {
	{	COLOR_BLACK,	0x00, 0x00, 0x00	},
	{	COLOR_BLACK,	0xC0, 0xC0, 0xC0	},
	{	COLOR_RED,	0x80, 0x00, 0x00	},
	{	COLOR_RED,	0xFF, 0x00, 0x00	},
	{	COLOR_GREEN,	0x00, 0x80, 0x00	},
	{	COLOR_GREEN,	0x00, 0xFF, 0x00	},
	{	COLOR_YELLOW,	0x80, 0x80, 0x00	},
	{	COLOR_YELLOW,	0xFF, 0xFF, 0x00	},
	{	COLOR_BLUE,	0x00, 0x00, 0x80	},
	{	COLOR_BLUE,	0x00, 0x00, 0xFF	},
	{	COLOR_MAGENTA,	0x80, 0x00, 0x80	},
	{	COLOR_MAGENTA,	0xFF, 0x00, 0xFF	},
	{	COLOR_CYAN,	0x00, 0x80, 0x80	},
	{	COLOR_CYAN,	0x00, 0xFF, 0xFF	},
	{	COLOR_WHITE,	0x80, 0x80, 0x80	},
	{	COLOR_WHITE,	0xFF, 0xFF, 0xFF	},
};

static const char *const parsehtml_pair_RGB(int pair, char bold) {
	static char buf[20];
	int	i;

	for (i = 0; i < sizeof(colar)/sizeof(*colar); i++)
		if ((i%2 == bold) && (colar[i].pair == pair)) {
			snprintf(buf, sizeof(buf), "#%02X%02X%02X", colar[i].R, colar[i].G, colar[i].B);
			return(buf);
		}
	return("#FFFFFF");
}

static int parsehtml_pair_closest(int _pair, int R, int G, int B, char *inbold, char foreorback) {
	int	i,
		back = _pair/nw_COLORS,
		fore = _pair%nw_COLORS,
		bestval = 0xFFFFFF;
	char	bold = 0;

	for (i = 0; i < sizeof(colar)/sizeof(*colar); i++) {
		int	val = ((R-colar[i].R)*(R-colar[i].R)) + ((G-colar[i].G)*(G-colar[i].G)) + ((B-colar[i].B)*(B-colar[i].B));

		if (val < bestval) {
			bestval = val;
			if (foreorback == 'B')
				back = colar[i].pair;
			else {
				fore = colar[i].pair;
				bold = (i%2);
			}
		}
	}

	*inbold = bold;
	if (bold || (back != fore))
		return(nw_COLORS*back + fore);
	else if (fore != nw_COLORS-1)
		return(nw_COLORS*back + nw_COLORS-1);
	else
		return(nw_COLORS*back);
}

static int parsehtml_pair(const unsigned char *buf, int _pair, char *inbold, char foreorback) {
	int	R, G, B;

	if (*buf == '#') {
		buf++;
		R = 16*hexdigit(*buf) + hexdigit(*(buf+1));
		buf += 2;
		G = 16*hexdigit(*buf) + hexdigit(*(buf+1));
		buf += 2;
		B = 16*hexdigit(*buf) + hexdigit(*(buf+1));
	} else if (strncasecmp(buf, "rgb(", sizeof("rgb(")-1) == 0) {
		buf += sizeof("rgb(")-1;
		R = atoi(buf);
		buf = strchr(buf, ',');
		if (buf == NULL)
			return(_pair);
		G = atoi(buf);
		buf = strchr(buf, ',');
		if (buf == NULL)
			return(_pair);
		B = atoi(buf);
	} else
		return(_pair);
	return(parsehtml_pair_closest(_pair, R, G, B, inbold, foreorback));
}

#define CHECKTAG(tag)	(strcasecmp(tagbase, (tag)) == 0)
#define CHECKAMP(tag)	(strcasecmp(tagbuf, (tag)) == 0)

static unsigned long parsehtml_tag(h_t *h, const unsigned char *text, int backup) {
	unsigned char tagbuf[20] = { 0 },
		argbuf[1024] = { 0 },
		*tagbase;
	const unsigned char *textsave = text;
	int	tagpos = 0;

	while (isspace(*text))
		text++;
	while ((*text != 0) && !isspace(*text) && (*text != '>')) {
		if (tagpos < sizeof(tagbuf)-1)
			tagbuf[tagpos++] = *text;
		text++;
	}
	while (isspace(*text))
		text++;
	tagpos = 0;
	while ((*text != 0) && (*text != '>')) {
		if (tagpos < sizeof(argbuf)-1)
			argbuf[tagpos++] = *text;
		text++;
	}

	tagbase = tagbuf;
	if (*tagbase == '/')
		tagbase++;

	if CHECKTAG("B") {
		h->inbold = (*tagbuf != '/');
	} else if CHECKTAG("I") {
		h->initalic = (*tagbuf != '/');
	} else if CHECKTAG("U") {
		h->inunderline = (*tagbuf != '/');
	} else if CHECKTAG("A") {
		if (*tagbuf != '/') {
			char	*t = argbuf;
			int	found = 0;

			while ((found == 0) && (*t != 0)) {
				while (isspace(*t))
					t++;
				if ((strncasecmp(t, "href ", sizeof("href ")-1) == 0) || (strncasecmp(t, "href=", sizeof("href=")-1) == 0)) {
					char	refbuf[256];
					int	i = 0;

					t += sizeof("href")-1;
					while (isspace(*t))
						t++;
					if (*t != '=')
						continue;
					t++;
					while (isspace(*t))
						t++;
					if ((*t == '"') || (*t == '\'')) {
						char	q = *t++;

						while ((*t != 0) && (*t != q)) {
							if (i < (sizeof(refbuf)-1))
								refbuf[i++] = *t;
							t++;
						}
						if (*t == q)
							t++;
					} else {
						while ((*t != 0) && (!isspace(*t))) {
							if (i < (sizeof(refbuf)-1))
								refbuf[i++] = *t;
							t++;
						}
					}
					refbuf[i] = 0;
					script_setvar("lasturl", refbuf);
					h->last_inunderline = h->inunderline;
					h->inunderline = 1;
					found = 1;
				} else {
					while ((*t != 0) && (!isspace(*t)))
						t++;
					while (isspace(*t))
						t++;
					if (*t == '=') {
						t++;
						while (isspace(*t))
							t++;
						if ((*t == '"') || (*t == '\'')) {
							char	q = *t++;

							while ((*t != 0) && (*t != q))
								t++;
							if (*t == q)
								t++;
						} else
							while ((*t != 0) && (!isspace(*t)))
								t++;
					}
				}
			}
		} else {
			char	*lasturl = script_getvar("lasturl");

			h->inunderline = h->last_inunderline;
			if ((lasturl != NULL) && (strncmp(textsave-backup, lasturl, strlen(lasturl)) != 0)) {
				nw_wrap_addstr(h, " [");
				nw_wrap_addstr(h, lasturl);
				nw_wrap_addch(h, ']');
			}
		}
	} else if CHECKTAG("IMG") {
		nw_wrap_addstr(h, "[IMAGE:");
		nw_wrap_addstr(h, argbuf);
		nw_wrap_addch(h, ']');
	} else if (CHECKTAG("BR") || CHECKTAG("BR/")) {
		nw_wrap_addstr(h, "\n ");
		h->newline = h->white = 1;
		h->newlines++;
	} else if CHECKTAG("HR") {
		nw_wrap_addstr(h, "----------------\n ");
		h->newline = h->white = 1;
		h->newlines++;
	} else if CHECKTAG("FONT") {
		if ((colormode == COLOR_FORCE_ON) || ((colormode == COLOR_HONOR_USER) && script_getvar_int("color"))) {
		    if (*tagbuf != '/') {
			char	*t = argbuf;
			int	found = 0;

			while ((found == 0) && (*t != 0)) {
				while (isspace(*t))
					t++;
				if ((strncasecmp(t, "color ", sizeof("color ")-1) == 0) || (strncasecmp(t, "color=", sizeof("color=")-1) == 0)) {
					char	colbuf[20];
					int	i = 0;

					t += sizeof("color")-1;
					while (isspace(*t))
						t++;
					if (*t != '=')
						continue;
					t++;
					while (isspace(*t))
						t++;
					if ((*t == '"') || (*t == '\'')) {
						char	q = *t++;

						while ((*t != 0) && (*t != q)) {
							if (i < (sizeof(colbuf)-1))
								colbuf[i++] = *t;
							t++;
						}
						if (*t == q)
							t++;
					} else {
						while ((*t != 0) && (!isspace(*t))) {
							if (i < (sizeof(colbuf)-1))
								colbuf[i++] = *t;
							t++;
						}
					}
					colbuf[i] = 0;
					h->fontstack[h->fontstacklen].pair = h->pair;
					h->fontstack[h->fontstacklen].inbold = h->inbold;
					h->fontstack[h->fontstacklen].initalic = h->initalic;
					h->fontstack[h->fontstacklen].inunderline = h->inunderline;
					if (h->fontstacklen < sizeof(h->fontstack)/sizeof(*(h->fontstack)))
						h->fontstacklen++;
					{
						char	inbold = h->inbold;

						h->pair = parsehtml_pair(colbuf, h->pair, &inbold, 'F');
						h->inbold = inbold?1:0;
					}
					found = 1;
				} else {
					while ((*t != 0) && (!isspace(*t)))
						t++;
					while (isspace(*t))
						t++;
					if (*t == '=') {
						t++;
						while (isspace(*t))
							t++;
						if ((*t == '"') || (*t == '\'')) {
							char	q = *t++;

							while ((*t != 0) && (*t != q))
								t++;
							if (*t == q)
								t++;
						} else
							while ((*t != 0) && (!isspace(*t)))
								t++;
					}
				}
			}
		    } else {
			if (h->fontstacklen > 0)
				h->fontstacklen--;
			else
				h->fontstacklen = 0;
			h->pair = h->fontstack[h->fontstacklen].pair;
			h->inbold = h->fontstack[h->fontstacklen].inbold;
			h->initalic = h->fontstack[h->fontstacklen].initalic;
			h->inunderline = h->fontstack[h->fontstacklen].inunderline;
		    }
		}
	} else if CHECKTAG("PRE") {
	} else if CHECKTAG("P") {
	} else if (CHECKTAG("HTML") || CHECKTAG("BODY") || CHECKTAG("DIV") || CHECKTAG("SPAN")) {
		if ((colormode == COLOR_FORCE_ON) || ((colormode == COLOR_HONOR_USER) && script_getvar_int("color"))) {
		    if (*tagbuf != '/') {
			char	*t = argbuf;
			int	found = 0;

			while ((found == 0) && (*t != 0)) {
				while (isspace(*t))
					t++;
				if ((strncasecmp(t, "bgcolor ", sizeof("bgcolor ")-1) == 0) || (strncasecmp(t, "bgcolor=", sizeof("bgcolor=")-1) == 0)) {
					char	colbuf[20];
					int	i = 0;

					t += sizeof("bgcolor")-1;
					while (isspace(*t))
						t++;
					if (*t != '=')
						continue;
					t++;
					while (isspace(*t))
						t++;
					if ((*t == '"') || (*t == '\'')) {
						char	q = *t++;

						while ((*t != 0) && (*t != q)) {
							if (i < (sizeof(colbuf)-1))
								colbuf[i++] = *t;
							t++;
						}
						if (*t == q)
							t++;
					} else {
						while ((*t != 0) && (!isspace(*t))) {
							if (i < (sizeof(colbuf)-1))
								colbuf[i++] = *t;
							t++;
						}
					}
					colbuf[i] = 0;
					h->fontstack[h->fontstacklen].pair = h->pair;
					h->fontstack[h->fontstacklen].inbold = h->inbold;
					h->fontstack[h->fontstacklen].initalic = h->initalic;
					h->fontstack[h->fontstacklen].inunderline = h->inunderline;
					h->fontstack[h->fontstacklen].bodytag = 1;
					if (h->fontstacklen < sizeof(h->fontstack)/sizeof(*(h->fontstack)))
						h->fontstacklen++;
					{
						char	inbold = h->inbold;

						h->pair = parsehtml_pair(colbuf, h->pair, &inbold, 'B');
						h->inbold = inbold?1:0;
					}
					found = 1;
				} else {
					while ((*t != 0) && (!isspace(*t)))
						t++;
					while (isspace(*t))
						t++;
					if (*t == '=') {
						t++;
						while (isspace(*t))
							t++;
						if ((*t == '"') || (*t == '\'')) {
							char	q = *t++;

							while ((*t != 0) && (*t != q))
								t++;
							if (*t == q)
								t++;
						} else
							while ((*t != 0) && (!isspace(*t)))
								t++;
					}
				}
			}
		    } else {
			if (h->fontstacklen > 0) {
				int	i;

				for (i = h->fontstacklen-1; i >= 0; i--)
					if (h->fontstack[i].bodytag)
						break;
				h->pair = h->fontstack[i].pair;
				h->inbold = h->fontstack[i].inbold;
				h->initalic = h->fontstack[i].initalic;
				h->inunderline = h->fontstack[i].inunderline;
			} else
				h->inbold = h->initalic = h->inunderline = 0;
			h->fontstacklen = 0;
		    }
		}
	} else
		return(0);
	return((unsigned long)(text-textsave+1));
}

static unsigned long parsehtml_amp(h_t *h, const unsigned char *text) {
	unsigned char tagbuf[20] = { 0 };
	const unsigned char *textsave = text;
	int	tagpos = 0;

	while (isspace(*text))
		text++;
	while ((*text != 0) && !isspace(*text) && (*text != ';') && (*text != '\n')) {
		if (tagpos < sizeof(tagbuf)-1)
			tagbuf[tagpos++] = *text;
		else
			return(0);
		text++;
	}

	if (*text != ';')
		return(0);

	if (*tagbuf == '#') {
		int	c = atoi(tagbuf+1);

		if (naimisprint(c))
			nw_wrap_addch(h, c);
		else
			nw_wrap_addstr(h, keyname(c));
	} else if CHECKAMP("NBSP") {
		nw_wrap_addch(h, '\1');
	} else if CHECKAMP("AMP") {
		nw_wrap_addch(h, '&');
	} else if CHECKAMP("LT") {
		nw_wrap_addch(h, '<');
	} else if CHECKAMP("GT") {
		nw_wrap_addch(h, '>');
	} else if CHECKAMP("QUOT") {
		nw_wrap_addch(h, '"');
	} else
		return(0);

	h->newline = h->white = 0;
	return((unsigned long)(text-textsave+1));
}

static unsigned long parsehtml(h_t *h, const unsigned char *str, int backup) {
	if (*str == '<')
		return(parsehtml_tag(h, str+1, backup));
	else if (*str == '&')
		return(parsehtml_amp(h, str+1));
	else
		return(0);
}

int	hhprint(h_t *h, const unsigned char *str, const size_t len) {
	int	pos, lastpos = 0;

	for (pos = 0; pos < len; pos++)
		if ((str[pos] == '<') || (str[pos] == '&')) {
			unsigned long skiplen = 0;

			if ((skiplen = parsehtml(h, str+pos, pos-lastpos)) == 0) {
				nw_wrap_addch(h, str[pos]);
				continue;
			}
			pos += skiplen;
			nw_attr(h->win, h->inbold, h->initalic, h->inunderline, 0, 0, 0);
			nw_color(h->win, h->pair);
			lastpos = pos;
		} else {
			if (str[pos] == '\r')
				continue;
			h->newline = 0;
			if (isspace(str[pos]) || (str[pos] == '\n')) {
				if (!h->white)
					nw_wrap_addch(h, ' ');
				h->white = 1;
				continue;
			} else
				h->white = 0;
			if (naimisprint(str[pos]))
				nw_wrap_addch(h, str[pos]);
			else
				nw_wrap_addstr(h, keyname(str[pos]));
		}

	return(h->newlines);
}

h_t	*hhandle(win_t *win) {
	static h_t h;

	h_init(&h, win);

	return(&h);
}

void	hendblock(h_t *h) {
	if (h->newline)
		nw_wrap_addch(h, '\b');

	h->fontstacklen = 0;
}

int	vhhprintf(h_t *h, const int dolog, const unsigned char *format, va_list msg) {
	unsigned char _buf[2048], *str;
	size_t	len;
	int	ret;

	assert(h->win != NULL);
	assert(format != NULL);

	len = vsnprintf(_buf, sizeof(_buf), format, msg);

	if (len >= sizeof(_buf)) {
		size_t	len2;

		str = malloc(len+1);
		assert(str != NULL);

		len2 = vsnprintf(str, len+1, format, msg);
		assert(len2 == len);
	} else
		str = _buf;

	if (dolog && (h->win->logfile != NULL))
		fprintf(h->win->logfile, "<font color=\"%s\">%s%s%s%s%s</font>\n",
			parsehtml_pair_RGB(h->pair, h->inbold),
			h->initalic?"<I>":"",
			h->inunderline?"<U>":"",
			str,
			h->inunderline?"</U>":"",
			h->initalic?"</I>":"");

	nw_attr(h->win, h->inbold, h->initalic, h->inunderline, 0, 0, 0);
	nw_color(h->win, h->pair);

	ret = hhprint(h, str, len);

	if (str != _buf)
		free(str);

	if (dolog)
		h->win->logfilelines += ret;

	return(ret);
}

int	vhwprintf(win_t *win, int _pair, const unsigned char *format, va_list msg) {
	h_t	h;
	int	ret, dolog = 0;

	h_init(&h, win);

	if (_pair > -1)
		dolog = 1;
	else
		_pair = -_pair-1;

	if (_pair >= 2*(nw_COLORS*nw_COLORS)) {
		h.inbold = 1;
		_pair -= 2*(nw_COLORS*nw_COLORS);
	} else if (_pair >= (nw_COLORS*nw_COLORS)) {
		h.inbold = 0;
		_pair -= (nw_COLORS*nw_COLORS);
	}
	h.pair = _pair;

	ret = vhhprintf(&h, dolog, format, msg);

	if (h.white)
		nw_wrap_addch(&h, '\b');

	return(ret);
}

int	hwprintf(win_t *win, int _pair, const unsigned char *format, ...) {
	va_list	msg;
	int	ret;

	va_start(msg, format);
	ret = vhwprintf(win, _pair, format, msg);
	va_end(msg);

	return(ret);
}
