#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "firetalk-int.h"
#include "rpc.h"

void	ftrpc_prettyprint(char *prefix, char *buf, int buflen);

static int remote_handlerpc(firetalk_t conn) {
	char	*buf = conn->rpcpeer.buffer;
	int	buflen = conn->rpcpeer.bufferpos;
	uint32_t funcnum;

	fprintf(stderr, "REMOTE read some data; looking for complete headers\r\n");
	ftrpc_prettyprint("REMOTE", buf, buflen);

	if (strncmp(buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(0);
	buf += sizeof("FTRPC01 ")-1;
	buflen -= sizeof("FTRPC01 ")-1;

	if (ftrpc01_get_uint32(&buf, &buflen, &funcnum, sizeof(funcnum)) != 0)
		return(0);

	if (buflen == 0)
		return(0);

	fprintf(stderr, "REMOTE attempting to dispatch funcnum=%u (may fail if arguments or END not yet in buffer)\r\n", funcnum);

	switch (funcnum) {
	  case FTRPC_FUNC_COMPARENICKS: {
			const char *a, *b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->comparenicks(conn, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_ISPRINTABLE: {
			uint32_t a, ret;

			if (ftrpc01_expect_uint32(&buf, &buflen, &a) != 0)
				return(0);
			ret = conn->PD->isprintable(conn, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_DISCONNECT: {
			uint32_t ret;

			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->disconnect(conn, conn->handle);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CONNECT: {
			const char *a, *d;
			uint16_t b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint16(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->connect(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SENDPASS: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->sendpass(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SAVE_CONFIG: {
			uint32_t ret;

			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->save_config(conn, conn->handle);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_GET_INFO: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->get_info(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SET_INFO: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->set_info(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SET_AWAY: {
			const char *a;
			uint32_t b, ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->set_away(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SET_NICKNAME: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->set_nickname(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SET_PASSWORD: {
			const char *a, *b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->set_password(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SET_PRIVACY: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->set_privacy(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_ADD_BUDDY: {
			const char *a, *b, *d;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_add_buddy(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_REMOVE_BUDDY: {
			const char *a, *b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_remove_buddy(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_ADD_DENY: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_add_deny(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_REMOVE_DENY: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_remove_deny(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_UPLOAD_BUDDIES: {
			uint32_t ret;

			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_upload_buddies(conn, conn->handle);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_UPLOAD_DENIES: {
			uint32_t ret;

			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_upload_denies(conn, conn->handle);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_SEND_MESSAGE: {
			const char *a, *b;
			uint32_t d, ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_send_message(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_SEND_ACTION: {
			const char *a, *b;
			uint32_t d, ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_send_action(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_IM_EVIL: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->im_evil(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_JOIN: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_join(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_PART: {
			const char *a;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_part(conn, conn->handle, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_INVITE: {
			const char *a, *b, *d;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_invite(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_SET_TOPIC: {
			const char *a, *b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_set_topic(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_OP: {
			const char *a, *b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_op(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_DEOP: {
			const char *a, *b;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_deop(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_KICK: {
			const char *a, *b, *d;
			uint32_t ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_kick(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_SEND_MESSAGE: {
			const char *a, *b;
			uint32_t d, ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_send_message(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_CHAT_SEND_ACTION: {
			const char *a, *b;
			uint32_t d, ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->chat_send_action(conn, conn->handle, a, b, d);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_UINT32, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_SUBCODE_ENCODE: {
			const char *a, *b, *ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->subcode_encode(conn, conn->handle, a, b);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_STRING, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_ROOM_NORMALIZE: {
			const char *a, *ret;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			ret = conn->PD->room_normalize(conn, a);
			while (ftrpc_send(conn, FTRPC_FUNC_REPLY, FTRPC_TYPE_STRING, ret, FTRPC_TYPE_END) != 0)
				sleep(1);
			break;
		}
	  case FTRPC_FUNC_DESTROY_HANDLE: {
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			fprintf(stderr, "REMOTE asking PD to clean itself up\r\n");
			conn->PD->destroy_handle(conn, conn->handle);
			fprintf(stderr, "REMOTE exitting\r\n");
			_exit(0);
			break;
		}
	  default:
		abort();
		return(0);
	}

	memmove(conn->rpcpeer.buffer, buf, buflen);
	conn->rpcpeer.bufferpos = buflen;

	fprintf(stderr, "REMOTE successfully dispatched one RPC (%i bytes left to dispatch)\r\n", conn->rpcpeer.bufferpos);

	return(1);
}

firetalk_t remote_mainloop(firetalk_t conn) {
	while (1) {
		fd_set	readfds, writefds, exceptfds;
		int	n = 0;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);
		firetalk_sock_preselect(&(conn->rpcpeer), &readfds, &writefds, &exceptfds, &n);
		fprintf(stderr, "REMOTE preselect %i\r\n", getpid());
		firetalk_select_custom(n, &readfds, &writefds, &exceptfds, NULL);
		fprintf(stderr, "REMOTE postselect %i\r\n", getpid());
		if (firetalk_sock_postselect(&(conn->rpcpeer), &readfds, &writefds, &exceptfds) != FE_SUCCESS) {
fprintf(stderr, "postselect returned error or disconnect\r\n");
			break;
		}
//		if (conn->rpcpeer.readdata)
			while ((conn->rpcpeer.bufferpos > 0) && (remote_handlerpc(conn) > 0))
				;
	}

	sleep(1);

fprintf(stderr, "child shut down\r\n");

	_exit(0);
}

static void remote_im_getmessage(firetalk_t conn, client_t c, const char *const sender, const int automessage, const char *const message) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_IM_GETMESSAGE, FTRPC_TYPE_STRING, sender, FTRPC_TYPE_UINT32, automessage, FTRPC_TYPE_STRING, message, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_im_getaction(firetalk_t conn, client_t c, const char *const sender, const int automessage, const char *const message) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_IM_GETACTION, FTRPC_TYPE_STRING, sender, FTRPC_TYPE_UINT32, automessage, FTRPC_TYPE_STRING, message, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_im_buddyonline(firetalk_t conn, client_t c, const char *const nickname, const int online) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_IM_BUDDYONLINE, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_UINT32, online, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_im_buddyaway(firetalk_t conn, client_t c, const char *const nickname, const int away) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_IM_BUDDYAWAY, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_UINT32, away, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_buddyadded(firetalk_t conn, client_t c, const char *const name, const char *const group, const char *const friendly) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_BUDDYADDED, FTRPC_TYPE_STRING, name, FTRPC_TYPE_STRING, group, FTRPC_TYPE_STRING, friendly, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_buddyremoved(firetalk_t conn, client_t c, const char *const name, const char *const group) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_BUDDYREMOVED, FTRPC_TYPE_STRING, name, FTRPC_TYPE_STRING, group, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_typing(firetalk_t conn, client_t c, const char *const name, const int typing) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_TYPING, FTRPC_TYPE_STRING, name, FTRPC_TYPE_UINT32, typing, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_capabilities(firetalk_t conn, client_t c, char const *const nickname, const char *const caps) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CAPABILITIES, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_STRING, caps, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_warninfo(firetalk_t conn, client_t c, char const *const nickname, const long warnval) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_WARNINFO, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_UINT32, warnval, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_error(firetalk_t conn, client_t c, const int error, const char *const roomoruser, const char *const description) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_ERROR, FTRPC_TYPE_UINT32, error, FTRPC_TYPE_STRING, roomoruser, FTRPC_TYPE_STRING, description, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_connectfailed(firetalk_t conn, client_t c, const int error, const char *const description) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CONNECTFAILED, FTRPC_TYPE_UINT32, error, FTRPC_TYPE_STRING, description, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_connected(firetalk_t conn, client_t c) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CONNECTED, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_disconnect(firetalk_t conn, client_t c, const int error) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_DISCONNECT, FTRPC_TYPE_UINT32, error, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_gotinfo(firetalk_t conn, client_t c, const char *const nickname, const char *const info, const int warning, const long online, const long idle, const int flags) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_GOTINFO, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_STRING, info, FTRPC_TYPE_UINT32, warning, FTRPC_TYPE_UINT32, online, FTRPC_TYPE_UINT32, idle, FTRPC_TYPE_UINT32, flags, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_idleinfo(firetalk_t conn, client_t c, char const *const nickname, const long idletime) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_IDLEINFO, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_UINT32, idletime, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_doinit(firetalk_t conn, client_t c, char const *const nickname) {
	fprintf(stderr, "REMOTE sending doinit(%s)\r\n", nickname);
	while (ftrpc_send(conn, FTRPC_CALLBACK_DOINIT, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_END) != 0)
		sleep(1);
	fprintf(stderr, "REMOTE sent doinit\r\n");
}

static void remote_setidle(firetalk_t conn, client_t c, long *const idle) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_SETIDLE, FTRPC_TYPE_UINT32, *idle, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_eviled(firetalk_t conn, client_t c, const int newevil, const char *const eviler) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_EVILED, FTRPC_TYPE_UINT32, newevil, FTRPC_TYPE_STRING, eviler, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_newnick(firetalk_t conn, client_t c, const char *const nickname) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_NEWNICK, FTRPC_TYPE_STRING, nickname, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_passchanged(firetalk_t conn, client_t c) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_PASSCHANGED, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_user_nickchanged(firetalk_t conn, client_t c, const char *const oldnick, const char *const newnick) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_USER_NICKCHANGED, FTRPC_TYPE_STRING, oldnick, FTRPC_TYPE_STRING, newnick, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_joined(firetalk_t conn, client_t c, const char *const room) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_JOINED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_left(firetalk_t conn, client_t c, const char *const room) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_LEFT, FTRPC_TYPE_STRING, room, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_kicked(firetalk_t conn, client_t c, const char *const room, const char *const by, const char *const reason) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_KICKED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, by, FTRPC_TYPE_STRING, reason, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_getmessage(firetalk_t conn, client_t c, const char *const room, const char *const from, const int automessage, const char *const message) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_GETMESSAGE, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, from, FTRPC_TYPE_UINT32, automessage, FTRPC_TYPE_STRING, message, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_getaction(firetalk_t conn, client_t c, const char *const room, const char *const from, const int automessage, const char *const message) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_GETACTION, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, from, FTRPC_TYPE_UINT32, automessage, FTRPC_TYPE_STRING, message, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_invited(firetalk_t conn, client_t c, const char *const room, const char *const from, const char *const message) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_INVITED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, from, FTRPC_TYPE_STRING, message, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_user_joined(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const extra) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_USER_JOINED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, who, FTRPC_TYPE_STRING, extra, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_user_left(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const reason) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_USER_LEFT, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, who, FTRPC_TYPE_STRING, reason, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_user_quit(firetalk_t conn, client_t c, const char *const who, const char *const reason) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_USER_QUIT, FTRPC_TYPE_STRING, who, FTRPC_TYPE_STRING, reason, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_gottopic(firetalk_t conn, client_t c, const char *const room, const char *const topic, const char *const author) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_GOTTOPIC, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, topic, FTRPC_TYPE_STRING, author, FTRPC_TYPE_END) != 0)
		sleep(1);
}

#ifdef RAWIRCMODES
static void remote_chat_modechanged(firetalk_t conn, client_t c, const char *const room, const char *const mode, const char *const by) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_MODECHANGED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, mode, FTRPC_TYPE_STRING, by, FTRPC_TYPE_END) != 0)
		sleep(1);
}
#endif

static void remote_chat_user_opped(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_USER_OPPED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, who, FTRPC_TYPE_STRING, by, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_user_deopped(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_USER_DEOPPED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, who, FTRPC_TYPE_STRING, by, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_keychanged(firetalk_t conn, client_t c, const char *const room, const char *const what, const char *const by) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_KEYCHANGED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, what, FTRPC_TYPE_STRING, by, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_opped(firetalk_t conn, client_t c, const char *const room, const char *const by) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_OPPED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, by, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_deopped(firetalk_t conn, client_t c, const char *const room, const char *const by) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_DEOPPED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, by, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_chat_user_kicked(firetalk_t conn, client_t c, const char *const room, const char *const who, const char *const by, const char *const reason) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_CHAT_USER_KICKED, FTRPC_TYPE_STRING, room, FTRPC_TYPE_STRING, who, FTRPC_TYPE_STRING, by, FTRPC_TYPE_STRING, reason, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_subcode_request(firetalk_t conn, client_t c, const char *const from, const char *const command, char *args) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_SUBCODE_REQUEST, FTRPC_TYPE_STRING, from, FTRPC_TYPE_STRING, command, FTRPC_TYPE_STRING, args, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_subcode_reply(firetalk_t conn, client_t c, const char *const from, const char *const command, const char *const args) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_SUBCODE_REPLY, FTRPC_TYPE_STRING, from, FTRPC_TYPE_STRING, command, FTRPC_TYPE_STRING, args, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_file_offer(firetalk_t conn, client_t c, const char *const from, const char *const filename, const long size, const char *const ipstring, const char *const ip6string, const uint16_t port, const int type) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_FILE_OFFER, FTRPC_TYPE_STRING, from, FTRPC_TYPE_STRING, filename, FTRPC_TYPE_UINT32, size, FTRPC_TYPE_STRING, ipstring, FTRPC_TYPE_STRING, ip6string, FTRPC_TYPE_UINT16, port, FTRPC_TYPE_UINT32, type, FTRPC_TYPE_END) != 0)
		sleep(1);
}

static void remote_needpass(firetalk_t conn, client_t c) {
	while (ftrpc_send(conn, FTRPC_CALLBACK_NEEDPASS, FTRPC_TYPE_END) != 0)
		sleep(1);
}

const firetalk_PI_t firetalk_PI_remote = {
	im_getmessage:		remote_im_getmessage,
	im_getaction:		remote_im_getaction,
	im_buddyonline:		remote_im_buddyonline,
	im_buddyaway:		remote_im_buddyaway,
	buddyadded:		remote_buddyadded,
	buddyremoved:		remote_buddyremoved,
	typing:			remote_typing,
	capabilities:		remote_capabilities,
	warninfo:		remote_warninfo,
	error:			remote_error,
	connectfailed:		remote_connectfailed,
	connected:		remote_connected,
	disconnect:		remote_disconnect,
	gotinfo:		remote_gotinfo,
	idleinfo:		remote_idleinfo,
	doinit:			remote_doinit,
	setidle:		remote_setidle,
	eviled:			remote_eviled,
	newnick:		remote_newnick,
	passchanged:		remote_passchanged,
	user_nickchanged:	remote_user_nickchanged,
	chat_joined:		remote_chat_joined,
	chat_left:		remote_chat_left,
	chat_kicked:		remote_chat_kicked,
	chat_getmessage:	remote_chat_getmessage,
	chat_getaction:		remote_chat_getaction,
	chat_invited:		remote_chat_invited,
	chat_user_joined:	remote_chat_user_joined,
	chat_user_left:		remote_chat_user_left,
	chat_user_quit:		remote_chat_user_quit,
	chat_gottopic:		remote_chat_gottopic,
#ifdef RAWIRCMODES
	chat_modechanged:	remote_chat_modechanged,
#endif
	chat_user_opped:	remote_chat_user_opped,
	chat_user_deopped:	remote_chat_user_deopped,
	chat_keychanged:	remote_chat_keychanged,
	chat_opped:		remote_chat_opped,
	chat_deopped:		remote_chat_deopped,
	chat_user_kicked:	remote_chat_user_kicked,
	subcode_request:	remote_subcode_request,
	subcode_reply:		remote_subcode_reply,
	file_offer:		remote_file_offer,
	needpass:		remote_needpass,
};












static int stub_handlerpc(firetalk_t conn) {
	char	*buf = conn->rpcpeer.buffer;
	int	startlen = conn->rpcpeer.bufferpos,
		buflen = conn->rpcpeer.bufferpos;
	uint32_t funcnum;

	fprintf(stderr, "STUB read some data; looking for complete headers\r\n");
	ftrpc_prettyprint("STUB", buf, buflen);

	if (strncmp(buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(0);
	buf += sizeof("FTRPC01 ")-1;
	buflen -= sizeof("FTRPC01 ")-1;

	if (ftrpc01_get_uint32(&buf, &buflen, &funcnum, sizeof(funcnum)) != 0)
		return(0);

	if (buflen == 0)
		return(0);

	fprintf(stderr, "STUB attempting to dispatch funcnum=%u (may fail if arguments or END not yet in buffer)\r\n", funcnum);

	switch (funcnum) {
	  case FTRPC_CALLBACK_IM_GETMESSAGE: {
			const char *a, *d;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->im_getmessage(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_IM_GETACTION: {
			const char *a, *d;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->im_getaction(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_IM_BUDDYONLINE: {
			const char *a;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->im_buddyonline(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_IM_BUDDYAWAY: {
			const char *a;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->im_buddyaway(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_BUDDYADDED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->buddyadded(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_BUDDYREMOVED: {
			const char *a, *b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->buddyremoved(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_TYPING: {
			const char *a;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->typing(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_CAPABILITIES: {
			const char *a, *b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->capabilities(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_WARNINFO: {
			const char *a;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->warninfo(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_ERROR: {
			uint32_t a;
			const char *b, *d;

			if (ftrpc01_expect_uint32(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->error(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CONNECTFAILED: {
			uint32_t a;
			const char *b;

			if (ftrpc01_expect_uint32(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->connectfailed(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_CONNECTED: {
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->connected(conn, conn->handle);
			break;
		}
	  case FTRPC_CALLBACK_DISCONNECT: {
			uint32_t a;

			if (ftrpc01_expect_uint32(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->disconnect(conn, conn->handle, a);
			break;
		}
	  case FTRPC_CALLBACK_GOTINFO: {
			const char *a, *b;
			uint32_t d, e, f, g;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &e) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &f) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &g) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->gotinfo(conn, conn->handle, a, b, d, e, f, g);
			break;
		}
	  case FTRPC_CALLBACK_IDLEINFO: {
			const char *a;
			uint32_t b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->idleinfo(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_DOINIT: {
			const char *a;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			fprintf(stderr, "STUB got doinit(%s)\r\n", a);
			conn->PI->doinit(conn, conn->handle, a);
			fprintf(stderr, "STUB dispatched doinit\r\n");
			break;
		}
	  case FTRPC_CALLBACK_SETIDLE: {
			uint32_t a;

			if (ftrpc01_expect_uint32(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->setidle(conn, conn->handle, &a);
			break;
		}
	  case FTRPC_CALLBACK_EVILED: {
			uint32_t a;
			const char *b;

			if (ftrpc01_expect_uint32(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->eviled(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_NEWNICK: {
			const char *a;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->newnick(conn, conn->handle, a);
			break;
		}
	  case FTRPC_CALLBACK_PASSCHANGED: {
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->passchanged(conn, conn->handle);
			break;
		}
	  case FTRPC_CALLBACK_USER_NICKCHANGED: {
			const char *a, *b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->user_nickchanged(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_JOINED: {
			const char *a;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_joined(conn, conn->handle, a);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_LEFT: {
			const char *a;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_left(conn, conn->handle, a);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_KICKED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_kicked(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_GETMESSAGE: {
			const char *a, *b, *e;
			uint32_t d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &e) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_getmessage(conn, conn->handle, a, b, d, e);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_GETACTION: {
			const char *a, *b, *e;
			uint32_t d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &e) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_getaction(conn, conn->handle, a, b, d, e);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_INVITED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_invited(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_USER_JOINED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_user_joined(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_USER_LEFT: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_user_left(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_USER_QUIT: {
			const char *a, *b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_user_quit(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_GOTTOPIC: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_gottopic(conn, conn->handle, a, b, d);
			break;
		}
#ifdef RAWIRCMODES
	  case FTRPC_CALLBACK_CHAT_MODECHANGED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_modechanged(conn, conn->handle, a, b, d);
			break;
		}
#endif
	  case FTRPC_CALLBACK_CHAT_USER_OPPED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_user_opped(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_USER_DEOPPED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_user_deopped(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_KEYCHANGED: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_keychanged(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_OPPED: {
			const char *a, *b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_opped(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_DEOPPED: {
			const char *a, *b;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_deopped(conn, conn->handle, a, b);
			break;
		}
	  case FTRPC_CALLBACK_CHAT_USER_KICKED: {
			const char *a, *b, *d, *e;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &e) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->chat_user_kicked(conn, conn->handle, a, b, d, e);
			break;
		}
	  case FTRPC_CALLBACK_SUBCODE_REQUEST: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->subcode_request(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_SUBCODE_REPLY: {
			const char *a, *b, *d;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->subcode_reply(conn, conn->handle, a, b, d);
			break;
		}
	  case FTRPC_CALLBACK_FILE_OFFER: {
			const char *a, *b, *e, *f;
			uint32_t d, h;
			uint16_t g;

			if (ftrpc01_expect_string(&buf, &buflen, &a) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &b) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &d) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &e) != 0)
				return(0);
			if (ftrpc01_expect_string(&buf, &buflen, &f) != 0)
				return(0);
			if (ftrpc01_expect_uint16(&buf, &buflen, &g) != 0)
				return(0);
			if (ftrpc01_expect_uint32(&buf, &buflen, &h) != 0)
				return(0);
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->file_offer(conn, conn->handle, a, b, d, e, f, g, h);
			break;
		}
	  case FTRPC_CALLBACK_NEEDPASS: {
			if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
				return(0);
			conn->PI->needpass(conn, conn->handle);
			break;
		}
	  default:
		abort();
		return(0);
	}

	conn->rpcpeer.bufferpos -= (startlen - buflen);
	memmove(conn->rpcpeer.buffer, buf, conn->rpcpeer.bufferpos);

	fprintf(stderr, "STUB successfully dispatched one RPC (%i bytes left to dispatch)\r\n", conn->rpcpeer.bufferpos);

	return(1);
}

fte_t	stub_periodic(firetalk_t conn) {
	return(FE_SUCCESS);
}

fte_t	stub_preselect(firetalk_t conn, client_t c, fd_set *read, fd_set *write, fd_set *except, int *n) {
	firetalk_sock_preselect(&(conn->rpcpeer), read, write, except, n);

	return(FE_SUCCESS);
}

fte_t	stub_postselect(firetalk_t conn, client_t c, fd_set *read, fd_set *write, fd_set *except) {
	firetalk_sock_postselect(&(conn->rpcpeer), read, write, except);

//	if (conn->rpcpeer.readdata)
		while ((conn->rpcpeer.bufferpos > 0) && (stub_handlerpc(conn) > 0))
			;

	return(FE_SUCCESS);
}

fte_t	stub_comparenicks(firetalk_t conn, const char *const a, const char *const b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_COMPARENICKS, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_isprintable(firetalk_t conn, const int c) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_ISPRINTABLE, FTRPC_TYPE_UINT32, c, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_disconnect(firetalk_t conn, client_t c) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_DISCONNECT, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_connect(firetalk_t conn, client_t c, const char *server, uint16_t port, const char *const username) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CONNECT, FTRPC_TYPE_STRING, server, FTRPC_TYPE_UINT16, port, FTRPC_TYPE_STRING, username, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_sendpass(firetalk_t conn, client_t c, const char *const password) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SENDPASS, FTRPC_TYPE_STRING, password, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_save_config(firetalk_t conn, client_t c) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SAVE_CONFIG, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_get_info(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_GET_INFO, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_set_info(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SET_INFO, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_set_away(firetalk_t conn, client_t c, const char *const a, const int b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SET_AWAY, FTRPC_TYPE_STRING, a, FTRPC_TYPE_UINT32, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_set_nickname(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SET_NICKNAME, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_set_password(firetalk_t conn, client_t c, const char *const a, const char *const b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SET_PASSWORD, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_set_privacy(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_SET_PRIVACY, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_add_buddy(firetalk_t conn, client_t c, const char *const a, const char *const b, const char *const d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_ADD_BUDDY, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_STRING, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_remove_buddy(firetalk_t conn, client_t c, const char *const a, const char *const b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_REMOVE_BUDDY, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_add_deny(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_ADD_DENY, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_remove_deny(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_REMOVE_DENY, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_upload_buddies(firetalk_t conn, client_t c) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_UPLOAD_BUDDIES, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_upload_denies(firetalk_t conn, client_t c) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_UPLOAD_DENIES, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_send_message(firetalk_t conn, client_t c, const char *const a, const char *const b, const int d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_SEND_MESSAGE, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_UINT32, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_send_action(firetalk_t conn, client_t c, const char *const a, const char *const b, const int d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_SEND_ACTION, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_UINT32, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_im_evil(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_IM_EVIL, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_join(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_JOIN, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_part(firetalk_t conn, client_t c, const char *const a) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_PART, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_invite(firetalk_t conn, client_t c, const char *const a, const char *const b, const char *const d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_INVITE, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_STRING, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_set_topic(firetalk_t conn, client_t c, const char *const a, const char *const b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_SET_TOPIC, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_op(firetalk_t conn, client_t c, const char *const a, const char *const b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_OP, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_deop(firetalk_t conn, client_t c, const char *const a, const char *const b) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_DEOP, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_kick(firetalk_t conn, client_t c, const char *const a, const char *const b, const char *const d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_KICK, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_STRING, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_send_message(firetalk_t conn, client_t c, const char *const a, const char *const b, const int d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_SEND_MESSAGE, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_UINT32, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

fte_t	stub_chat_send_action(firetalk_t conn, client_t c, const char *const a, const char *const b, const int d) {
	fte_t	ret;

	if (ftrpc_send(conn, FTRPC_FUNC_CHAT_SEND_ACTION, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_UINT32, d, FTRPC_TYPE_END) != 0)
		return(FE_RPC);
	if (ftrpc_get_reply_uint32(conn, &ret) != 0)
		return(FE_RPC);
	return(ret);
}

char	*stub_subcode_encode(firetalk_t conn, client_t c, const char *const a, const char *const b) {
	const char *str;

	if (ftrpc_send(conn, FTRPC_FUNC_SUBCODE_ENCODE, FTRPC_TYPE_STRING, a, FTRPC_TYPE_STRING, b, FTRPC_TYPE_END) != 0)
		return(NULL);
	if (ftrpc_get_reply_string(conn, &str) != 0)
		return(NULL);
	return(str);
}

const char *stub_room_normalize(firetalk_t conn, const char *const a) {
	const char *str;

	if (ftrpc_send(conn, FTRPC_FUNC_ROOM_NORMALIZE, FTRPC_TYPE_STRING, a, FTRPC_TYPE_END) != 0)
		return(NULL);
	if (ftrpc_get_reply_string(conn, &str) != 0)
		return(NULL);
	return(str);
}

client_t stub_create_handle(firetalk_t conn) {
	abort();
}

void	stub_destroy_handle(firetalk_t conn, client_t c) {
	if (ftrpc_send(conn, FTRPC_FUNC_DESTROY_HANDLE, FTRPC_TYPE_END) != 0)
		return;

	firetalk_sock_close(&(conn->rpcpeer));

	return;
}

const firetalk_PD_t firetalk_PD_remote = {
	periodic:		stub_periodic,
	preselect:		stub_preselect,
	postselect:		stub_postselect,
	comparenicks:		stub_comparenicks,
	isprintable:		stub_isprintable,
	disconnect:		stub_disconnect,
	connect:		stub_connect,
	sendpass:		stub_sendpass,
	save_config:		stub_save_config,
	get_info:		stub_get_info,
	set_info:		stub_set_info,
	set_away:		stub_set_away,
	set_nickname:		stub_set_nickname,
	set_password:		stub_set_password,
	set_privacy:		stub_set_privacy,
	im_add_buddy:		stub_im_add_buddy,
	im_remove_buddy:	stub_im_remove_buddy,
	im_add_deny:		stub_im_add_deny,
	im_remove_deny:		stub_im_remove_deny,
	im_upload_buddies:	stub_im_upload_buddies,
	im_upload_denies:	stub_im_upload_denies,
	im_send_message:	stub_im_send_message,
	im_send_action:		stub_im_send_action,
	im_evil:		stub_im_evil,
	chat_join:		stub_chat_join,
	chat_part:		stub_chat_part,
	chat_invite:		stub_chat_invite,
	chat_set_topic:		stub_chat_set_topic,
	chat_op:		stub_chat_op,
	chat_deop:		stub_chat_deop,
	chat_kick:		stub_chat_kick,
	chat_send_message:	stub_chat_send_message,
	chat_send_action:	stub_chat_send_action,
	subcode_encode:		stub_subcode_encode,
	room_normalize:		stub_room_normalize,
	create_handle:		stub_create_handle,
	destroy_handle:		stub_destroy_handle,
};
