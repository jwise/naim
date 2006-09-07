/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include "moon-int.h"
#include "cmdar.h"

static int _lua2conio(lua_State *L, int first, const char **args, const int argmax) {
	int	argc;

	for (argc = 0; (argc < argmax) && (lua_type(L, argc+first) != LUA_TNONE); argc++)
		args[argc] = lua_tostring(L, argc+first);

	return(argc);
}

static int _table2conio(lua_State *L, int t, const char **args, const int argmax) {
	const int top = lua_gettop(L);
	int	argc;

	lua_pushnil(L);
	for (argc = 0; (argc < argmax) && lua_next(L, t); argc++) {
		args[argc] = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	assert(lua_gettop(L) == top);
	return(argc);
}

static conn_t *_get_conn_t(lua_State *L, int index) {
	const int top = lua_gettop(L);
	conn_t	*obj;

	lua_pushstring(L, "handle");
	lua_gettable(L, index);
	obj = (conn_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);

	assert(lua_gettop(L) == top);
	return(obj);
}

#define NLUA_COMMAND(name) \
static int _nlua_command_ ## name(lua_State *L) { \
	extern void ua_ ## name(conn_t *conn, int argc, const char **args); \
	conn_t	*conn; \
	const char *args[UA_MAXPARMS]; \
	int	argc; \
	const char *error; \
	\
	if ((conn = _get_conn_t(L, 1)) == NULL) \
		return(luaL_error(L, "No connection object; use naim.connections[CONN]:" #name " instead of naim.connections[CONN]." #name ".")); \
	if (lua_istable(L, 2)) \
		argc = _table2conio(L, 2, args, UA_MAXPARMS); \
	else if (lua_isstring(L, 2) || lua_isnumber(L, 2) || lua_isnil(L, 2)) \
		argc = _lua2conio(L, 2, args, UA_MAXPARMS); \
	else \
		return(luaL_error(L, "Invalid argument passed to " #name ".")); \
	if ((error = ua_valid(#name, conn, argc)) == NULL) \
		ua_ ## name(conn, argc, args); \
	else \
		return(luaL_error(L, "%s", error)); \
	return(0); \
}

#define UAFUNC3(x) NLUA_COMMAND(x)
NAIM_COMMANDS
#undef UAFUNC3

static const struct luaL_reg naim_commandslib[] = {
#define UAFUNC3(x) { #x, _nlua_command_ ## x },
NAIM_COMMANDS
#undef UAFUNC3
	{ NULL, 		NULL}
};

void	naim_commandsreg(lua_State *L) {
	const int top = lua_gettop(L);
	int	i;

	assert(cmdc == sizeof(naim_commandslib)/sizeof(*naim_commandslib)-1);

	luaL_findtable(L, LUA_GLOBALSINDEX, "naim.commands", cmdc);	// { naim.commands }

	for (i = 0; i < cmdc; i++) {
		int	j;

		assert(strcmp(cmdar[i].c, naim_commandslib[i].name) == 0);

		lua_pushstring(L, cmdar[i].c);				// { CMD, naim.commands }
		lua_newtable(L);					// { {}, CMD, naim.commands }

		lua_pushstring(L, "func");				// { "func", {}, CMD, naim.commands }
		lua_pushcfunction(L, naim_commandslib[i].func);		// { FUNC, "func", {}, CMD, naim.commands }
		lua_settable(L, -3);					// { { "func" = FUNC }, CMD, naim.commands }
		lua_pushstring(L, "min");				// { "min", { "func" = FUNC }, CMD, naim.commands }
		lua_pushnumber(L, cmdar[i].minarg);			// { MIN, "min", { "func" = FUNC }, CMD, naim.commands }
		lua_settable(L, -3);					// { { "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_pushstring(L, "max");				// { "max", { "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_pushnumber(L, cmdar[i].maxarg);			// { MAX, "max", { "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_settable(L, -3);					// { { "max" = MAX, "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_pushstring(L, "desc");				// { "desc", { "max" = MAX, "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_pushstring(L, cmdar[i].desc);			// { DESC, "desc", { "max" = MAX, "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_settable(L, -3);					// { { "desc" = DESC, "max" = MAX, "func" = FUNC, "min" = MIN }, CMD, naim.commands }
		lua_pushstring(L, "args");				// { "args", ... }
		lua_newtable(L);					// { {}, "args", ... }
		for (j = 0; j < cmdar[i].maxarg; j++) {
			lua_pushnumber(L, j+1);
			lua_pushstring(L, cmdar[i].args[j].name);
			lua_settable(L, -3);
		}
		lua_settable(L, -3);

		lua_settable(L, -3);					// { naim.commands }
	}

	for (i = 0; i < cmdc; i++) {
		int	j;

		for (j = 0; cmdar[i].aliases[j] != NULL; j++) {
			lua_pushstring(L, cmdar[i].aliases[j]);		// { ALIAS, naim.commands }
			lua_getglobal(L, "naim");			// { naim, ALIAS, naim.commands }
			lua_pushstring(L, "commands");			// { "commands", naim, ALIAS, naim.commands }
			lua_gettable(L, -2);				// { naim.commands, naim, ALIAS, naim.commands }
			lua_remove(L, -2);				// { naim.commands, ALIAS, naim.commands }
			lua_pushstring(L, cmdar[i].c);			// { CMD, naim.commands, ALIAS, naim.commands }
			lua_gettable(L, -2);				// { naim.commands[CMD], naim.commands, ALIAS, naim.commands }
			lua_remove(L, -2);				// { naim.commands[CMD], ALIAS, naim.commands }
			lua_settable(L, -3);				// { naim.commands }
		}
	}

	lua_pop(L, 1);							// {}

	assert(lua_gettop(L) == top);
}
