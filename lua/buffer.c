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
	firetalk_buffer_t *buf;
	buf = firetalk_buffer_t_new();
	
	lua_newtable(L);
	
	lua_pushstring(L, "userdata");
	lua_pushlightuserdata(L, (void*)buf);
	lua_settable(L, -3);
	
	lua_newtable(L);
	lua_pushstring(L, "__index");
	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.buffer.internal", 1) != NULL)
		return luaL_error(L, "failed to look up metatable for buffer library");
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
	
	return 1;
}

const struct luaL_reg naim_bufferlib[] = {
	{ "create",	_nlua_create },
	{ NULL,		NULL }
};

#define STACK_TO_BUFFER(L, stack, buffer) \
	do {\
		lua_pushvalue(L, stack);\
		if (!lua_istable(L, -1))\
			return luaL_error(L, "argument 1 was not a table");\
		lua_pushstring(L, "userdata");\
		lua_gettable(L, -2);\
		if (!lua_isuserdata(L, -1))\
			return luaL_error(L, "argument 1 did not have a userdata");\
		buf = lua_touserdata(L, -1);\
		lua_pop(L, 2);\
		if (!firetalk_buffer_t_valid(buffer))\
			return luaL_error(L, "argument 1's userdata wasn't a firetalk_buffer_t");\
	} while(0)

static int _nlua_delete(lua_State *L) {
	firetalk_buffer_t *buf;
	
	STACK_TO_BUFFER(L, 1, buf);
	firetalk_buffer_t_delete(buf);
	lua_pushvalue(L, 1);
	lua_pushstring(L, "userdata");
	lua_pushnil(L);
	lua_settable(L, -3);
	lua_pop(L, 1);
	
	return 0;
}

static int _nlua_resize(lua_State *L) {
	firetalk_buffer_t *buf;
	int newsize;
	
	STACK_TO_BUFFER(L, 1, buf);
	if (!lua_isnumber(L, 2))
		return luaL_error(L, "argument 2 was not a number");
	newsize = lua_tonumber(L, 2);
	if (newsize == buf->size)
		return 0;
	if (newsize < buf->pos)
		return luaL_error(L, "too much unread data");
	buf->buffer = realloc(buf->buffer, newsize);
	buf->size = newsize;
	return 0;
}

/*
typedef struct {
	void	*magic;
	uint32_t size, pos;
	uint8_t	*buffer,
		readdata:1;
	void	*canary;
} firetalk_buffer_t;
*/

static int _nlua_peek(lua_State *L) {
	firetalk_buffer_t *buf;
	int size;
	
	STACK_TO_BUFFER(L, 1, buf);
	if (!lua_isnumber(L, 2))
		size = buf->size;
	else
		size = lua_tonumber(L, 2);
	if (size > buf->pos)
		size = buf->pos;
	lua_pushlstring(L, buf->buffer, size);
	return 1;
}

static int _nlua_take(lua_State *L) {
	firetalk_buffer_t *buf;
	int size;
	
	STACK_TO_BUFFER(L, 1, buf);
	if (!lua_isnumber(L, 2))
		size = buf->size;
	else
		size = lua_tonumber(L, 2);
	if (size > buf->pos)
		size = buf->pos;
	lua_pushlstring(L, buf->buffer, size);
	memmove(buf->buffer, buf->buffer + buf->pos, buf->size - buf->pos);
	buf->pos = 0;
	return 1;
}

const struct luaL_reg naim_buffer_internallib[] = {
	{ "delete",	_nlua_delete },
	{ "resize",	_nlua_resize },
	{ "peek",	_nlua_peek },
	{ "take",	_nlua_take },
	{ NULL,		NULL }
};
