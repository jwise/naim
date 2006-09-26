/*  _ __   __ _ ___ __  __
** | '_ \ / _` |_ _|  \/  | naim
** | | | | | | || || |\/| | Copyright 1998-2006 Daniel Reed <n@ml.org>
** |_| |_|\__,_|___|_|  |_| ncurses-based chat client
*/

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "firetalk-int.h"
#include "moon-int.h"

struct firetalk_driver_connection_t {
	firetalk_driver_t pd;
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

static int _nlua_pdcall(multival_t *ret, struct firetalk_driver_connection_t *c, const char *call, const char *signature, ...) {
	va_list	msg;
	int	i, args = 0, top = lua_gettop(lua);

	_get_global_ent(lua, "naim", "internal", "protos", c->pd.strprotocol, call, NULL);

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
	va_end(msg);

	if (lua_pcall(lua, args, LUA_MULTRET, 0) != 0) {
		extern conn_t *curconn;

		status_echof(curconn, "PD %s call %s run error: %s\n", c->pd.strprotocol, call, lua_tostring(lua, -1));
		lua_pop(lua, 1);
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

	assert(lua_gettop(lua) == top);

	return(0);
}

static fte_t _nlua_pd_periodic(firetalk_connection_t *const conn) {
	return(FE_SUCCESS);
}

static fte_t _nlua_pd_preselect(struct firetalk_driver_connection_t *c, fd_set *read, fd_set *write, fd_set *except, int *n) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "preselect", HOOK_T_FDSET HOOK_T_FDSET HOOK_T_FDSET HOOK_T_WRUINT32, read, write, except, n);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_postselect(struct firetalk_driver_connection_t *c, fd_set *read, fd_set *write, fd_set *except) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "postselect", HOOK_T_FDSET HOOK_T_FDSET HOOK_T_FDSET, read, write, except);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_got_data(struct firetalk_driver_connection_t *c, firetalk_buffer_t *buffer) {
//	multival_t val;
//	int	ret;
//
//	ret = _nlua_pdcall(&val, c, "got_data", HOOK_T
//	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
//		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_got_data_connecting(struct firetalk_driver_connection_t *c, firetalk_buffer_t *buffer) {
//	multival_t val;
//	int	ret;
//
//	ret = _nlua_pdcall(&val, c, "got_data_connecting", HOOK_T
//	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
//		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_comparenicks(const char *const s1, const char *const s2) {
//	multival_t val;
//	int	ret;
//
//	ret = _nlua_pdcall(&val, c, "comparenicks", HOOK_T_STRING HOOK_T_STRING, s1, s2);
//	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
//		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_isprintable(const int key) {
//	multival_t val;
//	int	ret;
//
//	ret = _nlua_pdcall(&val, c, "isprintable", HOOK_T_UINT32, key);
//	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
//		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_disconnect(struct firetalk_driver_connection_t *c) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "disconnect", "");
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_disconnected(struct firetalk_driver_connection_t *c, const fte_t reason) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "disconnected", HOOK_T_STRING, firetalk_strerror(reason));
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_signon(struct firetalk_driver_connection_t *c, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "signon", HOOK_T_STRING, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_get_info(struct firetalk_driver_connection_t *c, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "get_info", HOOK_T_STRING, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_set_info(struct firetalk_driver_connection_t *c, const char *const text) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "set_info", HOOK_T_STRING, text);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_set_away(struct firetalk_driver_connection_t *c, const char *const text, const int isauto) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "set_away", HOOK_T_STRING HOOK_T_UINT32, text, isauto);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_set_nickname(struct firetalk_driver_connection_t *c, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "set_nickname", HOOK_T_STRING, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_set_password(struct firetalk_driver_connection_t *c, const char *const password, const char *const password2) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "set_password", HOOK_T_STRING, password);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_set_privacy(struct firetalk_driver_connection_t *c, const char *const flag) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "set_privacy", HOOK_T_STRING, flag);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_add_buddy(struct firetalk_driver_connection_t *c, const char *const account, const char *const group, const char *const friendly) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_add_buddy", HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, account, group, friendly);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_remove_buddy(struct firetalk_driver_connection_t *c, const char *const account, const char *const group) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_remove_buddy", HOOK_T_STRING HOOK_T_STRING, account, group);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_add_deny(struct firetalk_driver_connection_t *c, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_add_deny", HOOK_T_STRING, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_remove_deny(struct firetalk_driver_connection_t *c, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_remove_deny", HOOK_T_STRING, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_send_message(struct firetalk_driver_connection_t *c, const char *const account, const char *const text, const int isauto) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_send_message", HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32, account, text, isauto);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_send_action(struct firetalk_driver_connection_t *c, const char *const account, const char *const text, const int isauto) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_send_action", HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32, account, text, isauto);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_im_evil(struct firetalk_driver_connection_t *c, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "im_evil", HOOK_T_STRING, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_join(struct firetalk_driver_connection_t *c, const char *const group) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_join", HOOK_T_STRING, group);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_part(struct firetalk_driver_connection_t *c, const char *const group) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_part", HOOK_T_STRING, group);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_invite(struct firetalk_driver_connection_t *c, const char *const group, const char *const account, const char *const text) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_invite", HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, group, account, text);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_set_topic(struct firetalk_driver_connection_t *c, const char *const group, const char *const text) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_set_topic", HOOK_T_STRING HOOK_T_STRING, group, text);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_op(struct firetalk_driver_connection_t *c, const char *const group, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_op", HOOK_T_STRING HOOK_T_STRING, group, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_deop(struct firetalk_driver_connection_t *c, const char *const group, const char *const account) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_deop", HOOK_T_STRING HOOK_T_STRING, group, account);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_kick(struct firetalk_driver_connection_t *c, const char *const group, const char *const account, const char *const text) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_kick", HOOK_T_STRING HOOK_T_STRING HOOK_T_STRING, group, account, text);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_send_message(struct firetalk_driver_connection_t *c, const char *const group, const char *const text, const int isauto) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_send_message", HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32, group, text, isauto);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static fte_t _nlua_pd_chat_send_action(struct firetalk_driver_connection_t *c, const char *const group, const char *const text, const int isauto) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "chat_send_action", HOOK_T_STRING HOOK_T_STRING HOOK_T_UINT32, group, text, isauto);
	if ((ret == 0) && ((val.t == HOOK_T_UINT32c) || (val.t == HOOK_T_FLOATc)))
		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
	return(FE_UNKNOWN);
}

