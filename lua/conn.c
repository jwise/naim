/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | conn.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <assert.h>
#include "moon-int.h"
#include <naim/naim.h>

extern faimconf_t faimconf;

#define NLUA_STRING_GET(ctype, varname) \
static int _nlua_ ## ctype ## _get_ ## varname(lua_State *L) { \
	ctype	*obj = (ctype *)lua_touserdata(L, 1); \
	\
	lua_pushstring(L, obj->varname?obj->varname:""); \
	return(1); \
}

static int _lua2conio(lua_State *L, int first, const char **args, const int argmax) {
	int	argc;

	for (argc = 0; (argc < argmax) && (lua_type(L, argc+first) != LUA_TNONE); argc++)
		args[argc] = lua_tostring(L, argc+first);

	return(argc);
}



void	nlua_hook_newconn(conn_t *conn) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "newconn", NULL);
	lua_pushstring(lua, conn->winname);
	lua_pushlightuserdata(lua, conn);
	if (lua_pcall(lua, 2, 0, 0))
		lua_pop(lua, 1);
	assert(lua_gettop(lua) == top);
}

void	nlua_hook_delconn(conn_t *conn) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "delconn", NULL);
	lua_pushstring(lua, conn->winname);
	if (lua_pcall(lua, 1, 0, 0))
		lua_pop(lua, 1);
	assert(lua_gettop(lua) == top);
}

void	_push_conn_t(lua_State *L, conn_t *conn) {
	const int top = lua_gettop(lua);

	if (conn != NULL)
		_get_global_ent(lua, "naim", "connections", conn->winname, NULL);
	else
		lua_pushnil(L);
	assert(lua_gettop(lua) == top+1);
}

conn_t	*_get_conn_t(lua_State *L, int index) {
	const int top = lua_gettop(L);
	conn_t	*obj;

	lua_pushstring(L, "handle");
	lua_gettable(L, index);
	obj = (conn_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);

	assert(lua_gettop(L) == top);
	return(obj);
}

NLUA_STRING_GET(conn_t, sn);
NLUA_STRING_GET(conn_t, password);
NLUA_STRING_GET(conn_t, winname);
NLUA_STRING_GET(conn_t, server);
NLUA_STRING_GET(conn_t, profile);

static int _nlua_conn_t_curwin(lua_State *L) {
	conn_t	*conn = _get_conn_t(L, 1);

	if (conn == NULL)
		return(luaL_error(L, "conn was nil"));
	if (conn->curbwin != NULL)
		_get_global_ent(L, "naim", "connections", conn->winname, "windows", conn->curbwin->winname, NULL);
	else
		_push_conn_t(L, conn);
	return(1);
}

static int _nlua_conn_t_status_echo(lua_State *L) {
	/* lua_pushlightuserdata(L, void *p) */
	conn_t	*conn = _get_conn_t(L, 1);
	const char *s = lua_tostring(L, 2);

	if (conn == NULL)
		return(luaL_error(L, "conn was nil"));
	status_echof(conn, "%s", s);
	return(0);
}

static int _nlua_conn_t_x_hprint(lua_State *L) {
	conn_t	*conn;
	const unsigned char *text;

	if ((conn = _get_conn_t(L, 1)) == NULL)
		return(luaL_error(L, "No connection object; use naim.connections[CONN]:x_hprint instead of naim.connections[CONN].x_hprint."));
	text = luaL_checkstring(L, 2);
	hwprintf(&(conn->nwin), C(CONN,TEXT), "%s", text);
	return(0);
}

#define NLUA_CONN_COMMAND(name) \
static int _nlua_conn_t_ ## name(lua_State *L) { \
	extern void ua_ ## name(conn_t *conn, int argc, const char **args); \
	conn_t *conn; \
	const char *args[UA_MAXPARMS]; \
	int	argc; \
	const char *error; \
	\
	if ((conn = _get_conn_t(L, 1)) == NULL) \
		return(luaL_error(L, "No connection object; use naim.connections[CONN]:" #name " instead of naim.connections[CONN]." #name ".")); \
	argc = _lua2conio(L, 2, args, UA_MAXPARMS); \
	if ((error = ua_valid(#name, conn, argc)) == NULL) \
		ua_ ## name(conn, argc, args); \
	else \
		return(luaL_error(L, "%s", error)); \
	return(0); \
}

#define CONN_COMMANDS \
	NN(echo) \
	NN(msg) \
	NN(say) \
	NN(me) \
	NN(ctcp) \
	NN(addbuddy) \
	NN(delbuddy) \
	NN(info) \
	NN(open) \
	NN(join) \
	NN(close) \
	NN(ignore) \
	NN(block) \
	NN(unblock) \
	NN(warn) \
	NN(setpriv) \
	NN(connect) \
	NN(server) \
	NN(disconnect) \

#define NN(x) NLUA_CONN_COMMAND(x)
CONN_COMMANDS
#undef NN

const struct luaL_reg naim_prototypes_connectionslib[] = {
	{ "get_sn",		_nlua_conn_t_get_sn },
	{ "get_password",	_nlua_conn_t_get_password },
	{ "get_winname",	_nlua_conn_t_get_winname },
	{ "get_server",		_nlua_conn_t_get_server },
	{ "get_profile",	_nlua_conn_t_get_profile },
	{ "curwin",		_nlua_conn_t_curwin },
	{ "status_echo",	_nlua_conn_t_status_echo },
	{ "x_hprint",		_nlua_conn_t_x_hprint },
#define NN(x) { #x, _nlua_conn_t_ ## x },
CONN_COMMANDS
#undef NN
	{ NULL, 		NULL}
};




void	nlua_hook_newwin(buddywin_t *bwin) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "newwin", NULL);
	_push_conn_t(lua, bwin->conn);
	lua_pushstring(lua, bwin->winname);
	lua_pushlightuserdata(lua, bwin);
	if (lua_pcall(lua, 3, 0, 0))
		lua_pop(lua, 3);
	assert(lua_gettop(lua) == top);
}

void	nlua_hook_delwin(buddywin_t *bwin) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "delwin", NULL);
	_push_conn_t(lua, bwin->conn);
	lua_pushstring(lua, bwin->winname);
	if (lua_pcall(lua, 2, 0, 0))
		lua_pop(lua, 2);
	assert(lua_gettop(lua) == top);
}

