/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | moon.c Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <assert.h>
#include <stdlib.h>
#include "moon-int.h"

extern conn_t *curconn;

/* Returns number of Lua returns that were not eaten. */
int	_call_hook(lua_State *L, int npreargs, int nrets, const char *signature, va_list msg) {
	va_list msg2;
	int args = npreargs, top = lua_gettop(lua) - npreargs - 1, i, newtop;

	va_copy(msg2, msg);

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
				break;
			}
		  case HOOK_T_WRLSTRINGc: {
				const char **str = va_arg(msg, const char **);
				uint32_t *len = va_arg(msg, uint32_t *);

				lua_pushlstring(lua, *str, *len);
				break;
			}
		  case HOOK_T_WRUINT32c: {
				uint32_t *val = va_arg(msg, uint32_t *);

				lua_pushnumber(lua, *val);
				break;
			}
		  case HOOK_T_WRFLOATc: {
				double *val = va_arg(msg, double *);

				lua_pushnumber(lua, *val);
				break;
			}
		  default:
			lua_pushlightuserdata(lua, va_arg(msg, void *));
			break;
		}
	}

	if (lua_pcall(lua, args, LUA_MULTRET, 0) != 0) {
		status_echof(curconn, "Lua hook run error: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		return -1;
	}

	newtop = lua_gettop(lua);

	assert(top <= newtop);
	if (top == newtop)
		return 0;

	for (i = 0; (signature[i] != 0) && (top+nrets+1 <= newtop); i++) {
		switch (signature[i]) {
		  case HOOK_T_CONNc:
			va_arg(msg2, conn_t *);
			break;
		  case HOOK_T_STRINGc:
			va_arg(msg2, const char *);
			break;
		  case HOOK_T_LSTRINGc:
			va_arg(msg2, const char *);
			va_arg(msg2, uint32_t);
			break;
		  case HOOK_T_UINT32c:
			va_arg(msg2, uint32_t);
			break;
		  case HOOK_T_FLOATc:
			va_arg(msg2, double);
			break;
		  case HOOK_T_WRSTRINGc: {
				char	**str = va_arg(msg2, char **);
				const char *newstr = lua_tostring(lua, top+nrets+1);
				uint32_t len = strlen(newstr);

				if (strcmp(*str, newstr) != 0) {
					*str = realloc(*str, len+1);
					strcpy(*str, newstr);
				}

				lua_remove(lua, top+nrets+1);
				break;
			}
		  case HOOK_T_WRLSTRINGc: {
		  		size_t leni;
				char	**str = va_arg(msg2, char **);
				uint32_t *len = va_arg(msg2, uint32_t *);
				const char *newstr = lua_tolstring(lua, top+nrets+1, &leni);
				*len = leni;

				if (memcmp(*str, newstr, *len) != 0) {
					*str = realloc(*str, (*len)+1);
					memmove(*str, newstr, *len);
					(*str)[*len] = 0;
				}

				lua_remove(lua, top+nrets+1);
				break;
			}
		  case HOOK_T_WRUINT32c: {
				uint32_t *val = va_arg(msg2, uint32_t *),
					newval = lua_tonumber(lua, top+nrets+1);

				*val = newval;

				lua_remove(lua, top+nrets+1);
				break;
			}
		  case HOOK_T_WRFLOATc: {
				double	*val = va_arg(msg2, double *),
					newval = lua_tonumber(lua, top+nrets+1);

				if (*val != newval)
					*val = newval;

				lua_remove(lua, top+nrets+1);
				break;
			}
		  default:
			va_arg(msg2, void *);
			break;
		}
		newtop = lua_gettop(lua);
	}

	va_end(msg2);

	return(newtop - top);
}

static int _nlua_hook(void *userdata, const char *signature, ...) {
	va_list msg;
	int ref = (int)(long)userdata, nrets, top;

	top = lua_gettop(lua);

	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.hooks", 1) != NULL)
		abort();
	lua_rawgeti(lua, -1, ref);
	lua_remove(lua, -2);

	va_start(msg, signature);
	nrets = _call_hook(lua, 0, 1, signature, msg);
	va_end(msg);

	if (nrets >= 1)
		ref = lua_toboolean(lua, top+1)?HOOK_CONTINUE:HOOK_STOP;
	else
		ref = HOOK_CONTINUE;

	lua_pop(lua, lua_gettop(lua)-top);
	assert(lua_gettop(lua) == top);

	return(ref);
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

	HOOK_ADD2(chainname, mod, _nlua_hook, weight, (void *)(long)ref);

	lua_pushlightuserdata(L, (void *)(long)ref);	/* opaque reference */
	return(1);
}

static int _nlua_hooks_del(lua_State *L) {
	const char *chainname;
	int	ref;
	void	*mod = NULL;

	chainname = luaL_checkstring(L, 1);
	if (!lua_islightuserdata(L, 2))
		return(luaL_typerror(L, 1, "light userdata"));
	ref = (int)(long)lua_touserdata(L, 2);

	HOOK_DEL2(chainname, mod, _nlua_hook, (void *)(long)ref);

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
