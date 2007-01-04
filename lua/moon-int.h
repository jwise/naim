/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon-int.h Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <naim/naim.h>
#include <naim/modutil.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* conn.c */
void	_push_conn_t(lua_State *L, conn_t *conn);
conn_t	*_get_conn_t(lua_State *L, int index);

/* garbage.c */
void	nlua_clean_garbage(void);
void	_garbage_add(void *ptr);

/* moon.c */
extern lua_State *lua;

/* util.c */
void	_get_entv(lua_State *L, const int index, const char *name, va_list msg);
void	_get_ent(lua_State *L, const int index, const char *name, ...);
void	_get_global_entv(lua_State *L, const char *name, va_list msg);
void	_get_global_ent(lua_State *L, const char *name, ...);

/* socket.c */
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

/* buffer.c */
#define STACK_TO_BUFFER(L, stack, buffer) \
	do {\
		lua_pushvalue(L, stack);\
		if (!lua_istable(L, -1))\
			return luaL_error(L, "argument 1 was not a table");\
		lua_pushstring(L, "userdata");\
		lua_gettable(L, -2);\
		if (!lua_isuserdata(L, -1))\
			return luaL_error(L, "argument 1 did not have a userdata");\
		buffer = lua_touserdata(L, -1);\
		lua_pop(L, 2);\
		if (!firetalk_buffer_t_valid(buffer))\
			return luaL_error(L, "argument 1's userdata wasn't a firetalk_buffer_t");\
	} while(0)