static buddywin_t *_get_buddywin_t(lua_State *L, int index) {
	const int top = lua_gettop(L);
	buddywin_t *obj;

	lua_pushstring(L, "handle");
	lua_gettable(L, index);
	obj = (buddywin_t *)lua_touserdata(L, -1);
	lua_pop(L, 1);

	assert(lua_gettop(lua) == top);
	return(obj);
}

NLUA_STRING_GET(buddywin_t, winname);

static int _nlua_buddywin_t_echo(lua_State *L) {
	buddywin_t *bwin;
	const char *str;

	if ((bwin = _get_buddywin_t(L, 1)) == NULL)
		return(luaL_error(L, "No buddywin object; use naim.connections[CONN].windows[BUDDY]:echo instead of naim.connections[CONN].windows[BUDDY].echo."));
	str = luaL_checkstring(L, 2);
	window_echof(bwin, "%s\n", str);
	return(0);
}

static int _nlua_buddywin_t_x_hprint(lua_State *L) {
	buddywin_t *bwin;
	const char *text;

	if ((bwin = _get_buddywin_t(L, 1)) == NULL)
		return(luaL_error(L, "No buddywin object; use naim.connections[CONN].windows[BUDDY]:x_hprint instead of naim.connections[CONN].windows[BUDDY].x_hprint."));
	text = luaL_checkstring(L, 2);
	hwprintf(&(bwin->nwin), C(IMWIN,TEXT), "%s", text);
	return(0);
}

static int _nlua_buddywin_t_notify(lua_State *L) {
	buddywin_t *bwin;

	if ((bwin = _get_buddywin_t(L, 1)) == NULL)
		return(luaL_error(L, "No buddywin object; use naim.connections[CONN].windows[BUDDY]:notify instead of naim.connections[CONN].windows[BUDDY].notify."));
	bwin->waiting = 1;
	bupdate();
	return(0);
}

#define NLUA_BUDDYWIN_T_COMMAND(name) \
static int _nlua_buddywin_t_ ## name(lua_State *L) { \
	extern void ua_ ## name(conn_t *conn, int argc, const char **args); \
	buddywin_t *bwin; \
	const char *args[UA_MAXPARMS]; \
	int	argc; \
	const char *error; \
	\
	if ((bwin = _get_buddywin_t(L, 1)) == NULL) \
		return(luaL_error(L, "No buddywin object; use naim.connections[CONN].windows[BUDDY]:" #name " instead of naim.connections[CONN].windows[BUDDY]." #name ".")); \
	args[0] = bwin->winname; \
	argc = _lua2conio(L, 2, args+1, UA_MAXPARMS-1)+1; \
	if ((error = ua_valid(#name, bwin->conn, argc)) == NULL) \
		ua_ ## name(bwin->conn, argc, args); \
	else \
		return(luaL_error(L, "%s", error)); \
	return(0); \
}

#define BUDDYWIN_COMMANDS \
	NN(msg) \
	NN(say)

#define NN(x) NLUA_BUDDYWIN_T_COMMAND(x)
BUDDYWIN_COMMANDS
#undef NN

const struct luaL_reg naim_prototypes_windowslib[] = {
	{ "get_winname",	_nlua_buddywin_t_get_winname },
	{ "echo",		_nlua_buddywin_t_echo },
	{ "x_hprint",		_nlua_buddywin_t_x_hprint },
	{ "notify",		_nlua_buddywin_t_notify },
#define NN(x) { #x, _nlua_buddywin_t_ ## x },
BUDDYWIN_COMMANDS
#undef NN
	{ NULL,			NULL }
};




void	nlua_hook_newbuddy(buddylist_t *buddy) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "newbuddy", NULL);
	_push_conn_t(lua, buddy->conn);
	lua_pushstring(lua, USER_ACCOUNT(buddy));
	lua_pushlightuserdata(lua, buddy);
	if (lua_pcall(lua, 3, 0, 0))
		lua_pop(lua, 3);
	assert(lua_gettop(lua) == top);
}

void	nlua_hook_changebuddy(buddylist_t *buddy, const char *newaccount) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "changebuddy", NULL);
	_push_conn_t(lua, buddy->conn);
	lua_pushstring(lua, USER_ACCOUNT(buddy));
	lua_pushstring(lua, newaccount);
	if (lua_pcall(lua, 3, 0, 0))
		lua_pop(lua, 3);
	assert(lua_gettop(lua) == top);
}

void	nlua_hook_delbuddy(buddylist_t *buddy) {
	const int top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "delbuddy", NULL);
	_push_conn_t(lua, buddy->conn);
	lua_pushstring(lua, USER_ACCOUNT(buddy));
	if (lua_pcall(lua, 2, 0, 0))
		lua_pop(lua, 2);
	assert(lua_gettop(lua) == top);
}

const struct luaL_reg naim_prototypes_buddieslib[] = {
	{ NULL,			NULL }
};
