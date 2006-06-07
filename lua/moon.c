/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <naim/naim.h>
#include "naim-int.h"

#ifdef ENABLE_LUA

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "default_lua.h"

static lua_State *lua = NULL;
extern conn_t *curconn;

/* Note: This replaces stuff that was previously handled by SECS.
 * Notable changes --
 *  * Previously, SECS allowed *all* characters in variable names. Now, nLua
 *    requires variable names to match the regexp "[a-zA-Z0-9:_]+".  nLua may
 *    not barf if you don't satisfy this, but it might not always expand
 *    properly. 
 *  * Previously, the scripting engine was called SECS. It is now
 *    called Lua, and is interfaced by Moon. Lua is Portuguese for Moon.
 */

extern void (*script_client_cmdhandler)(const char *);

static void _loadfunctions();
static void _getmaintable();
static void _getvarstable();
static void _getconnstable();

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

static void _getmaintable()
{
	lua_getglobal(lua, "naim");
	if (!lua_istable(lua, -1))
	{
		printf("Argh! naim's main table in Lua went away! I can't do anything "
			   "intelligent now, so I'm just going to explode in a burst of "
			   "flame and let you sort out your buggy scripts.\n");
		abort();	
	}
}

static void _getvarstable()
{
	_getmaintable();
	lua_pushstring(lua, "variables");
	lua_gettable(lua, -2);
	lua_remove(lua, -2);
	if (!lua_istable(lua, -1))
	{
		printf("Argh! naim's vars table in Lua went away! I can't do anything "
			   "intelligent now, so I'm just going to explode in a burst of "
			   "flame and let you sort out your buggy scripts.\n");
		abort();
	}
}

static void _getconnstable()
{
	_getmaintable();
	lua_pushstring(lua, "connections");
	lua_gettable(lua, -2);
	lua_remove(lua, -2);
	if (!lua_istable(lua, -1))
	{
		printf("Argh! naim's conns table in Lua went away! I can't do anything "
			   "intelligent now, so I'm just going to explode in a burst of "
			   "flame and let you sort out your buggy scripts.\n");
		abort();
	}
}

int nlua_setvar_int(const char *name, const long value)
{
	if (name == NULL)
		return;
	
	_getvarstable();
	lua_pushstring(lua, name);
	lua_pushnumber(lua, value);
	lua_settable(lua, -3);			/* Load the stuff into the table. */
	lua_pop(lua, 1);			/* Clean up table. */
	return(1);
}

int nlua_setvar(const char *const name, const char *const value)
{
	if ((name == NULL) || (value == NULL))
		return(0);
	
	_getvarstable();
	lua_pushstring(lua, name);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);			/* Load the stuff into the table. */
	lua_pop(lua, 1);			/* Clean up table. */
	return(1);
}

long nlua_getvar_int(const char *const name)
{
	long result;
	
	assert(name != NULL);
	
	_getvarstable();
	lua_pushstring(lua, name);
	lua_gettable(lua, -2);			/* Pull it out of the table. */
	/* XXX error handling? */
	if (lua_isnumber(lua, -1))
		result = (long)lua_tonumber(lua, -1);
	else
		result = 0;
	lua_pop(lua, 2);			/* Clean up value, table. */
	return result;
	
}

char* nlua_getvar_safe(const char *const name, char** buf)
{
	assert(name != NULL);
	
	_getvarstable();
	lua_pushstring(lua, name);
	lua_gettable(lua, -2);			/* Pull it out of the table. */

	if (lua_isstring(lua, -1))		/* Remember that LUA is loosely typed, so a Number would actually give us a string representation! */
		*buf = strdup(lua_tostring(lua, -1));
	else
		*buf = NULL;
	lua_pop(lua, 2);			/* Clean up value, table. */
	
	return *buf;
}

char* nlua_getvar(const char *const name)
{
	static char* result = NULL;
	
	if (result)
		free(result);	/* XXX this is really bad behavior. I need to strdup, though, so... */
	
	return nlua_getvar_safe(name, &result);
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
	static char *last = NULL;
	const char* result;
	
	if (last)
		free(last);
	
	_getmaintable();
	lua_pushstring(lua, "__expandString");
	lua_gettable(lua, -2);
	lua_pushstring(lua, script);
	lua_pcall(lua, 1, 1, 0);		/* Feed the error message to the caller if there is one */
	result = lua_tostring(lua, -1);
	last = strdup(result);
	lua_pop(lua, 2);
	return last;
}

void nlua_listvars_start()
{
	_getvarstable();
	lua_pushnil(lua);
}

char* nlua_listvars_next()
{
	static char* last = NULL;
	
	if (last)
		free(last);
	if (lua_next(lua, -2) == 0)
	{
		lua_pushnil(lua);	/* dummy for nlua_listvars_stop */
		last = NULL;
		return NULL;
	}
	lua_pop(lua, 1);

	last = strdup(lua_tostring(lua, -1));
	return last;
}

void nlua_listvars_stop()
{
	lua_pop(lua, 2);
}

struct conn_id_map { int id; conn_t *conn; };

static struct conn_id_map* connidmaps = NULL;
static int connidmapsize = 0;
static int connidmapallocated = 0;
static int connidmapmaxid = 0;

static int _newconn_id(conn_t *conn)
{
	if ((connidmapsize + 1) > connidmapallocated)
	{
		connidmapallocated += 4;
		connidmaps = realloc(connidmaps, connidmapallocated * sizeof(struct conn_id_map));
	}
	connidmapmaxid++;
	connidmaps[connidmapsize].id = connidmapmaxid-1;
	connidmaps[connidmapsize].conn = conn;
	connidmapsize++;
	return connidmapmaxid-1;
}

