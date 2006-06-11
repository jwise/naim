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

/* moon.c */
extern lua_State *lua;

/* moon_conn.c */
extern void _push_conn_t(lua_State *L, conn_t *conn);

/* helpful inlines */
static inline void _getmaintable()
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

static inline void _getitem(const char *t)
{
	lua_pushstring(lua, t);
	lua_gettable(lua, -2);
	lua_remove(lua, -2);
}

static inline void _getsubtable(const char *t)
{
	_getmaintable();
	_getitem(t);
	if (!lua_istable(lua, -1))
	{
		printf("Argh! naim's \"%s\" table in Lua went away! I can't do anything "
			   "intelligent now, so I'm just going to explode in a burst of "
			   "flame and let you sort out your buggy scripts.\n", t);
		abort();
	}
}
