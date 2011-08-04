/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 2007 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <stdlib.h>
#include <string.h>
#include "firetalk-int.h"
#include "moon-int.h"

static int _nlua_connect(lua_State *L) {
	firetalk_sock_t *sock;
	const char *host;
	int port;
	fte_t error;
	
	STACK_TO_SOCKET(L, 1, sock);
        if (!lua_isstring(L, 2))
        	return luaL_error(L, "argument 2 was not a string");
	if (!lua_isnumber(L, 3))
		return luaL_error(L, "argument 3 was not a number");
	host = lua_tostring(L, 2);
	port = lua_tonumber(L, 3);
	error = firetalk_sock_connect_host(sock, host, port);
	if (error == FE_SUCCESS)
		return 0;
	lua_pushstring(L, firetalk_strerror(error));
	return 1;
}

static int _nlua_send(lua_State *L) {
	firetalk_sock_t *sock;
	const char *data;
	size_t len;
	fte_t error;
	
	STACK_TO_SOCKET(L, 1, sock);
        if (!lua_isstring(L, 2))
        	return luaL_error(L, "argument 2 was not a string");
	data = lua_tolstring(L, 2, &len);
	if (sock->state != FCS_NOTCONNECTED)
		error = firetalk_sock_send(sock, data, len);
	else
		error = FE_NOTCONNECTED;
	if (error == FE_SUCCESS)
		return 0;
	lua_pushstring(L, firetalk_strerror(error));
	return 1;
}

static int _nlua_close(lua_State *L) {
	firetalk_sock_t *sock;
	
	STACK_TO_SOCKET(L, 1, sock);
	firetalk_sock_close(sock);
	return 0;
}

static int _nlua_preselect(lua_State *L) {
	firetalk_sock_t *sock;
	fd_set *rd, *wr, *ex;
	int n;
	
	STACK_TO_SOCKET(L, 1, sock);
	if (!lua_islightuserdata(L, 2))
		return luaL_error(L, "argument 2 was not a light userdata");	/* XXX do more checks later to make sure it's a fd_set */
	if (!lua_islightuserdata(L, 3))
		return luaL_error(L, "argument 3 was not a light userdata");	/* XXX do more checks later to make sure it's a fd_set */
	if (!lua_islightuserdata(L, 4))
		return luaL_error(L, "argument 4 was not a light userdata");	/* XXX do more checks later to make sure it's a fd_set */
	if (!lua_isnumber(L, 5))
		return luaL_error(L, "argument 5 was not a number");
	rd = lua_touserdata(L, 2);
	wr = lua_touserdata(L, 3);
	ex = lua_touserdata(L, 4);
	n = lua_tonumber(L, 5);
	
	if (sock->state != FCS_NOTCONNECTED)
		firetalk_sock_preselect(sock, rd, wr, ex, &n);

	lua_pushnumber(L, n);
	return 1;
}

static int _nlua_postselect(lua_State *L) {
	firetalk_sock_t *sock;
	fd_set *rd, *wr, *ex;
	firetalk_buffer_t *buf;
	fte_t e = FE_SUCCESS;
	
	STACK_TO_SOCKET(L, 1, sock);
	if (!lua_islightuserdata(L, 2))
		return luaL_error(L, "argument 2 was not a light userdata");	/* XXX do more checks later to make sure it's a fd_set */
	if (!lua_islightuserdata(L, 3))
		return luaL_error(L, "argument 3 was not a light userdata");	/* XXX do more checks later to make sure it's a fd_set */
	if (!lua_islightuserdata(L, 4))
		return luaL_error(L, "argument 4 was not a light userdata");	/* XXX do more checks later to make sure it's a fd_set */
	rd = lua_touserdata(L, 2);
	wr = lua_touserdata(L, 3);
	ex = lua_touserdata(L, 4);
	STACK_TO_BUFFER(L, 5, buf);
	
	if (sock->state != FCS_NOTCONNECTED)
		e = firetalk_sock_postselect(sock, rd, wr, ex, buf);
	if (sock->state == FCS_SEND_SIGNON)
		sock->state = FCS_ACTIVE;
	if (e == FE_SUCCESS)
		return 0;
	lua_pushnumber(L, e);
	return 1;
}

static int _nlua_connected(lua_State *L) {
	firetalk_sock_t *sock;
	
	STACK_TO_SOCKET(L, 1, sock);
	lua_pushboolean(L, sock->state == FCS_ACTIVE);
	return 1;
}

const static struct luaL_reg socket_internallib[] = {
	{ "connect",	_nlua_connect },
	{ "close",	_nlua_close },
	{ "send",	_nlua_send },
	{ "preselect",	_nlua_preselect },
	{ "postselect",	_nlua_postselect },
	{ "connected",	_nlua_connected },
	{ NULL,		NULL }
};

static int _nlua___gc(lua_State *L) {
	firetalk_sock_t *sock;
	
	STACK_TO_SOCKET(L, 1, sock);
	firetalk_sock_t_delete(sock);
	
	return 0;
}

static int _nlua_new(lua_State *L) {
	firetalk_sock_t **sock;
	static int hascreatedmetatable = 0;
	
	if (!hascreatedmetatable)
	{
		luaL_newmetatable(L, "naim.socket");
		
		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);
		luaL_openlib(L, NULL, socket_internallib, 0);
		
		lua_pushstring(L, "__gc");
		lua_pushcfunction(L, _nlua___gc);
		lua_settable(L, -3);
		
		lua_pop(L, 1);
		
		hascreatedmetatable = 1;
	}
	sock = lua_newuserdata(L, sizeof(firetalk_sock_t*));
	*sock = firetalk_sock_t_new();
	luaL_getmetatable(L, "naim.socket");
	lua_setmetatable(L, -2);
	
	return 1;
}

const struct luaL_reg naim_socketlib[] = {
	{ "new",	_nlua_new },
	{ NULL,		NULL }
};
