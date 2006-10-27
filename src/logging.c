/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>

#include "naim-int.h"

extern faimconf_t faimconf;
extern time_t	now;
extern const char *home;

extern int colormode G_GNUC_INTERNAL;
int	colormode = COLOR_HONOR_USER;

const unsigned char *naim_normalize(const unsigned char *const name) {
	static char newname[2048];
	int	i, j = 0;

	for (i = 0; (name[i] != 0) && (j < sizeof(newname)-1); i++)
		if ((name[i] == '/') || (name[i] == '.'))
			newname[j++] = '_';
		else if (name[i] != ' ')
			newname[j++] = tolower(name[i]);
	newname[j] = 0;
	return(newname);
}

static int makedir(const char *d) {
	char	*dir;

	if (*d != '/') {
		static char buf[1024];

		snprintf(buf, sizeof(buf), "%s/%s", home, d);
		d = buf;
	}
	dir = strdup(d);
	while (chdir(d) != 0) {
		strcpy(dir, d);
		while (chdir(dir) != 0) {
			char	*pdir = strrchr(dir, '/');

			if (mkdir(dir, 0700) != 0)
				if (errno != ENOENT) {
					chdir(home);
					free(dir);
					return(-1);
				}
			if (pdir == NULL)
				break;
			*pdir = 0;
		}
	}
	chdir(home);
	free(dir);
	return(0);
}

static FILE *playback_fopen(conn_t *const conn, buddywin_t *const bwin, const char *const mode) {
	FILE	*rfile;
	char	*n, *nhtml, *ptr,
		buf[256];

	script_setvar("conn", conn->winname);
	script_setvar("cur", naim_normalize(bwin->winname));

	n = script_expand(script_getvar("logdir"));
	snprintf(buf, sizeof(buf), "%s", n);
	if ((ptr = strrchr(buf, '/')) != NULL) {
		*ptr = 0;
		makedir(buf);
	}

	if (strstr(n, ".html") == NULL) {
		snprintf(buf, sizeof(buf), "%s.html", n);
		nhtml = buf;
	} else {
		nhtml = n;
		snprintf(buf, sizeof(buf), "%s", nhtml);
		if ((ptr = strstr(buf, ".html")) != NULL)
			*ptr = 0;
		n = buf;
	}

	if ((rfile = fopen(n, "r")) != NULL) {
		fclose(rfile);
		if ((rfile = fopen(nhtml, "r")) != NULL) {
			fclose(rfile);
			status_echof(conn, "Warning: While opening logfile for %s, two versions were found: [%s] and [%s]. I will use [%s], but you may want to look into this discrepency.\n",
				bwin->winname, n, nhtml, nhtml);
		} else
			rename(n, nhtml);
	}

	return(fopen(nhtml, mode));
}

FILE	*logging_open(conn_t *const conn, buddywin_t *const bwin) {
	return(playback_fopen(conn, bwin, "a"));
}

void	logging_playback(conn_t *const conn, buddywin_t *const bwin, const int lines) {
	FILE	*rfile;
	struct h_t *h = hhandle(&(bwin->nwin));

	assert(bwin->nwin.logfile != NULL);
	fflush(bwin->nwin.logfile);
	bwin->nwin.dirty = 0;

	if ((rfile = playback_fopen(conn, bwin, "r")) != NULL) {
		char	buf[2048];
		int	maxlen = lines*faimconf.wstatus.widthx;
		long	filesize, playbackstart, playbacklen, pos;
		time_t	lastprogress = now;

#ifdef DEBUG_ECHO
		status_echof(conn, "Redrawing window for %s.", bwin->winname);
#endif

		nw_statusbarf("Redrawing window for %s.", bwin->winname);

		fseek(rfile, 0, SEEK_END);
		while (((filesize = ftell(rfile)) == -1) && (errno == EINTR))
			;
		assert(filesize >= 0);
		if (filesize > maxlen) {
			fseek(rfile, -maxlen, SEEK_CUR);
			while ((fgetc(rfile) != '\n') && !feof(rfile))
				;
		} else
			fseek(rfile, 0, SEEK_SET);
		while (((playbackstart = ftell(rfile)) == -1) && (errno == EINTR))
			;
		assert(playbackstart >= 0);
		pos = 0;
		playbacklen = filesize-playbackstart;
		if (script_getvar_int("color"))
			colormode = COLOR_FORCE_ON;
		else
			colormode = COLOR_FORCE_OFF;
		while (fgets(buf, sizeof(buf), rfile) != NULL) {
			long	len = strlen(buf);

			pos += len;
			//hwprintf(&(bwin->nwin), -C(IMWIN,TEXT)-1, "%s", buf);
			if (buf[len-1] == '\n') {
				hhprint(h, buf, len-1);
				hendblock(h);
			} else
				hhprint(h, buf, len);

			if ((now = time(NULL)) > lastprogress) {
				nw_statusbarf("Redrawing window for %s (%li lines left).",
					bwin->winname, lines*(playbacklen-pos)/playbacklen);
				lastprogress = now;
			}
		}
		colormode = COLOR_HONOR_USER;
		fclose(rfile);
	}
}
