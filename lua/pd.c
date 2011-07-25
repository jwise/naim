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
	int	i, args = 0, top = lua_gettop(lua), buffers = 0;

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

	args++;
	lua_pushvalue(lua, -2);	/* duplicate it for the 'self' argument */
	lua_remove(lua, -3);
	
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
		  case HOOK_T_BUFFERc: {
		  		/* this is nasty, but functional. */
		  		firetalk_buffer_t *val = va_arg(msg, firetalk_buffer_t *);
		  		
		  		lua_newtable(lua);
		  		
		  		lua_pushstring(lua, "data");
		  		lua_pushlstring(lua, val->buffer, val->pos);
		  		lua_settable(lua, -3);
		  		
		  		lua_pushstring(lua, "taken");
		  		lua_pushnumber(lua, 0);
		  		lua_settable(lua, -3);
		  		
		  		lua_pushstring(lua, "userdata");
		  		lua_pushlightuserdata(lua, (void*)val);
		  		lua_settable(lua, -3);
		  		
		  		lua_pushvalue(lua, -1);
		  		lua_insert(lua, -(args+2));	/* +1 for the function and +1 to get to the buffer */
		  		buffers++;
		  		top++;
		  		break;
			}
		  default:
			lua_pushlightuserdata(lua, va_arg(msg, void *));
			break;
		}
	}

	if (lua_pcall(lua, args, LUA_MULTRET, 0) != 0) {
		extern conn_t *curconn;

		status_echof(curconn, "[pdlua] [%s:%s] run error: %s\n", c->pd->pd->strprotocol, call, lua_tostring(lua, -1));
		lua_pop(lua, 1+buffers);
		return(-1);
	}

	assert((lua_gettop(lua) == top) || (lua_gettop(lua) == top+1));

	if (lua_gettop(lua) == top)
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

	for (i = 0; i < buffers; i++)
	{
		firetalk_buffer_t *val;
		int taken;
		
		lua_getfield(lua, -1, "taken");
		taken = lua_tonumber(lua, -1);
		lua_pop(lua, 1);
		
		lua_getfield(lua, -1, "userdata");
		val = (firetalk_buffer_t*)lua_touserdata(lua, -1);
		lua_pop(lua, 2);
		
		if (taken > (val->pos))
		{
			extern conn_t *curconn;
			
			status_echof(curconn, "[pdlua] [%s:%s] Oh no! Took %d bytes, but only %d bytes were available!", c->pd->pd->strprotocol, call, taken, val->pos);
			taken = val->pos;
		}
		memmove(val->buffer, val->buffer + taken, val->size - taken);
		val->pos -= taken;
	}

	assert(lua_gettop(lua) == (top - buffers));

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

static fte_t _nlua_pd_disconnected(struct firetalk_driver_connection_t *c, const fte_t reason) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "disconnected", HOOK_T_STRING, firetalk_strerror(reason));
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
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
PD_CALL_WRAPPER(got_data, HOOK_T_BUFFER) /* firetalk_buffer_t *buffer */
PD_CALL_WRAPPER(got_data_connecting, HOOK_T_BUFFER) /* firetalk_buffer_t *buffer */
PD_CALL_WRAPPER(disconnect, "") /* */
PD_CALL_WRAPPER(signon, HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(get_info, HOOK_T_STRING) /* const char *const account */
PD_CALL_WRAPPER(set_info, HOOK_T_STRING) /* const char *const text */
PD_CALL_WRAPPER(set_away, HOOK_T_STRING HOOK_T_UINT32) /* const char *const text, const int isauto */
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
	if ((ret == 0) && (val.t == HOOK_T_STRINGc))
		return(val.u.string);
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
		static const char *s;
		s = lua_tostring(lua, -1); /* for GDB */
		status_echof(curconn, "[pdlua] [%s:create] run error: %s\n", cookie->pd->strprotocol, s);
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
	got_data:		_nlua_pd_got_data,
	got_data_connecting:	_nlua_pd_got_data_connecting,
	comparenicks:		_nlua_pd_comparenicks,
	isprintable:		_nlua_pd_isprintable,
	disconnect:		_nlua_pd_disconnect,
	disconnected:		_nlua_pd_disconnected,
	signon:			_nlua_pd_signon,
	get_info:		_nlua_pd_get_info,
	set_info:		_nlua_pd_set_info,
	set_away:		_nlua_pd_set_away,
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
#warning No further warnings are harmless, unless you're using gcc 4.
};

static int _nlua_create(lua_State *L) {
	struct firetalk_driver_cookie_t *ck;
	
	ck = malloc(sizeof(*ck));

	ck->pd = malloc(sizeof(*ck->pd));
	memmove(ck->pd, &firetalk_protocol_template, sizeof(*ck->pd));

	lua_pushstring(L, "name");
	lua_gettable(L, 1);
	ck->pd->strprotocol = strdup(lua_tostring(L, -1));
	
	lua_pushstring(L, "server");
	lua_gettable(L, 1);
	ck->pd->default_server = strdup(lua_tostring(L, -1));
	
	lua_pushstring(L, "port");
	lua_gettable(L, 1);
	ck->pd->default_port = lua_tonumber(L, -1);
	
	lua_pushstring(L, "buffersize");
	lua_gettable(L, 1);
	ck->pd->default_buffersize = lua_tonumber(L, -1);
	
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

static int _nlua_connected(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	firetalk_callback_connected(c);
	
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

static int _nlua_send_data(lua_State *L) {
	struct firetalk_driver_connection_t *c;
	firetalk_connection_t *fchandle;
	const char *s;
	size_t sz;
	
	c = _nlua_pd_lua_to_driverconn(L, 1);
	if (!c)
		return luaL_error(L, "expected a connection for argument #1");
	s = luaL_checklstring(L, 2, &sz);
	
	fchandle = firetalk_find_conn(c);
	firetalk_internal_send_data(fchandle, s, sz);
	
	return(0);
}

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

const struct luaL_reg naim_pd_internallib[] = {
	{ "connected",		_nlua_connected },
	{ "needpass",		_nlua_needpass },
	{ "send_data",		_nlua_send_data },
	{ "im_get_message", 	_nlua_im_getmessage },
	{ "chat_get_message", 	_nlua_chat_getmessage },
	{ "chat_joined",	_nlua_chat_joined },
	{ "im_buddyonline",	_nlua_im_buddyonline },
	{ "buddyadded",		_nlua_buddyadded },
	{ NULL,			NULL },
};
