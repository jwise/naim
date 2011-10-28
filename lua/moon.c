/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <stdlib.h>
#include "moon-int.h"
#include "default_lua.h"
#include "naim-int.h"
#include "firetalk-int.h"

lua_State *lua = NULL;
extern conn_t *curconn;
extern void (*script_client_cmdhandler)(const char *);

/* Note: This replaces stuff that was previously handled by SECS.
 * Notable changes --
 *  * Previously, SECS allowed *all* characters in variable names. Now, nLua
 *    requires variable names to match the regexp "[a-zA-Z0-9:_]+".  nLua may
 *    not barf if you don't satisfy this, but it might not always expand
 *    properly.
 *  * Previously, the scripting engine was called SECS. It is now
 *    called Lua, and is interfaced by Moon. Lua is Portuguese for Moon.
 */

static int _nlua_debug(lua_State *L) {
	const char *s = lua_tostring(L, 1);

	status_echof(curconn, "%s", s);
	return(0);
}

static int _nlua_curconn(lua_State *L) {
	_push_conn_t(L, curconn);
	return(1);
}

static int _nlua_curwin(lua_State *L) {
	if (!inconn)
		_push_conn_t(L, curconn);
	else if (curconn->curbwin != NULL)
		_get_global_ent(L, "naim", "connections", curconn->winname, "windows", curconn->curbwin->winname, NULL);
	else
		lua_pushnil(L);
	return(1);
}

static int _nlua_conio(lua_State *L) {
	const char *s = lua_tostring(L, 1);

	if (s == NULL)
		return(luaL_error(L, "l_conio: string was nil"));
	script_client_cmdhandler(s);
	return(0);
}

static int _nlua_echo(lua_State *L) {
	echof(curconn, NULL, "%s\n", lua_tostring(L, 1));
	return(0);
}

static int _nlua_statusbar(lua_State *L) {
	nw_statusbarf("%s", lua_tostring(L, 1));
	return(0);
}

static int _nlua_version(lua_State *L) {
	lua_pushnumber(L, NLUA_API_VERSION);
	return(1);
}

static const struct luaL_Reg naimlib[] = {
	{ "debug",	_nlua_debug },
	{ "curconn",	_nlua_curconn },
	{ "curwin",	_nlua_curwin },
	{ "conio",	_nlua_conio },
	{ "echo",	_nlua_echo },
	{ "statusbar",	_nlua_statusbar },
	{ "version",	_nlua_version },
	{ NULL,		NULL } /* sentinel */
};

static const char *grabword(char *str) {
	int	inquote = 0;
	char	*start;

	while (isspace(*str))
		str++;
	start = str;

	while ((*str != 0) && (inquote || !isspace(*str))) {
		if (*str == '"') {
			memmove(str, str+1, strlen(str+1)+1);
			inquote = !inquote;
			continue;
		}
		str++;
	}

	*str = 0;

	return(start);
}

static int _nlua_pullword(lua_State *L) {
	const char *string = lua_tostring(L, 1), *car, *cdr;
	char	*copy;

	if (string == NULL)
		return(luaL_error(L, "_nlua_firstwhite: string was nil"));
	copy = malloc(strlen(string)+2);
	strncpy(copy, string, strlen(string)+2);
	car = grabword(copy);
	lua_pushstring(L, car);
	cdr = car + strlen(car)+1;
	while (isspace(*cdr))
		cdr++;
	if (*cdr != 0)
		lua_pushstring(L, cdr);
	else
		lua_pushnil(L);
	free(copy);
	return(2);
}

static const struct luaL_reg naim_internallib[] = {
	{ "pullword",	_nlua_pullword },
	{ NULL,		NULL } /* sentinel */
};

#define OPERATION(name, function) \
	static int _nlua_##name(lua_State *L) {\
		unsigned int i,j;\
		i = luaL_checkint(L, 1);\
		j = luaL_checkint(L, 2);\
		lua_pushnumber(L, function);\
		return 1;\
	}

OPERATION(and, i&j)
OPERATION(or, i|j)
OPERATION(xor, i^j)

static int _nlua_md5(lua_State *L) {
	firetalk_md5_t md5;
	const char *s;
	size_t len;

	s = luaL_checklstring(L, 1, &len);

	firetalk_md5_init(&md5);
	firetalk_md5_update(&md5, s, len);
	s = firetalk_md5_final(&md5);

	lua_pushlstring(L, s, 16);

	return(1);
}

static const struct luaL_reg naim_bitlib[] = {
	{ "_and",	_nlua_and },
	{ "_or",	_nlua_or },
	{ "xor",	_nlua_xor },
	{ "md5",        _nlua_md5 },
	{ NULL,		NULL },	/* sentinel */
};

static void _loadfunctions(void) {
	extern const struct luaL_reg naim_prototypes_connectionslib[],
		naim_prototypes_windowslib[],
		naim_prototypes_buddieslib[],
		naim_hookslib[],
		naim_pdlib[],
		naim_pd_internallib[],
		naim_bufferlib[],
		naim_socketlib[];
	extern void naim_commandsreg(lua_State *L);

	luaL_register(lua, "naim", naimlib);
	luaL_register(lua, "naim.internal", naim_internallib);
	luaL_register(lua, "naim.prototypes.connections", naim_prototypes_connectionslib);
	luaL_register(lua, "naim.prototypes.windows", naim_prototypes_windowslib);
	luaL_register(lua, "naim.prototypes.buddies", naim_prototypes_buddieslib);
	luaL_register(lua, "naim.hooks", naim_hookslib);
	luaL_register(lua, "naim.bit", naim_bitlib);
	luaL_register(lua, "naim.pd", naim_pdlib);
	luaL_register(lua, "naim.pd.internal", naim_pd_internallib);
	luaL_register(lua, "naim.buffer", naim_bufferlib);
	luaL_register(lua, "naim.socket", naim_socketlib);

	naim_commandsreg(lua);

	luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.pd.fterrors", 16);
#define ERROR_EXPANDO(x, s) \
		lua_pushstring(lua, #x); \
		lua_newtable(lua); \
		lua_pushstring(lua, "num"); \
		lua_pushnumber(lua, FE_##x); \
		lua_settable(lua, -3); \
		lua_pushstring(lua, "descr"); \
		lua_pushstring(lua, s); \
		lua_settable(lua, -3); \
		lua_settable(lua, -3);
#include "firetalk-errors.h"
#undef ERROR_EXPANDO
	lua_pop(lua, 1);
}

void	nlua_init(void) {
	lua = luaL_newstate();

	/* XXX: Do we need to set a panic function here? */
	lua_gc(lua, LUA_GCSTOP, 0);	/* Paul says we should stop the garbage collector while we bring in libraries. */
		luaL_openlibs(lua);
		_loadfunctions();
	lua_gc(lua, LUA_GCRESTART, 0);

	if (luaL_loadbuffer(lua, default_lua, strlen(default_lua), "default") != 0) {
		printf("default.lua load error: %s\n", lua_tostring(lua, -1));
		abort();
	}
	if (lua_pcall(lua, 0, 0, 0) != 0) {
		printf("default.lua run error: %s\n", lua_tostring(lua, -1));
		abort();
	}
}

void	nlua_ready(void) {
	_get_global_ent(lua, "naim", "internal", "ready", NULL);
	if (lua_pcall(lua, 0, 0, 0) != 0) {
		printf("naim.internal.ready run error: %s\n", lua_tostring(lua, -1));
		abort();
	}
}

void	nlua_shutdown(void) {
	lua_close(lua);
}
