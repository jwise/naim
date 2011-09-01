/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** | | | | |_| || || |  | | Copyright 2006 Joshua Wise <joshua@joshuawise.com>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "firetalk-int.h"
#include "moon-int.h"

struct firetalk_driver_cookie_t {
	firetalk_driver_t *pd;
	int pdtref;
};

struct firetalk_driver_connection_t {
	struct firetalk_driver_cookie_t *pd;
	int conntref;
};

typedef struct {
	int	t;
	union {
		conn_t	*conn;
		char	*string;
		uint32_t u32;
		double	f;
	} u;
} multival_t;


/* for loc > 0 */
struct firetalk_driver_connection_t *_nlua_pd_lua_to_driverconn(lua_State *L, int loc)
{
	struct firetalk_driver_connection_t *c;
	
	lua_pushstring(L, "userdata");
	lua_gettable(L, loc);
	c = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	return c;
}
                

static int _nlua_pdvcall(multival_t *ret, struct firetalk_driver_connection_t *c, const char *call, const char *signature, va_list msg) {
	int	i, args = 0, top = lua_gettop(lua), nret;

	if (!c)
	{
		extern conn_t *curconn;
		status_echof(curconn, "[pdlua] [NULL:%s] c was null!\n", call);
		return -1;
	}

	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.pds", 1) != NULL) {
		extern conn_t *curconn;
		
		status_echof(curconn, "[pdlua] [%s:%s] failed to look up naim.internal.pds\n", c->pd->pd->strprotocol, call);
		return -1;
	}
	lua_rawgeti(lua, -1, c->conntref); //You can retrieve an object referred by reference r by calling lua_rawgeti(L, t, r).
	lua_remove(lua, -2);
	
	lua_pushstring(lua, call);
	lua_gettable(lua, -2);

	lua_pushvalue(lua, -2);	/* duplicate it for the 'self' argument */
	lua_remove(lua, -3);
	
	nret = _call_hook(lua, 1, 0, signature, msg);
	if (nret < 0)
		return -1;
	
	assert(nret == 0 || nret == 1);
	
	if (nret == 0)
		ret->t = -1;
	else {
		int	t = lua_type(lua, top+1);

		switch (t) {
		  case LUA_TTABLE:
			ret->t = HOOK_T_CONNc;
			ret->u.conn = _get_conn_t(lua, top+1);
			break;
		  case LUA_TSTRING:
			ret->t = HOOK_T_STRINGc;
			ret->u.string = strdup(lua_tostring(lua, top+1));
			_garbage_add(ret->u.string);
			break;
		  case LUA_TBOOLEAN:
			ret->t = HOOK_T_UINT32c;
			ret->u.u32 = lua_tointeger(lua, top+1);
			break;
		  case LUA_TNUMBER:
			ret->t = HOOK_T_FLOATc;
			ret->u.f = lua_tonumber(lua, top+1);
			break;
		  default:
			ret->t = -1;
		}
		lua_pop(lua, 1);
	}

	assert(lua_gettop(lua) == top);

	return(0);
}

static int _nlua_pdcall(multival_t *ret, struct firetalk_driver_connection_t *c, const char *call, const char *signature, ...) {
	va_list msg;
	int i;
	
	va_start(msg, signature);
	i = _nlua_pdvcall(ret, c, call, signature, msg);
	va_end(msg);
	return i;
}
  
static fte_t _nlua_pd_periodic(firetalk_connection_t *const conn) {
	return(FE_SUCCESS);
}

#define PD_CALL_WRAPPER(name, signature) \
	static fte_t _nlua_pd_##name(struct firetalk_driver_connection_t *c, ...) { \
		multival_t val; \
		int ret; \
		va_list msg; \
		\
		va_start(msg, c); \
		ret = _nlua_pdvcall(&val, c, #name , signature, msg); \
		if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc))) \
			return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f); \
		return(FE_UNKNOWN); \
	}

