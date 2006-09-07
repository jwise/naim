/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <stdarg.h>
#include "moon-int.h"

#if 0
static int _literal_index(lua_State *L, const int index) {
	const int top = lua_gettop(L);

	assert(top >= 0);

	if ((index < 0) && (-index <= top))
		return(top + index + 1);
	return(index);
}

static void _print_stack(lua_State *L) {
	const int top = lua_gettop(L);
	int	i;

	fprintf(stderr, "stack = {");
	for (i = 1; i <= top; i++) {
		fprintf(stderr, " %i = (%s)", i, lua_typename(L, lua_type(L, i)));
		if (lua_isstring(L, i))
			fprintf(stderr, "\"%s\"", lua_tostring(L, i));
		else if (lua_isnumber(L, i))
			fprintf(stderr, "%li", (long)lua_tonumber(L, i));
	}
	fprintf(stderr, " }\r\n");
}
#endif

static void _replace_entv(lua_State *L, const char *name, va_list msg) {
	const int top = lua_gettop(L);

	assert(top > 0);

	while (name != NULL) {
		const char *dot;

		assert(lua_istable(L, top));

		if (!lua_istable(L, top)) {			// { t }
			luaL_error(L, "trying to look up stack[%i][...] but stack[%i] is not a table (it is a %s)\r\n", top, top, lua_typename(L, lua_type(L, top)));
			abort(); /* NOTREACH */
		}

		if ((dot = strchr(name, '.')) != NULL) {
			lua_pushlstring(L, name, dot-name);	// { NAME, t }
			name = dot+1;
		} else {
			lua_pushstring(L, name);		// { NAME, t }
			name = va_arg(msg, const char *);
		}
		lua_gettable(L, top);				// { t[NAME], t }
		lua_replace(L, top);				// { t[NAME] }
		assert(lua_gettop(L) == top);
		//_print_stack(L);
	}

	assert(lua_gettop(L) == top);
}

void	_get_entv(lua_State *L, int index, const char *name, va_list msg) {
	const int top = lua_gettop(L);

	if (name == NULL)
		return;

	if (!lua_istable(L, index)) {
		luaL_error(L, "trying to look up %i[..] but %i is not a table (it is a %s)", index, index, lua_typename(L, lua_type(L, index)));
		abort(); /* NOTREACH */
	}

	lua_pushvalue(L, index);				// { t }

	if (!lua_istable(L, -1)) {
		luaL_error(L, "made a copy of %i to -1 but -1 is not a table (it is a %s)", index, lua_typename(L, lua_type(L, -1)));
		abort(); /* NOTREACH */
	}

	_replace_entv(L, name, msg);				// { t[NAME] }

	assert(lua_gettop(L) == top+1);
}

void	_get_ent(lua_State *L, const int index, const char *name, ...) {
	va_list	msg;

	va_start(msg, name);
	_get_entv(L, index, name, msg);
	va_end(msg);
}

void	_get_global_entv(lua_State *L, const char *name, va_list msg) {
	_get_entv(L, LUA_GLOBALSINDEX, name, msg);
}

void	_get_global_ent(lua_State *L, const char *name, ...) {
	const int top = lua_gettop(L);
	va_list	msg;

	if (name == NULL)
		return;

	va_start(msg, name);
	_get_global_entv(L, name, msg);
	va_end(msg);

	assert(lua_gettop(L) == top+1);
}
