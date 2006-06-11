/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon_conn.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <naim/naim.h>
#include <naim/modutil.h>
#include "naim-int.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "moon-int.h"

extern conn_t *curconn;

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
			memmove(&connidmaps[i], &connidmaps[i+1], (connidmapsize - i - 1) * sizeof(struct conn_id_map));
			connidmapsize--;
			return;
		}
	abort();
}

void _push_conn_t(lua_State *L, conn_t *conn)
{
	_getsubtable("connections");
	lua_pushstring(lua, conn->winname);
	lua_gettable(lua, -2);
	lua_remove(lua, -2);
}

static conn_t *_get_conn_t(lua_State *L, int index)
{
	int i;
	lua_pushstring(lua, "id");
	lua_gettable(lua, index);
	i = lua_tonumber(lua, -1);
	lua_pop(lua, 1);
	return _lookup_conn_id(i);	
}

void nlua_hook_newconn(conn_t *conn)
{
	_getsubtable("internal");
	_getitem("newConn");
	lua_pushnumber(lua, _newconn_id(conn));
	if (lua_pcall(lua, 1, 0, 0))				
		lua_pop(lua, 1);
}

void nlua_hook_delconn(conn_t *conn)
{
	_getsubtable("internal");
	_getitem("delConn");
	lua_pushnumber(lua, _lookup_conn(conn));
	if (lua_pcall(lua, 1, 0, 0))
		lua_pop(lua, 1);
	_remove_conn(conn);
}

#define CONN_STRING_GET(accessor, varname) \
	static int l_conn_get_##accessor (lua_State *L)\
	{\
		int id = lua_tonumber(L, 1);\
		conn_t *conn = _lookup_conn_id(id);\
		if (!conn)\
		{\
			lua_pushstring(L, "connection no longer valid");\
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

static int l_conn_status_echo(lua_State *L)
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

static int l_conn_echo(lua_State *L) {
	conn_t *conn = _get_conn_t(L, 1);
	const char *s = lua_tostring(L, 2);
	
	if (conn == NULL) {
		lua_pushstring(L, "conn was nil");
		return(lua_error(L));
	}
	echof(conn, NULL, "%s", s);
	return(0);
}

typedef struct {
	int	argc;
	const char *args[CONIO_MAXPARMS];
} _lua2conio_ret;

static _lua2conio_ret *_lua2conio(lua_State *L, int first) {
	static _lua2conio_ret ret;

	for (ret.argc = 0; (ret.argc < CONIO_MAXPARMS) && (lua_type(L, ret.argc+first) != LUA_TNONE); ret.argc++)
		ret.args[ret.argc] = lua_tostring(L, ret.argc+first);

	return(&ret);
}

static int l_conn_msg(lua_State *L) {
	extern void conio_msg(conn_t *conn, int argc, const char **args);
	conn_t *conn = _get_conn_t(L, 1);
	_lua2conio_ret *ret = _lua2conio(L, 2);
	const char *error;

	if ((error = conio_valid("msg", conn, ret->argc)) == NULL)
		conio_msg(conn, ret->argc, ret->args);
	else {
		lua_pushstring(L, error);
		return(lua_error(L));
	}
	return(0);
}

const struct luaL_reg naimprototypeconnlib[] = {
	{"get_sn", l_conn_get_sn},
	{"get_password", l_conn_get_password},
	{"get_winname", l_conn_get_winname},
	{"get_server", l_conn_get_server},
	{"get_profile", l_conn_get_profile},
	{"status_echo", l_conn_status_echo},
	{"echo", l_conn_echo},
	{"msg", l_conn_msg},
	{NULL, NULL}
};