static char *_nlua_pd_subcode_encode(struct firetalk_driver_connection_t *c, const char *const command, const char *const text) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "subcode_encode", HOOK_T_STRING HOOK_T_STRING, command, text);
	if ((ret == 0) && (val.t == HOOK_T_STRINGc))
		return(val.u.string);
	return(NULL);
}

static const char *_nlua_pd_room_normalize(const char *const group) {
//	multival_t val;
//	int	ret;
//
//	ret = _nlua_pdcall(&val, c, "room_normalize", HOOK_T_STRING, group);
//	if ((ret == 0) && (val.t == HOOK_T_STRING))
//		return(val.u.string);
//	return(FE_UNKNOWN);
	return(group);
}

static struct firetalk_driver_connection_t *_nlua_pd_create_conn(struct firetalk_driver_cookie_t *cookie) {
	struct firetalk_driver_connection_t *driver = (struct firetalk_driver_connection_t *)cookie;
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, driver, "create", HOOK_T_STRING, driver->pd.strprotocol);
//	if ((ret == 0) && (val.t == -1))
//		return((val.t == HOOK_T_UINT32c)?val.u.u32:val.u.f);
//	return(FE_UNKNOWN);
	return("");
}

static void  _nlua_pd_destroy_conn(struct firetalk_driver_connection_t *c) {
	multival_t val;
	int	ret;

	ret = _nlua_pdcall(&val, c, "destroy", "");
}

static const firetalk_driver_t firetalk_protocol_template = {
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
};

static int _nlua_create(lua_State *L) {
	firetalk_driver_t *pd;

	pd = malloc(sizeof(*pd));
	memmove(pd, &firetalk_protocol_template, sizeof(*pd));

	pd->strprotocol = strdup(lua_tostring(L, -4));
	pd->default_server = strdup(lua_tostring(L, -3));
	pd->default_port = lua_tonumber(L, -2);
	pd->default_buffersize = lua_tonumber(L, -1);
	pd->cookie = (struct firetalk_driver_cookie_t *)pd;

	firetalk_register_protocol(pd);

	return(0);
}

const struct luaL_reg naim_pdlib[] = {
	{ "create",	_nlua_create },
	{ NULL,		NULL }
};
