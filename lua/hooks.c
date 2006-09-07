/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include "moon-int.h"

extern conn_t *curconn;

static int _nlua_hook(void *userdata, const char *signature, ...) {
	va_list	msg;
	int	ref = (int)userdata, i, args = 0, recoverable = 0;

	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.hooks", 1) != NULL)
		abort();
	lua_rawgeti(lua, -1, ref);
	lua_remove(lua, -2);

	va_start(msg, signature);
	for (i = 0; signature[i] != 0; i++) {
		args++;
		switch(signature[i]) {
		  case HOOK_T_CONNc:
			_push_conn_t(lua, va_arg(msg, conn_t *));
			break;
		  case HOOK_T_STRINGc:
			lua_pushstring(lua, va_arg(msg, const char *));
			break;
		  case HOOK_T_LSTRINGc: {
				const char *str = va_arg(msg, const char *);
				uint32_t len = va_arg(msg, uint32_t);

				lua_pushlstring(lua, str, len);
				break;
			}
		  case HOOK_T_UINT32c:
			lua_pushnumber(lua, va_arg(msg, uint32_t));
			break;
		  case HOOK_T_FLOATc:
			lua_pushnumber(lua, va_arg(msg, double));
			break;
		  case HOOK_T_WRSTRINGc: {
				const char **str = va_arg(msg, const char **);

				lua_pushstring(lua, *str);
				recoverable++;
				break;
			}
		  case HOOK_T_WRLSTRINGc: {
				const char **str = va_arg(msg, const char **);
				uint32_t *len = va_arg(msg, uint32_t *);

				lua_pushlstring(lua, *str, *len);
				recoverable++;
				break;
			}
		  case HOOK_T_WRUINT32c: {
				uint32_t *val = va_arg(msg, uint32_t *);

				lua_pushnumber(lua, *val);
				recoverable++;
				break;
			}
		  case HOOK_T_WRFLOATc: {
				double *val = va_arg(msg, double *);

				lua_pushnumber(lua, *val);
				recoverable++;
				break;
			}
		  default:
			lua_pushlightuserdata(lua, va_arg(msg, void *));
			break;
		}
	}
	va_end(msg);

	if (lua_pcall(lua, args, 1+recoverable, 0) != 0) {
		status_echof(curconn, "Chain %d run error: %s\n", ref, lua_tostring(lua, -1));
		lua_pop(lua, 1);
		return(HOOK_CONTINUE);
	}
	if (!lua_isnumber(lua, -1)) {
		lua_pop(lua, 1);
		return(HOOK_CONTINUE);
	}
	i = lua_tonumber(lua, -1);
	lua_pop(lua, 1);

	return(i);
}

static int _nlua_hooks_add(lua_State *L) {
	void	*mod = NULL;
	const char *chainname;
	int	weight,
		ref;

	chainname = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	weight = luaL_checkint(L, 3);
	if (luaL_findtable(L, LUA_GLOBALSINDEX, "naim.internal.hooks", 1) != NULL)
		return(luaL_error(L, "Hooks table damaged"));
	lua_pushvalue(L, 2);
	ref = luaL_ref(L, -2); //You can retrieve an object referred by reference r by calling lua_rawgeti(L, t, r). 
	lua_pop(L, 2);

	HOOK_ADD2(chainname, mod, _nlua_hook, weight, (void *)ref);

	lua_pushlightuserdata(L, (void *)ref);	/* opaque reference */
	return(1);
}

static int _nlua_hooks_del(lua_State *L) {
	const char *chainname;
	int	ref;
	void	*mod = NULL;

	chainname = luaL_checkstring(L, 1);
	if (!lua_islightuserdata(L, 2))
		return(luaL_typerror(L, 1, "light userdata"));
	ref = (int)lua_touserdata(L, 2);

	HOOK_DEL2(chainname, mod, _nlua_hook, (void *)ref);

	if (luaL_findtable(L, LUA_GLOBALSINDEX, "naim.internal.hooks", 1) != NULL)
		return(luaL_error(L, "Hooks table damaged"));
	luaL_unref(L, -1, ref);
	lua_pop(L, 1);

	return(0);
}

const struct luaL_reg naim_hookslib[] = {
	{ "add",	_nlua_hooks_add },
	{ "del",	_nlua_hooks_del },
	{ NULL,		NULL } /* sentinel */
};
