/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include "moon-int.h"
#include "default_lua.h"

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

static int l_debug(lua_State *L)
{
	const char *s = lua_tostring(L, 1);
	status_echof(curconn, "%s", s);
	return 0;
}

static int l_curconn(lua_State *L)
{
	_push_conn_t(L, curconn);
	return 1;
}

static int l_conio(lua_State *L)
{
	/* lua_pushlightuserdata(L, void *p) */
	const char *s = lua_tostring(L, 1);
	
	if (!s)
	{
		lua_pushstring(L, "string was nil");
		return lua_error(L);
	}
	script_client_cmdhandler(s);
	return 0;
}

static int l_echo(lua_State *L) {
	echof(curconn, NULL, "%s\n", lua_tostring(L, 1));
	return(0);
}

static const struct luaL_Reg naimlib [] = {
	{"debug", l_debug},
	{"curconn", l_curconn},
	{"conio", l_conio},
	{"echo", l_echo},
	{NULL, NULL} /* sentinel */
};

static const struct luaL_reg naiminternallib[] = {
	{NULL, NULL} /* sentinel */
};

extern const struct luaL_reg naimprototypeconnlib[];

static void _loadfunctions()
{
	luaL_register(lua, "naim", naimlib);
	luaL_register(lua, "naim.internal", naiminternallib);
	luaL_register(lua, "naim.prototypes.connection", naimprototypeconnlib);
}

/*
typedef struct {
	char	*script;
} _client_hook_t;

static int _nlua_recvfrom(void *userdata, conn_t *conn, char **name, char **dest, unsigned char **message, int *len, int *flags) {
	const char *script = ((_client_hook_t *)userdata)->script;

	nlua_script_parse(script);

	return(HOOK_CONTINUE);
}

static void _client_hook_recvfrom(const char *script, const int weight) {
	void	*mod = NULL;
	_client_hook_t *hook;

	if ((hook = calloc(1, sizeof(*hook))) == NULL)
		abort();
	if ((hook->script = strdup(script)) == NULL)
		abort();
	HOOK_ADD(recvfrom, mod, _nlua_recvfrom, weight, hook);
}
*/

void nlua_init()
{
	lua = luaL_newstate();
	
	/* XXX: Do we need to set a panic function here? */
	lua_gc(lua, LUA_GCSTOP, 0);	/* Paul says we should stop the garbage collector while we bring in libraries. */
		luaL_openlibs(lua);
		_loadfunctions();		/* this creates global "naim" for default.lua */
	lua_gc(lua, LUA_GCRESTART, 0);
	
	if (luaL_loadstring(lua, default_lua) != 0)
	{
		printf("default.lua load error: %s\n", lua_tostring(lua, -1));
		abort();
	}
	if (lua_pcall(lua, 0, 0, 0) != 0)
	{
		printf("default.lua run error: %s\n", lua_tostring(lua, -1));
		abort();
	}

//	_client_hook_recvfrom("naim.conio(\"echo test 1\")", 100);
//	_client_hook_recvfrom("naim.conio(\"echo test 2\")", 100);
}

void nlua_shutdown()
{
	lua_close(lua);
}
