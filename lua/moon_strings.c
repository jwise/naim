/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include "moon-int.h"

extern conn_t *curconn;

static void *_garbage_dump[100] = { 0 }, **_garbage_dump2 = NULL;
static int _garbage_count = 0, _garbage_count2 = 0;

void	nlua_clean_garbage(void) {
	int	i;

	if (_garbage_count > 0) {
		for (i = 0; i < _garbage_count; i++) {
			free(_garbage_dump[i]);
			_garbage_dump[i] = NULL;
		}
		_garbage_count = 0;
	}
	if (_garbage_count2 > 0) {
		for (i = 0; i < _garbage_count2; i++) {
			free(_garbage_dump2[i]);
			_garbage_dump2[i] = NULL;
		}
		free(_garbage_dump2);
		_garbage_dump2 = NULL;
		_garbage_count2 = 0;
	}
}

static void _garbage_add(void *ptr) {
	if (_garbage_count < sizeof(_garbage_dump)/sizeof(*_garbage_dump))
		_garbage_dump[_garbage_count++] = ptr;
	else {
		_garbage_dump2 = realloc(_garbage_dump2, (_garbage_count2+1)*sizeof(*_garbage_dump2));
		if (_garbage_dump2 == NULL)
			abort();
		_garbage_dump2[_garbage_count2++] = ptr;
	}
}

int nlua_setvar_int(const char *name, const long value)
{
	if (name == NULL)
		return(0);
	
	_getsubtable("variables");
	lua_pushstring(lua, name);
	lua_pushnumber(lua, value);
	lua_settable(lua, -3);
	lua_pop(lua, 1);
	return(1);
}

int nlua_setvar(const char *const name, const char *const value)
{
	if ((name == NULL) || (value == NULL))
		return(0);
	
	_getsubtable("variables");
	lua_pushstring(lua, name);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);
	lua_pop(lua, 1);
	return(1);
}

long nlua_getvar_int(const char *const name)
{
	long result;
	
	assert(name != NULL);
	
	_getsubtable("variables");
	lua_pushstring(lua, name);
	lua_gettable(lua, -2);
	if (lua_isnumber(lua, -1))
		result = (long)lua_tonumber(lua, -1);
	else
		result = 0;
	lua_pop(lua, 2);
	return result;
	
}

char* nlua_getvar_safe(const char *const name, char** buf)
{
	assert(name != NULL);
	
	_getsubtable("variables");
	lua_pushstring(lua, name);
	lua_gettable(lua, -2);

	if (lua_isstring(lua, -1))		/* Remember that LUA is loosely typed, so a Number would actually give us a string representation! */
		*buf = strdup(lua_tostring(lua, -1));
	else
		*buf = NULL;
	lua_pop(lua, 2);
	
	return *buf;
}

char* nlua_getvar(const char *const name)
{
	char* result = NULL;
	
	nlua_getvar_safe(name, &result);
	if (result != NULL)
		_garbage_add(result);
	return(result);
}

int nlua_script_parse(const char *script)
{
	if (luaL_loadstring(lua, script) != 0)
	{
		status_echof(curconn, "Parse error: \"%s\"", lua_tostring(lua, -1));
		lua_pop(lua, 1);		/* Error message that we don't care about. */
		return(0);
	}
	
	if (lua_pcall(lua, 0, 0, 0) != 0)
	{
		status_echof(curconn, "Lua function returned an error: \"%s\"", lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}

	return(1);
}

char* nlua_expand(const char *script)
{
	char* result;
	
	_getsubtable("internal");
	_getitem("expandString");
	lua_pushstring(lua, script);
	lua_pcall(lua, 1, 1, 0);		/* Feed the error message to the caller if there is one */
	result = strdup(lua_tostring(lua, -1));
	if (result != NULL)
		_garbage_add(result);
	lua_pop(lua, 1);
	return(result);
}

void nlua_listvars_start()
{
	_getsubtable("variables");
	lua_pushnil(lua);
}

char* nlua_listvars_next()
{
	char* result = NULL;
	
	if (lua_next(lua, -2) == 0)
	{
		lua_pushnil(lua);	/* dummy for nlua_listvars_stop -- avoid exploding the stack */
		return NULL;
	}
	lua_pop(lua, 1);

	result = strdup(lua_tostring(lua, -1));
	if (result != NULL)
		_garbage_add(result);
	return(result);
}

void nlua_listvars_stop()
{
	lua_pop(lua, 2);
}

int nlua_luacmd(char *cmd, char *arg, conn_t *conn)
{
	char *lcmd;
	
	_getmaintable();
	_getitem("commands");
	if (!lua_istable(lua, -1))
	{
		static int complained = 0;
		if (complained)
			return 0;
		complained++;
		status_echof(conn, "naim's Lua commands table went away. This is a bug in a user script. I'll continue for now, but Lua commands will no longer work. Sorry.");
		return 0;
	}
	lcmd = strdup(cmd);
	{
		char *p;
		for (p = lcmd; *p; p++)
			*p = ((*p >= 'A') && (*p <= 'Z')) ? (*p + 32) : *p;
	}
	lua_pushstring(lua, lcmd);
	free(lcmd);
	lua_gettable(lua, -2);
	lua_remove(lua, -2);
	if (!lua_isfunction(lua, -1))
	{
		lua_remove(lua, -1);
		return 0;
	}
	lua_pushstring(lua, arg);
	_push_conn_t(lua, conn);
	if (lua_pcall(lua, 2, 0, 0) != 0)
	{
		status_echof(conn, "LUA function \"%s\" returned an error: \"%s\"", cmd, lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}
	return 1;			//
}
