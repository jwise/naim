/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 2007 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <stdlib.h>
#include <string.h>
#include "firetalk-int.h"
#include "moon-int.h"

static int _nlua_create(lua_State *L) {
	firetalk_sock_t *sock;
	sock = firetalk_sock_t_new();
	
	lua_newtable(L);
	
	lua_pushstring(L, "userdata");
	lua_pushlightuserdata(L, (void*)sock);
	lua_settable(L, -3);
	
	lua_newtable(L);
	lua_pushstring(L, "__index");
	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.socket.internal", 1) != NULL)
		return luaL_error(L, "failed to look up metatable for buffer library");
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
	
	return 1;
}

const struct luaL_reg naim_socketlib[] = {
	{ "create",	_nlua_create },
	{ NULL,		NULL }
};

#define STACK_TO_SOCKET(L, stack, sock) \
	do {\
		lua_pushvalue(L, stack);\
		if (!lua_istable(L, -1))\
			return luaL_error(L, "argument 1 was not a table");\
		lua_pushstring(L, "userdata");\
		lua_gettable(L, -2);\
		if (!lua_isuserdata(L, -1))\
			return luaL_error(L, "argument 1 did not have a userdata");\
		sock = lua_touserdata(L, -1);\
		lua_pop(L, 2);\
		if (!firetalk_sock_t_valid(sock))\
			return luaL_error(L, "argument 1's userdata wasn't a firetalk_sock_t");\
	} while(0)

static int _nlua_delete(lua_State *L) {
	firetalk_sock_t *sock;
	
	STACK_TO_SOCKET(L, 1, sock);
	firetalk_sock_t_delete(sock);
	lua_pushvalue(L, 1);
	lua_pushstring(L, "userdata");
	lua_pushnil(L);
	lua_settable(L, -3);
	lua_pop(L, 1);
	
	return 0;
}

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
	int len;
	fte_t error;
	
	STACK_TO_SOCKET(L, 1, sock);
        if (!lua_isstring(L, 2))
        	return luaL_error(L, "argument 2 was not a string");
	data = lua_tolstring(L, 2, &len);
	error = firetalk_sock_send(sock, data, len);
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

const struct luaL_reg naim_socket_internallib[] = {
	{ "delete",	_nlua_delete },
	{ "connect",	_nlua_connect },
	{ "close",	_nlua_close },
	{ "send",	_nlua_send },
	{ NULL,		NULL }
};
