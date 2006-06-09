/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | (_| || || |\/| | Copyright 1998-2004 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <naim/naim.h>

#include "naim-int.h"

extern conn_t *curconn;

extern alias_t *aliasar G_GNUC_INTERNAL;
extern int aliasc G_GNUC_INTERNAL;
alias_t *aliasar = NULL;
int	aliasc = 0;

void	alias_makealias(const char *alias, const char *script) {
	int	i;

	for (i = 0; i < aliasc; i++)
		if (strcasecmp(aliasar[i].name, alias) == 0)
			break;
	if (i == aliasc) {
		aliasc++;
		aliasar = realloc(aliasar, aliasc*sizeof(*aliasar));
		aliasar[i].name = aliasar[i].script = NULL;
		STRREPLACE(aliasar[i].name, alias);
	}
	STRREPLACE(aliasar[i].script, script);
}

int	alias_parse(const char *script, const char *_arg) {
	char	*arg = NULL;
	int	a, b;

	if (script == NULL)
		return(0);

	if (_arg != NULL)
		_arg = arg = strdup(_arg);
	for (a = 0; (a < 50) && (arg != NULL); a++) {
		char	buf[1024], *tmp;

		tmp = atom(arg);
		snprintf(buf, sizeof(buf), "args%i*", a+1);
		script_setvar(buf, tmp);
		arg = firstwhite(arg);
		snprintf(buf, sizeof(buf), "arg%i", a+1);
		script_setvar(buf, tmp);
        }
	for (b = a; b < 50; b++) {
		char	buf[1024];

		snprintf(buf, sizeof(buf), "args%i*", b+1);
		script_setvar(buf, "");
		snprintf(buf, sizeof(buf), "arg%i", b+1);
		script_setvar(buf, "");
	}

	naim_eval(script);

	while (a > 0) {
		char	buf[1024];

		snprintf(buf, sizeof(buf), "arg%i", a);
		script_setvar(buf, "");
		snprintf(buf, sizeof(buf), "args%i*", a);
		script_setvar(buf, "");
		a--;
	}

	if (_arg != NULL)
		free((void *)_arg);
	return(1);
}

int	alias_doalias(const char *alias, const char *args) {
	int	i;

	for (i = 0; i < aliasc; i++)
		if (strcasecmp(alias, aliasar[i].name) == 0)
			if (alias_parse(aliasar[i].script, args) == 1)
				return(1);
	return(0);
}