static int _lookup_conn(conn_t *conn)
{
	int i;
	for (i=0; i<connidmapsize; i++)
		if (connidmaps[i].conn == conn)
			return connidmaps[i].id;
	abort();
}

static conn_t *_lookup_conn_id(int id)
{
	int i;
	for (i=0; i<connidmapsize; i++)
		if (connidmaps[i].id == id)
			return connidmaps[i].conn;
	return NULL;
}

static void _remove_conn(conn_t *conn)
{
	int i;
	for (i=0; i<connidmapsize; i++)
		if (connidmaps[i].conn == conn)
		{
			memcpy(&connidmaps[i], &connidmaps[i+1], (connidmapsize - i - 1) * sizeof(struct conn_id_map));
			connidmapsize--;
			return;
		}
	abort();
}

static void _push_conn_t(lua_State *L, conn_t *conn)
{
	_getconnstable();								// table
	lua_pushnumber(lua, _lookup_conn(conn));		// table number
	lua_gettable(lua, -2);							// table conn
	lua_remove(lua, -2);	// remove the table; we now have the conn table on the stack
}

static conn_t *_get_conn_t(lua_State *L, int index)
{
	int i;
	lua_pushstring(lua, "__id");
	lua_gettable(lua, index);
	i = lua_tonumber(lua, -1);
	lua_pop(lua, 1);
	return _lookup_conn_id(i);	
}

void nlua_hook_newconn(conn_t *conn)
{
	_getmaintable();
	lua_pushstring(lua, "__newConn");
	lua_gettable(lua, -2);						// maintable newconn
	lua_pushnumber(lua, _newconn_id(conn));		// maintable newconn connid
	if (lua_pcall(lua, 1, 0, 0))				
		lua_pop(lua, 1);
	lua_pop(lua, 1);							// maintable
												//
}

void nlua_hook_delconn(conn_t *conn)
{
	_getmaintable();							// maintable
	lua_pushstring(lua, "__delConn");			// maintable "delconn"
	lua_gettable(lua, -2);						// maintable delconn
	lua_pushnumber(lua, _lookup_conn(conn));	// maintable delconn connid
	if (lua_pcall(lua, 1, 0, 0))
		lua_pop(lua, 1);
	lua_pop(lua, 1);							//maintable
	_remove_conn(conn);							// 
}

#define CONN_STRING_GET(accessor, varname) \
	int l___conn_get_##accessor (lua_State *L)\
	{\
		int id = lua_tonumber(L, 1);\
		conn_t *conn = _lookup_conn_id(id);\
		if (!conn)\
		{\
			status_echof(curconn, "Whoops -- dereference attempt for %d", id);\
			lua_pushstring(L, "This connection is no longer valid. You should not be maintaining a reference to anything in the naim table between calls to your functions.");\
			return lua_error(L);\
		}\
		lua_pushstring(L, conn->varname?conn->varname:"");\
		return 1;\
	}

CONN_STRING_GET(sn, sn)
CONN_STRING_GET(password, password)
CONN_STRING_GET(winname, winname)
CONN_STRING_GET(server, server)
CONN_STRING_GET(profile, profile)

int nlua_luacmd(char *cmd, char *arg, conn_t *conn)
{
	char *lcmd;
	
	_getmaintable();							// maintable
	lua_pushstring(lua, "commands");			// maintable "commands"
	lua_gettable(lua, -2);						// maintable commands
	lua_remove(lua, -2);						// commands
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
	lua_pushstring(lua, lcmd);					// commands lcmd
	free(lcmd);
	lua_gettable(lua, -2);						// commands command
	lua_remove(lua, -2);						// command
	if (!lua_isfunction(lua, -1))
	{
		lua_remove(lua, -1);					//
		return 0;
	}
	lua_pushstring(lua, arg);					// command string
	_push_conn_t(lua, conn);					// command string conn
	if (lua_pcall(lua, 2, 0, 0) != 0)
	{
		status_echof(conn, "LUA function \"%s\" returned an error: \"%s\"", cmd, lua_tostring(lua, -1));
		lua_pop(lua, 1);
	}
	return 1;			//
}

int l_status_echof(lua_State *L)
{
	/* lua_pushlightuserdata(L, void *p) */
	conn_t *conn = _get_conn_t(L, 1);
	const char *s = lua_tostring(L, 2);
	
	if (!conn)
	{
		lua_pushstring(L, "conn was nil");
		return lua_error(L);
	}
	status_echof(conn, "%s", s);
	return 0;
}

int l_debug(lua_State *L)
{
	const char *s = lua_tostring(L, 1);
	status_echof(curconn, "%s", s);
	return 0;
}

int l_conio(lua_State *L)
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

int l_curconn(lua_State *L)
{
	_push_conn_t(L, curconn);
	return 1;
}

static const struct luaL_Reg naimlib [] = {
	{"status_echof", l_status_echof},
	{"debug", l_debug},
	{"curconn", l_curconn},
	{"conio", l_conio},
	{"__conn_get_sn", l___conn_get_sn},
	{"__conn_get_password", l___conn_get_password},
	{"__conn_get_winname", l___conn_get_winname},
	{"__conn_get_server", l___conn_get_server},
	{"__conn_get_profile", l___conn_get_profile},
	{NULL, NULL} /* sentinel */
};

static void _loadfunctions()
{
	luaL_register(lua, "naim", naimlib);
}

#endif
