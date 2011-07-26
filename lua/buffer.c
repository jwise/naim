/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 2007 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <stdlib.h>
#include <string.h>
#include "firetalk-int.h"
#include "moon-int.h"

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
	memmove(buf->buffer, buf->buffer + size, buf->size - buf->pos);
	buf->pos -= size;
	return 1;
}

static int _nlua_pos(lua_State *L) {
	firetalk_buffer_t *buf;
	int size;
	
	STACK_TO_BUFFER(L, 1, buf);
	lua_pushnumber(L, buf->pos);
	return 1;
}

static int _nlua_readdata(lua_State *L) {
	firetalk_buffer_t *buf;
	
	STACK_TO_BUFFER(L, 1, buf);
	lua_pushboolean(L, buf->readdata);
	return 1;
}

const static struct luaL_reg buffer_internallib[] = {
	{ "resize",	_nlua_resize },
	{ "peek",	_nlua_peek },
	{ "take",	_nlua_take },
	{ "pos",	_nlua_pos },
	{ "readdata",	_nlua_readdata },
	{ NULL,		NULL }
};

static int _nlua___gc(lua_State *L) {
	firetalk_buffer_t *buf;
	
	STACK_TO_BUFFER(L, 1, buf);
	firetalk_buffer_t_delete(buf);
	lua_pop(L, 1);
	
	return 0;
}

static int _nlua_new(lua_State *L) {
	firetalk_buffer_t **buf;
	static int hascreatedmetatable = 0;
	
	if (!hascreatedmetatable)	/* we have to do it like this because we aren't given an opportunity to do this at startup */
	{
		luaL_newmetatable(L, "naim.buffer");
		
		lua_pushstring(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);
		luaL_openlib(L, NULL, buffer_internallib, 0);
		
		lua_pushstring(L, "__gc");
		lua_pushcfunction(L, _nlua___gc);
		lua_settable(L, -3);
		
		lua_pushstring(L, "__tostring");
		lua_pushcfunction(L, _nlua_peek);
		lua_settable(L, -3);
		
		lua_pop(L, 1);
		
		hascreatedmetatable = 1;
	}
	
	buf = lua_newuserdata(L, sizeof(firetalk_buffer_t*));
	*buf = firetalk_buffer_t_new();
	luaL_getmetatable(L, "naim.buffer");
	lua_setmetatable(L, -2);
	
	return 1;
}

const struct luaL_reg naim_bufferlib[] = {
	{ "new",	_nlua_new },
	{ NULL,		NULL }
};