PD_CALL_WRAPPER(comparenicks, HOOK_T_STRING HOOK_T_STRING) /* const char *const s1, const char *const s2 */
PD_CALL_WRAPPER(isprintable, HOOK_T_UINT32) /* const int key */
PD_CALL_WRAPPER(preselect, HOOK_T_FDSET HOOK_T_FDSET HOOK_T_FDSET HOOK_T_WRUINT32) /* fd_set *read, fd_set *write, fd_set *except, int *n */
PD_CALL_WRAPPER(postselect, HOOK_T_FDSET HOOK_T_FDSET HOOK_T_FDSET) /* fd_set *read, fd_set *write, fd_set *except */
PD_CALL_WRAPPER(disconnect, "") /* */
PD_CALL_WRAPPER(connect, HOOK_T_STRING HOOK_T_UINT32 HOOK_T_STRING) /* const char *server, uint16_t port, const char *const username */
PD_CALL_WRAPPER(get_info, HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(set_info, HOOK_T_STRING) /* const char *const text */
PD_CALL_WRAPPER(set_away, HOOK_T_STRING HOOK_T_UINT32) /* const char *const text, const int isauto */
PD_CALL_WRAPPER(set_available, HOOK_T_STRING) /* const char *const text */
PD_CALL_WRAPPER(set_nickname, HOOK_T_STRING HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(set_password, HOOK_T_STRING HOOK_T_STRING) /* const char *const password, const char *const password2 */
PD_CALL_WRAPPER(set_privacy, HOOK_T_STRING) /* const char *const flag */
PD_CALL_WRAPPER(im_add_buddy, HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING) /* const char *const account, const char *const group, const char *const friendly */
PD_CALL_WRAPPER(im_remove_buddy, HOOK_T_STRING HOOK_T_STRING) /* const char *const account, const char *const group */
PD_CALL_WRAPPER(im_add_deny, HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(im_remove_deny, HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(im_send_message, HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32) /* const char *const account, const char *const text, const int isauto */
PD_CALL_WRAPPER(im_send_action, HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32) /* const char *const account, const char *const text, const int isauto */
PD_CALL_WRAPPER(im_evil, HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(chat_join, HOOK_T_STRING) /* const char *const group */
PD_CALL_WRAPPER(chat_part, HOOK_T_STRING) /* const char *const group */
PD_CALL_WRAPPER(chat_invite, HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING) /* const char *const group, const char *const account, const char *const text */
PD_CALL_WRAPPER(chat_set_topic, HOOK_T_STRING HOOK_T_STRING) /* const char *const group, const char *const text */
PD_CALL_WRAPPER(chat_op, HOOK_T_STRING HOOK_T_STRING) /* const char *const group, const char *const account */
PD_CALL_WRAPPER(chat_deop, HOOK_T_STRING HOOK_T_STRING) /* const char *const group, const char *const account */
PD_CALL_WRAPPER(chat_kick, HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING) /* const char *const group, const char *const account, const char *const text */
PD_CALL_WRAPPER(chat_send_message, HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32) /* const char *const group, const char *const text, const int isauto */
PD_CALL_WRAPPER(chat_send_action, HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32) /* const char *const group, const char *const text, const int isauto */

static char *_nlua_pd_subcode_encode(struct firetalk_driver_connection_t *c, const char *const command, const char *const text) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "subcode_encode", HOOK_T_STRING HOOK_T_STRING, command, text);
	
	/* Note that the returned value is already in the 'garbage' system
	 * and hence must be strdup'ed again so that dequeue_subcode_requests
	 * can free it later. 
	 */
	if ((ret == 0) && (val.t == HOOK_T_STRINGc))
		return(strdup(val.u.string)); 
	return(NULL);
}

static const char *_nlua_pd_room_normalize(struct firetalk_driver_connection_t *c, const char *const group) {
	multival_t val;
	int	ret;
	extern conn_t *curconn;

	ret = _nlua_pdcall(&val, c, "room_normalize", HOOK_T_STRING, group);
	if ((ret == 0) && (val.t == HOOK_T_STRINGc))
		return(val.u.string);
	return(NULL);
}

static struct firetalk_driver_connection_t *_nlua_pd_create_conn(struct firetalk_driver_cookie_t *cookie) {
	struct firetalk_driver_connection_t *c;
	int top = lua_gettop(lua);
	extern conn_t *curconn;
	
	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.pds", 1) != NULL) {
		status_echof(curconn, "[pdlua] [%s:create] failed to look up naim.internal.pds\n", cookie->pd->strprotocol);
		return NULL;
	}
	lua_rawgeti(lua, -1, cookie->pdtref); //You can retrieve an object referred by reference r by calling lua_rawgeti(L, t, r).
	lua_remove(lua, -2);
	
	lua_pushstring(lua, "create");
	lua_gettable(lua, -2);

	lua_pushvalue(lua, -2);	/* duplicate it for the 'self' argument */
	lua_remove(lua, -3);
	
	lua_pushstring(lua, cookie->pd->strprotocol);

	if (lua_pcall(lua, 2, LUA_MULTRET, 0) != 0) {
		status_echof(curconn, "[pdlua] [%s:create] run error: %s\n", cookie->pd->strprotocol, lua_tostring(lua, -1));
		lua_pop(lua, 1);
		assert(lua_gettop(lua) == top);
		return NULL;
	}

	if (lua_gettop(lua) == top) {
		status_echof(curconn, "[pdlua] [%s:create] return error: did not return anything\n", cookie->pd->strprotocol);
		assert(lua_gettop(lua) == top);
		return NULL;
	} else if (lua_type(lua, top+1) == LUA_TNIL) {
		const char *reason;
		reason = ((lua_gettop(lua)) > (top+1)) ? lua_tostring(lua, top+1) : "no reason";
		status_echof(curconn, "[pdlua] [%s:create] return error: returned nil, %s\n", cookie->pd->strprotocol, reason);
		assert(lua_gettop(lua) == top);
		return NULL;
	} else if (lua_type(lua, top+1) != LUA_TTABLE) {
		status_echof(curconn, "[pdlua] [%s:create] return error: did not return a table\n", cookie->pd->strprotocol);
		assert(lua_gettop(lua) == top);
		return NULL;
	} 
	
	c = malloc(sizeof(*c));
	c->pd = cookie;

	/* There's got to be a better way to do this. */	
	lua_pushvalue(lua, top+1);
	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.pds", 1) != NULL)
	{
		status_echof(curconn, "[pdlua] [%s:create] pds table damaged\n", cookie->pd->strprotocol);
		free(c);
		lua_pop(lua, 1 + (lua_gettop(lua) - top));
		assert(lua_gettop(lua) == top);
		return NULL;
	}
	lua_pushvalue(lua, -2);
	
	lua_pushstring(lua, "userdata");
	lua_pushlightuserdata(lua, (void*)c);
	lua_settable(lua, -3);
	
	c->conntref = luaL_ref(lua, -2); //You can retrieve an object referred by reference r by calling lua_rawgeti(L, t, r).
	lua_pop(lua, 3);
	/* End crackedness. */

	lua_pop(lua, lua_gettop(lua) - top);

	assert(lua_gettop(lua) == top);

	return(c);
}

static void  _nlua_pd_destroy_conn(struct firetalk_driver_connection_t *c) {
	multival_t val;
	int	ret;
	extern conn_t *curconn;

	ret = _nlua_pdcall(&val, c, "destroy", "");
	if (luaL_findtable(lua, LUA_GLOBALSINDEX, "naim.internal.pds", 1) != NULL)
		status_echof(curconn, "[pdlua] [%s:destroy] pds table damaged\n", c->pd->pd->strprotocol);
	else {
		luaL_unref(lua, -1, c->conntref);
		lua_pop(lua, 1);
	}
	
	free(c);
}

static const firetalk_driver_t firetalk_protocol_template = {
#warning The following 29 warnings are harmless.
	periodic:		_nlua_pd_periodic,
	preselect:		_nlua_pd_preselect,
	postselect:		_nlua_pd_postselect,
	comparenicks:		_nlua_pd_comparenicks,
	isprintable:		_nlua_pd_isprintable,
	disconnect:		_nlua_pd_disconnect,
	connect:		_nlua_pd_connect,
	get_info:		_nlua_pd_get_info,
	set_info:		_nlua_pd_set_info,
	set_away:		_nlua_pd_set_away,
	set_available:		_nlua_pd_set_available,
	set_nickname:		_nlua_pd_set_nickname,
	set_password:		_nlua_pd_set_password,
	im_add_buddy:		_nlua_pd_im_add_buddy,
	im_remove_buddy:	_nlua_pd_im_remove_buddy,
	im_add_deny:		_nlua_pd_im_add_deny,
	im_remove_deny:		_nlua_pd_im_remove_deny,
	im_send_message:	_nlua_pd_im_send_message,
	im_send_action:		_nlua_pd_im_send_action,
	im_evil:		_nlua_pd_im_evil,
	chat_join:		_nlua_pd_chat_join,
	chat_part:		_nlua_pd_chat_part,
	chat_invite:		_nlua_pd_chat_invite,
	chat_set_topic:		_nlua_pd_chat_set_topic,
	chat_op:		_nlua_pd_chat_op,
	chat_deop:		_nlua_pd_chat_deop,
	chat_kick:		_nlua_pd_chat_kick,
	chat_send_message:	_nlua_pd_chat_send_message,
	chat_send_action:	_nlua_pd_chat_send_action,
//	subcode_send_request:	_nlua_pd_subcode_send_request,
//	subcode_send_reply:	_nlua_pd_subcode_send_reply,
	subcode_encode:		_nlua_pd_subcode_encode,
	set_privacy:		_nlua_pd_set_privacy,
	room_normalize:		_nlua_pd_room_normalize,
	create_conn:		_nlua_pd_create_conn,
	destroy_conn:		_nlua_pd_destroy_conn,
#warning No further warnings are harmless, unless you are using gcc 4.
};

static int _nlua_create(lua_State *L) {
	struct firetalk_driver_cookie_t *ck;
	extern conn_t *curconn;
	
	ck = malloc(sizeof(*ck));

	ck->pd = malloc(sizeof(*ck->pd));
	memmove(ck->pd, &firetalk_protocol_template, sizeof(*ck->pd));

	lua_pushstring(L, "name");
	lua_gettable(L, 1);
	if (lua_type(L, -1) != LUA_TSTRING)
		return luaL_error(L, "pd did not contain name");
	ck->pd->strprotocol = strdup(lua_tostring(L, -1));
	lua_pop(L, 1);

	status_echof(curconn, "[pdlua] [%s register]\n", ck->pd->strprotocol);
	
	ck->pd->cookie = ck;
	
	if (luaL_findtable(L, LUA_GLOBALSINDEX, "naim.internal.pds", 1) != NULL)
	{
		free(ck->pd);
		free(ck);
		return luaL_error(L, "pds table damaged");
	}
	lua_pushvalue(L, 1);
	ck->pdtref = luaL_ref(L, -2); //You can retrieve an object referred by reference r by calling lua_rawgeti(L, t, r).
	lua_pop(L, 2);

	firetalk_register_protocol(ck->pd);

	return(0);
}

const struct luaL_reg naim_pdlib[] = {
	{ "create",	_nlua_create },
	{ NULL,		NULL }
};

static int _nlua_im_getmessage(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *sender;
	int automsg;
	const char *msg;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	sender = luaL_checkstring(L, 2);
	automsg = luaL_checkint(L, 3);
	msg = luaL_checkstring(L, 4);
	
	firetalk_callback_im_getmessage(c, sender, automsg, msg);
	
	return(0);
}

static int _nlua_im_getaction(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *sender;
	int automsg;
	const char *msg;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	sender = luaL_checkstring(L, 2);
	automsg = luaL_checkint(L, 3);
	msg = luaL_checkstring(L, 4);
	
	firetalk_callback_im_getaction(c, sender, automsg, msg);
	
	return(0);
}

static int _nlua_im_buddyonline(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy;
	int online;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	buddy = luaL_checkstring(L, 2);
	online = luaL_checkint(L, 3);
	
	firetalk_callback_im_buddyonline(c, buddy, online);
	
	return(0);
}

static int _nlua_im_buddyaway(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy;
	int away;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	buddy = luaL_checkstring(L, 2);
	away = luaL_checkint(L, 3);
	
	firetalk_callback_im_buddyaway(c, buddy, away);
	
	return(0);
}

static int _nlua_im_buddyflags(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy;
	int flags;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	buddy = luaL_checkstring(L, 2);
	flags = luaL_checkint(L, 3);
	
	firetalk_callback_im_buddyflags(c, buddy, flags);
	
	return(0);
}

static int _nlua_buddyadded(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy, *group, *friendly;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");

	buddy = luaL_checkstring(L, 2);
	group = lua_tostring(L, 3);
	friendly = lua_tostring(L, 4);
	
	firetalk_callback_buddyadded(c, buddy, group, friendly);
	
	return(0);
}

static int _nlua_buddyremoved(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy, *group;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");

	buddy = luaL_checkstring(L, 2);
	group = lua_tostring(L, 3);
	
	firetalk_callback_buddyremoved(c, buddy, group);
	
	return(0);
}

static int _nlua_typing(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *name;
	int typing;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");

	name = luaL_checkstring(L, 2);
	typing = luaL_checknumber(L, 3);
	
	firetalk_callback_typing(c, name, typing);
	
	return(0);
}

static int _nlua_error(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	int error;
	const char *sn, *s;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	error = luaL_checknumber(L, 2);
	sn = lua_tostring(L, 3);
	s = lua_tostring(L, 4);
	firetalk_callback_error(c, error, sn, s);
	
	return(0);
}

static int _nlua_connectfailed(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	int error;
	const char *s;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	error = luaL_checknumber(L, 2);
	s = luaL_checkstring(L, 3);
	firetalk_callback_connectfailed(c, error, s);
	
	return(0);
}

static int _nlua_connected(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	firetalk_callback_connected(c);
	
	return(0);
}

static int _nlua_disconnect(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	int error;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	error = luaL_checknumber(L, 2);
	firetalk_callback_disconnect(c, error);
	
	return(0);
}

static int _nlua_gotinfo(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *nick, *info;
	int warning, online, idle, flags;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");

	nick = luaL_checkstring(L, 2);
	info = luaL_checkstring(L, 3);
	warning = luaL_checknumber(L, 4);
	online = luaL_checknumber(L, 5);
	idle = luaL_checknumber(L, 6);
	flags = luaL_checknumber(L, 7);
	
	firetalk_callback_gotinfo(c, nick, info, warning, online, idle, flags);
	
	return(0);
}

static int _nlua_idleinfo(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy;
	long idletime;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	buddy = luaL_checkstring(L, 2);
	idletime = luaL_checkint(L, 3);
	
	firetalk_callback_idleinfo(c, buddy, idletime);
	
	return(0);
}

static int _nlua_statusinfo(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *buddy, *status;
	long idletime;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	buddy = luaL_checkstring(L, 2);
	status = luaL_checkstring(L, 3);
	
	firetalk_callback_statusinfo(c, buddy, status);
	
	return(0);
}

static int _nlua_doinit(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *nick;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	nick = luaL_checkstring(L, 2);
	firetalk_callback_doinit(c, nick);
	
	return(0);
}

static int _nlua_needpass(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	char pass[512];
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	firetalk_callback_needpass(c, pass, 512);
	lua_pushstring(L, pass);
	
	return(1);
}

static int _nlua_chat_getmessage(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *room;
	const char *sender;
	int automsg;
	const char *msg;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	room = luaL_checkstring(L, 2);
	sender = luaL_checkstring(L, 3);
	automsg = luaL_checkint(L, 4);
	msg = luaL_checkstring(L, 5);
	
	firetalk_callback_chat_getmessage(c, room, sender, automsg, msg);
	
	return(0);
}

static int _nlua_chat_joined(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *room;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	room = luaL_checkstring(L, 2);
	
	firetalk_callback_chat_joined(c, room);
	
	return(0);
}

static int _nlua_chat_left(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *room;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	room = luaL_checkstring(L, 2);
	
	firetalk_callback_chat_left(c, room);
	
	return(0);
}

/* Having this here is something of a kludge, but oh well.  We haven't got a
 * direct interface to talk to C Firetalk queues, but in practice, all Lua
 * will ever need is a dequeue function for each of these two queues.
 */

static int _nlua_dequeue_subcode_replies(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	firetalk_connection_t *fchandle;
	const char *nick;
	char *ect;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	nick = luaL_checkstring(L, 2);
	
	fchandle = firetalk_find_conn(c);
	
	ect = firetalk_dequeue(&(fchandle->subcode_replies), nick);
	if (!ect)
		return(0);
	
	lua_pushstring(L, ect);
	free(ect);
	
	return(1);
}

static int _nlua_dequeue_subcode_requests(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	firetalk_connection_t *fchandle;
	const char *nick;
	char *ect;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	nick = luaL_checkstring(L, 2);
	
	fchandle = firetalk_find_conn(c);
	
	ect = firetalk_dequeue(&(fchandle->subcode_requests), nick);
	if (!ect)
		return(0);
	
	lua_pushstring(L, ect);
	free(ect);
	
	return(1);
}

static int _nlua_subcode_request(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *from;
	const char *command;
	char *args;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	from = luaL_checkstring(L, 2);
	command = luaL_checkstring(L, 3);
	args = strdup(luaL_checkstring(L, 4));
	assert(args);
	
	firetalk_callback_subcode_request(c, from, command, args);
	
	free(args);
	
	return(0);
}

static int _nlua_subcode_reply(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	const char *from;
	const char *command;
	const char *args;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	from = luaL_checkstring(L, 2);
	command = luaL_checkstring(L, 3);
	args = luaL_checkstring(L, 4);
	
	firetalk_callback_subcode_reply(c, from, command, args);
	
	return(0);
}


const struct luaL_reg naim_pd_internallib[] = {
	{ "im_getmessage", 	_nlua_im_getmessage },
	{ "im_getaction", 	_nlua_im_getaction },
	{ "im_buddyonline",	_nlua_im_buddyonline },
	{ "im_buddyaway",	_nlua_im_buddyaway },
	{ "im_buddyflags",	_nlua_im_buddyflags },
	{ "buddyadded",		_nlua_buddyadded },
	{ "buddyremoved",	_nlua_buddyremoved },
	/* denyadded, denyremoved */
	{ "typing",		_nlua_typing },
	/* capabilities, warninfo */
	{ "error",		_nlua_error },
	{ "connectfailed",	_nlua_connectfailed },
	{ "connected",		_nlua_connected },
	{ "disconnect",		_nlua_disconnect },
	{ "gotinfo",		_nlua_gotinfo },
	{ "idleinfo",		_nlua_idleinfo },
	{ "statusinfo",		_nlua_statusinfo },
	{ "doinit",		_nlua_doinit },
	/* setidle, eviled, newnick, passchanged, user_nickchanged */
	{ "chat_joined",	_nlua_chat_joined },
	{ "chat_left",		_nlua_chat_left },
	/* chat_kicked */
	{ "chat_getmessage", 	_nlua_chat_getmessage },
	/* chat_getaction, chat_invited, chat_user_joined, chat_user_left, chat_user_quit, chat_gottopic, chat_modeset, chat_modunset */
	/* chat_user_opped, chat_user_deopped, chat_keychanged, chat_opped, chat_deopped, chat_user_kicked, subcode_get_request_reply */
	{ "subcode_request",	_nlua_subcode_request },
	{ "subcode_reply",	_nlua_subcode_reply },
	/* file_offer */
	{ "needpass",		_nlua_needpass },
	{ "dequeue_subcode_replies", _nlua_dequeue_subcode_replies },
	{ "dequeue_subcode_requests", _nlua_dequeue_subcode_requests },
	{ NULL,			NULL },
};

