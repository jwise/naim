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

/* Bump this whenever you add a feature that a client could reasonably want
 * to depend on.  */
#define NLUA_API_VERSION 3

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
		firetalk_sock_t **ss;\
		ss = (firetalk_sock_t**)luaL_checkudata(L, stack, "naim.socket");\
		luaL_argcheck(L, ss != NULL, stack, "`socket' expected");\
		sock = *ss;\
		luaL_argcheck(L, firetalk_sock_t_valid(sock), stack, "invalid firetalk_sock_t (this should never happen)");\
	} while(0)

/* buffer.c */
#define STACK_TO_BUFFER(L, stack, buf) \
	do {\
		firetalk_buffer_t **bb;\
		bb = (firetalk_buffer_t**)luaL_checkudata(L, stack, "naim.buffer");\
		luaL_argcheck(L, bb != NULL, stack, "`buffer' expected");\
		buf = *bb;\
		luaL_argcheck(L, firetalk_buffer_t_valid(buf), stack, "invalid firetalk_buffer_t (this should never happen)");\
	} while(0)

/* hooks.c */
int	_call_hook(lua_State *L, int npreargs, int nrets, const char *signature, va_list msg);
