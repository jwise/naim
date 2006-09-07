/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon-int.h Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <naim/naim.h>
#include <naim/modutil.h>
#include "naim-int.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* conn.c */
void	_push_conn_t(lua_State *L, conn_t *conn);

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
