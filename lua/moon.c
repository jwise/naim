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

static int naimcats(lua_State *L) {
	lua_pushstring(L, "hello kitties");
	return(1);
}

static const struct luaL_Reg naimlib [] = {
	{"debug", l_debug},
	{"curconn", l_curconn},
	{"conio", l_conio},
	{"echo", l_echo},
	{NULL, NULL} /* sentinel */
};

static const struct luaL_reg naiminternallib[] = {
	/* reserved for further use */
	{NULL, NULL} /* sentinel */
};

extern const struct luaL_reg naimprototypeconnlib[];
extern const struct luaL_reg naimprototypewindows[];
extern const struct luaL_reg naimprototypebuddies[];

static int _nlua_recvfrom(void *userdata, conn_t *conn, char **name, char **dest, unsigned char **message, int *len, int *flags) {
	int ref = (int)userdata;
	int ret;
	
	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.hooks.recvfrom", 1) != NULL)
		abort();
	lua_rawgeti(lua, -1, ref);
	lua_remove(lua, -2);
	
	_push_conn_t(lua, conn);
	lua_pushstring(lua, *name);
	lua_pushstring(lua, *dest);
	lua_pushlstring(lua, *message, *len);
	lua_pushnumber(lua, *flags);
	if (lua_pcall(lua, 5 /* args */, 1 /* results */, 0) != 0)
	{
		status_echof(curconn, "recvfrom chain %d run error: %s\n", ref, lua_tostring(lua, -1));
		lua_pop(lua, 1);
		return HOOK_CONTINUE;
	}
	if (!lua_isnumber(lua, -1))
		return HOOK_CONTINUE;
	ret = lua_tonumber(lua, -1);
	lua_pop(lua, 1);

	return ret;
}

static int l_hooks_recvfrom_add(lua_State *L) {
	void	*mod = NULL;
	int		weight;
	int		ref;
	
	luaL_checktype(L, 1, LUA_TFUNCTION);
	weight = luaL_checkint(L, 2);
	if (luaL_findtable(L, LUA_GLOBALSINDEX, "naim.internal.hooks.recvfrom", 1) != NULL)
		return luaL_error(L, "recvfrom hooks table damaged");
	lua_pushvalue(L, 1);
	ref = luaL_ref(L, -2); //You can retrieve an object referred by reference r by calling lua_rawgeti(L, t, r). 
	lua_pop(L, 2);
	
	HOOK_ADD(recvfrom, mod, _nlua_recvfrom, weight, (void*)ref);
	
	lua_pushlightuserdata(L, (void*)ref);	/* opaque reference */
	return 1;
}

static int l_hooks_recvfrom_del(lua_State *L) {
	int		ref;
	void	*mod = NULL;
	
	if (!lua_islightuserdata(L, 1))
		return luaL_typerror(L, 1, "light userdata");
	ref = (int)lua_touserdata(L, 1);
	
	HOOK_DEL(recvfrom, mod, _nlua_recvfrom, (void*)ref);
	
	if (luaL_findtable(L, LUA_GLOBALSINDEX, "naim.internal.hooks.recvfrom", 1) != NULL)
		return luaL_error(L, "recvfrom hooks table damaged");
	luaL_unref(L, -1, ref);
	lua_pop(L, 1);
	
	return 0;
}

static const struct luaL_reg naimhooksrecvfromlib[] = {
	{"add", l_hooks_recvfrom_add},
	{"del", l_hooks_recvfrom_del},
	{NULL, NULL} /* sentinel */
};

static void _loadfunctions()
{
	luaL_register(lua, "naim", naimlib);
	luaL_register(lua, "naim.internal", naiminternallib);
	luaL_register(lua, "naim.prototypes.connections", naimprototypeconnlib);
	luaL_register(lua, "naim.prototypes.windows", naimprototypewindows);
	luaL_register(lua, "naim.prototypes.buddies", naimprototypebuddies);
	luaL_register(lua, "naim.hooks.recvfrom", naimhooksrecvfromlib);
	lua_register(lua, "cats", naimcats);
}

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
}

void nlua_shutdown()
{
	lua_close(lua);
}
