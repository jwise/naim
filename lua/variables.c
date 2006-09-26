/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <assert.h>
#include "moon-int.h"

int	nlua_setvar_int(const char *name, const long value) {
	const int top = lua_gettop(lua);

	if (name == NULL)
		return(0);

	_get_global_ent(lua, "naim", "variables", NULL);
	lua_pushstring(lua, name);
	lua_pushnumber(lua, value);
	lua_settable(lua, -3);
	lua_pop(lua, 1);

	assert(lua_gettop(lua) == top);
	return(1);
}

int	nlua_setvar(const char *const name, const char *const value) {
	const int top = lua_gettop(lua);

	if ((name == NULL) || (value == NULL))
		return(0);

	_get_global_ent(lua, "naim", "variables", NULL);
	lua_pushstring(lua, name);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);
	lua_pop(lua, 1);

	assert(lua_gettop(lua) == top);
	return(1);
}

long	nlua_getvar_int(const char *const name) {
	const int top = lua_gettop(lua);
	long	result;

	assert(name != NULL);

	_get_global_ent(lua, "naim", "variables", name, NULL);
	if (lua_isnumber(lua, -1))
		result = (long)lua_tonumber(lua, -1);
	else
		result = 0;
	lua_pop(lua, 1);

	assert(lua_gettop(lua) == top);
	return(result);
}

char	*nlua_getvar_safe(const char *const name, char **buf) {
	const int top = lua_gettop(lua);
	assert(name != NULL);

	_get_global_ent(lua, "naim", "variables", name, NULL);

	if (lua_isstring(lua, -1))		/* Remember that LUA is loosely typed, so a Number would actually give us a string representation! */
		*buf = strdup(lua_tostring(lua, -1));
	else
		*buf = NULL;
	lua_pop(lua, 1);

	assert(lua_gettop(lua) == top);
	return(*buf);
}

char	*nlua_getvar(const char *const name) {
	const int top = lua_gettop(lua);
	char	*result = NULL;

	nlua_getvar_safe(name, &result);
	if (result != NULL)
		_garbage_add(result);

	assert(lua_gettop(lua) == top);
	return(result);
}

void	nlua_listvars_start(void) {
	_get_global_ent(lua, "naim", "variables", NULL);
	lua_pushnil(lua);
}

char	*nlua_listvars_next(void) {
	char	*result = NULL;

	if (lua_next(lua, -2) == 0) {
		lua_pushnil(lua);	/* dummy for nlua_listvars_stop -- avoid exploding the stack */
		return(NULL);
	}
	lua_pop(lua, 1);

	result = strdup(lua_tostring(lua, -1));
	if (result != NULL)
		_garbage_add(result);
	return(result);
}

void	nlua_listvars_stop(void) {
	lua_pop(lua, 2);
}
