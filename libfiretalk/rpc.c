#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "firetalk-int.h"
#include "rpc.h"

#include <ctype.h>
#include <stdio.h>

void	ftrpc_uglyprint(char *prefix, char *buf, int buflen) {
	int	i;

	fprintf(stderr, "%s (%i) [", prefix, buflen);
	for (i = 0; i < buflen; i++) {
		if ((i > 0) && (strncmp(buf+i, "FTRPC01 ", sizeof("FTRPC01 ")-1) == 0))
			fprintf(stderr, "]\r\n%s (%i) [", prefix, buflen);
		if (isprint(buf[i]))
			fprintf(stderr, "%c", buf[i]);
		else if (buf[i] < 8)
			fprintf(stderr, "\\%i", buf[i]);
		else
			fprintf(stderr, "\\x%02X", buf[i]);
	}
	fprintf(stderr, "]\r\n");
	fprintf(stderr, "%s END OF BUFFER (premature end of buffer)\r\n", prefix);
}

void	ftrpc_prettyprint(char *prefix, char *buf, int buflen) {
	fprintf(stderr, "%s START OF BUFFER (%i bytes to parse)\r\n", prefix, buflen);
	while (buflen > 0) {
		uint32_t funcnum;

		if (strncmp(buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0) {
			if (buflen > 0) {
				fprintf(stderr, "-- malformed or incomplete header; going ugly\r\n");
				ftrpc_uglyprint(prefix, buf, buflen);
			} else
				fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
			return;
		}
		buf += sizeof("FTRPC01 ")-1;
		buflen -= sizeof("FTRPC01 ")-1;

		fprintf(stderr, "%s FTRPC01 ", prefix);

		if (ftrpc01_get_uint32(&buf, &buflen, &funcnum, sizeof(funcnum)) != 0) {
			if (buflen > 0) {
				fprintf(stderr, "-- malformed or incomplete header; going ugly\r\n");
				ftrpc_uglyprint(prefix, buf, buflen);
			} else
				fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
			return;
		}

		if (funcnum != 0)
			fprintf(stderr, "%u (0x%08X) (", funcnum, funcnum);
		else
			fprintf(stderr, "REPLY (0x%08X) (", funcnum);

		while ((buflen > 0) && (*buf != FTRPC_TYPE_END))
			switch (*buf) {
			  case FTRPC_TYPE_STRING: {
					const char *str;

					if (ftrpc01_expect_string(&buf, &buflen, &str) != 0) {
						if (buflen > 0) {
							fprintf(stderr, "-- malformed or incomplete header; going ugly\r\n");
							ftrpc_uglyprint(prefix, buf, buflen);
						} else
							fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
						return;
					}
					if (str != NULL)
						fprintf(stderr, "STRING %i \"%s\\0\", ", strlen(str)+1, str);
					else
						fprintf(stderr, "STRING 0 NULL, ");
					break;
				}
			  case FTRPC_TYPE_UINT32: {
					uint32_t val;

					if (ftrpc01_expect_uint32(&buf, &buflen, &val) != 0) {
						if (buflen > 0) {
							fprintf(stderr, "-- malformed or incomplete header; going ugly\r\n");
							ftrpc_uglyprint(prefix, buf, buflen);
						} else
							fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
						return;
					}
					fprintf(stderr, "UINT32 %u (0x%08X), ", val, val);
					break;
				}
			  case FTRPC_TYPE_UINT16: {
					uint16_t val;

					if (ftrpc01_expect_uint16(&buf, &buflen, &val) != 0) {
						if (buflen > 0) {
							fprintf(stderr, "-- malformed or incomplete header; going ugly\r\n");
							ftrpc_uglyprint(prefix, buf, buflen);
						} else
							fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
						return;
					}
					fprintf(stderr, "UINT16 %u (0x%04X), ", val, val);
					break;
				}
			  default:
				if (buflen > 0) {
					fprintf(stderr, "-- malformed or incomplete header; going ugly\r\n");
					ftrpc_uglyprint(prefix, buf, buflen);
				} else
					fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
				return;
			}
		if ((buflen > 0) && (*buf == FTRPC_TYPE_END)) {
			buf++;
			buflen--;
			fprintf(stderr, "END)\r\n");
		} else {
			fprintf(stderr, "...\r\n%s END OF BUFFER (premature end of buffer)\r\n", prefix);
			return;
		}
	}
	fprintf(stderr, "%s END OF BUFFER\r\n", prefix);
}

static int ftrpc_read(firetalk_t conn) {
	fd_set	readfds, writefds, exceptfds;
	int	n = 0;

	fprintf(stderr, "GET_REPLY couldn't find a reply; blocking until more data is received\r\n");

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);

	firetalk_sock_preselect(&(conn->rpcpeer), &readfds, &writefds, &exceptfds, &n);
	select(n, &readfds, &writefds, &exceptfds, NULL);
	firetalk_sock_postselect(&(conn->rpcpeer), &readfds, &writefds, &exceptfds);

	fprintf(stderr, "GET_REPLY read some data; looking for complete headers\r\n");
	ftrpc_prettyprint("GET_REPLY", conn->rpcpeer.buffer, conn->rpcpeer.bufferpos);

	return(conn->rpcpeer.readdata);
}

static int ftrpc_write(firetalk_t conn, const void *data, const int length) {
	int	ret;

	if ((ret = firetalk_sock_send(&(conn->rpcpeer), data, length)) != FE_SUCCESS) {
//		conn->PI->disconnect(conn, conn->handle, FE_PACKET);
		return(-1);
	}
	return(0);
}

int	ftrpc_send(firetalk_t conn, uint32_t funcnum, ftrpc_type_t type, ...) {
	va_list	msg;

	if (ftrpc_write(conn, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(-1);

	{
		uint32_t tmp = htonl(funcnum);

		if (ftrpc_write(conn, &tmp, sizeof(tmp)) != 0)
			return(-1);
	}

	va_start(msg, type);
	while (type != FTRPC_TYPE_END) {
		char	tmp = (char)type;

		if (ftrpc_write(conn, &tmp, 1) != 0)
			return(-1);

		switch (type) {
		  case FTRPC_TYPE_UINT32: {
				uint32_t val = htonl(va_arg(msg, uint32_t));
//				uint32_t len = htonl(sizeof(val));

//				if (ftrpc_write(conn, &len, sizeof(len)) != 0)
//					return(-1);
				if (ftrpc_write(conn, &val, sizeof(val)) != 0)
					return(-1);
				break;
			}
		  case FTRPC_TYPE_UINT16: {
				uint16_t val = htons(va_arg(msg, int));
//				uint32_t len = htonl(sizeof(val));

//				if (ftrpc_write(conn, &len, sizeof(len)) != 0)
//					return(-1);
				if (ftrpc_write(conn, &val, sizeof(val)) != 0)
					return(-1);
				break;
			}
		  case FTRPC_TYPE_STRING: {
				unsigned char *ptr = va_arg(msg, unsigned char *);
				uint32_t len = (ptr == NULL)?0:htonl(strlen(ptr)+1);

				if (ftrpc_write(conn, &len, sizeof(len)) != 0)
					return(-1);
				if (ptr != NULL)
					if (ftrpc_write(conn, ptr, strlen(ptr)+1) != 0)
						return(-1);
				break;
			}
		  default:
			abort();
		}
		type = va_arg(msg, ftrpc_type_t);
	}
	va_end(msg);

	{
		char	tmp = (char)type;

		if (ftrpc_write(conn, &tmp, 1) != 0)
			return(-1);
	}

	return(0);
}

int	ftrpc01_get_type(char **buf, int *buflen) {
	int	type;

	if (*buflen < 1)
		return(-1);
	type = **buf;
	(*buf)++;
	(*buflen)--;
	return(type);
}

int	ftrpc01_get_uint32(char **buf, int *buflen, uint32_t *val, int len) {
	if (*buflen < len)
		return(-1);
	if (len != sizeof(*val))
		return(-1);
	*val = ntohl(*((uint32_t *)(*buf)));
	*buf += len;
	*buflen -= len;
	return(0);
}

int	ftrpc01_expect_uint32(char **buf, int *buflen, uint32_t *val) {
	if (ftrpc01_get_type(buf, buflen) != FTRPC_TYPE_UINT32)
		return(-1);
//	if (ftrpc01_get_len(buf, buflen) != sizeof(*val))
//		return(-1);
	return(ftrpc01_get_uint32(buf, buflen, val, sizeof(*val)));
}

int	ftrpc01_get_uint16(char **buf, int *buflen, uint16_t *val, int len) {
	if (*buflen < len)
		return(-1);
	if (len != sizeof(*val))
		return(-1);
	*val = ntohs(*((uint16_t *)(*buf)));
	*buf += len;
	*buflen -= len;
	return(0);
}

int	ftrpc01_expect_uint16(char **buf, int *buflen, uint16_t *val) {
	if (ftrpc01_get_type(buf, buflen) != FTRPC_TYPE_UINT16)
		return(-1);
//	if (ftrpc01_get_len(buf, buflen) != sizeof(*val))
//		return(-1);
	return(ftrpc01_get_uint16(buf, buflen, val, sizeof(*val)));
}

int	ftrpc01_get_string(char **buf, int *buflen, const char **str, int len) {
	if (*buflen < len)
		return(-1);
	*str = *buf;
	*buf += len;
	*buflen -= len;
	return(0);
}

int	ftrpc01_expect_string(char **buf, int *buflen, const char **str) {
	uint32_t len;

	if (ftrpc01_get_type(buf, buflen) != FTRPC_TYPE_STRING)
		return(-1);
	if (ftrpc01_get_uint32(buf, buflen, &len, sizeof(len)) != 0)
		return(-1);
	if (len == 0)
		*str = NULL;
	else {
		if (ftrpc01_get_string(buf, buflen, str, len) != 0)
			return(-1);
		if ((*str)[len-1] != 0)
			return(-1);
	}
	return(0);
}

static int ftrpc01_steal_peak(char *buf, int buflen, uint32_t *funcnum) {
	if (buflen < sizeof("FTRPC01 ")-1)
		return(-1);
	if (strncmp(buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(-1);
	buf += sizeof("FTRPC01 ")-1;
	buflen -= sizeof("FTRPC01 ")-1;

	if (ftrpc01_get_uint32(&buf, &buflen, funcnum, sizeof(*funcnum)) != 0)
		return(-1);

	while ((buflen > 0) && (*buf != FTRPC_TYPE_END))
		switch (*buf) {
		  case FTRPC_TYPE_UINT32: {
				uint32_t val;

				if (ftrpc01_expect_uint32(&buf, &buflen, &val) != 0)
					return(-1);
				break;
			}
		  case FTRPC_TYPE_UINT16: {
				uint16_t val;

				if (ftrpc01_expect_uint16(&buf, &buflen, &val) != 0)
					return(-1);
				break;
			}
		  case FTRPC_TYPE_STRING: {
				const char *str;

				if (ftrpc01_expect_string(&buf, &buflen, &str) != 0)
					return(-1);
				break;
			}
		  default:
			return(-1);
		}

	if (ftrpc01_get_type(&buf, &buflen) != FTRPC_TYPE_END)
		return(-1);

	return(0);
}

static int ftrpc01_steal_skip(char **buf, int *buflen) {
	uint32_t funcnum;

	if (*buflen < sizeof("FTRPC01 ")-1)
		return(-1);
	if (strncmp(*buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(-1);
	*buf += sizeof("FTRPC01 ")-1;
	*buflen -= sizeof("FTRPC01 ")-1;

	if (ftrpc01_get_uint32(buf, buflen, &funcnum, sizeof(funcnum)) != 0)
		return(-1);

	while ((*buflen > 0) && (**buf != FTRPC_TYPE_END))
		switch (**buf) {
		  case FTRPC_TYPE_UINT32: {
				uint32_t val;

				if (ftrpc01_expect_uint32(buf, buflen, &val) != 0)
					return(-1);
				break;
			}
		  case FTRPC_TYPE_UINT16: {
				uint16_t val;

				if (ftrpc01_expect_uint16(buf, buflen, &val) != 0)
					return(-1);
				break;
			}
		  case FTRPC_TYPE_STRING: {
				const char *str;

				if (ftrpc01_expect_string(buf, buflen, &str) != 0)
					return(-1);
				break;
			}
		  default:
			return(-1);
		}

	(*buf)++;
	(*buflen)--;

	return(0);
}

static int ftrpc01_steal_get_string(char **buf, int *buflen, const char **str) {
	static char *hackhackhack = NULL;
	char	*start = *buf;
	uint32_t funcnum;

fprintf(stderr, "GET_REPLY steal_get_string\r\n");

	if (*buflen < sizeof("FTRPC01 ")-1)
		return(-1);
	if (strncmp(*buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(-1);
	*buf += sizeof("FTRPC01 ")-1;
	*buflen -= sizeof("FTRPC01 ")-1;

	if (ftrpc01_get_uint32(buf, buflen, &funcnum, sizeof(funcnum)) != 0)
		return(-1);

	if (funcnum != 0)
		return(-1);

	if (ftrpc01_expect_string(buf, buflen, str) != 0)
		return(-1);

	if (**buf != FTRPC_TYPE_END)
		return(-1);

	(*buf)++;
	(*buflen)--;

	hackhackhack = realloc(hackhackhack, strlen(*str)+1);
	strcpy(hackhackhack, *str);
	*str = hackhackhack;

	memmove(start, *buf, *buflen);

fprintf(stderr, "GET_REPLY successfully stole a string (%s)\r\n", *str);

	return(0);
}

int	ftrpc_get_reply_string(firetalk_t conn, const char **str) {
	char	*buf = conn->rpcpeer.buffer;
	int	buflen = conn->rpcpeer.bufferpos, tmp, e;
	uint32_t funcnum;

fprintf(stderr, "GET_REPLY string\r\n");

	while (1) {
		while (ftrpc01_steal_peak(buf, buflen, &funcnum) != 0) {
			ftrpc_read(conn);
			buflen = conn->rpcpeer.bufferpos;
		}
		if (funcnum == 0)
			break;
fprintf(stderr, "GET_REPLY funcnum=%i; skipping\r\n", funcnum);
		if (ftrpc01_steal_skip(&buf, &buflen) != 0)
			return(-1);
	}

	tmp = buflen;
	e = ftrpc01_steal_get_string(&buf, &buflen, str);
fprintf(stderr, "GET_REPLY reducing bufferpos from %i by %i\r\n", conn->rpcpeer.bufferpos, (tmp - buflen));
	conn->rpcpeer.bufferpos -= (tmp - buflen);
	return(e);
}

static int ftrpc01_steal_get_uint32(char **buf, int *buflen, uint32_t *val) {
	char	*start = *buf;
	uint32_t funcnum;

fprintf(stderr, "GET_REPLY steal_get_uint32\r\n");

	if (*buflen < sizeof("FTRPC01 ")-1)
		return(-1);
	if (strncmp(*buf, "FTRPC01 ", sizeof("FTRPC01 ")-1) != 0)
		return(-1);
	*buf += sizeof("FTRPC01 ")-1;
	*buflen -= sizeof("FTRPC01 ")-1;

	if (ftrpc01_get_uint32(buf, buflen, &funcnum, sizeof(funcnum)) != 0)
		return(-1);

	if (funcnum != 0)
		return(-1);

	if (ftrpc01_expect_uint32(buf, buflen, val) != 0)
		return(-1);

	if (**buf != FTRPC_TYPE_END)
		return(-1);

	(*buf)++;
	(*buflen)--;

	memmove(start, *buf, *buflen);

fprintf(stderr, "GET_REPLY successfully stole a uint32 (%u)\r\n", *val);

	return(0);
}

int	ftrpc_get_reply_uint32(firetalk_t conn, uint32_t *ret) {
	char	*buf = conn->rpcpeer.buffer;
	int	buflen = conn->rpcpeer.bufferpos, tmp, e;
	uint32_t funcnum;

fprintf(stderr, "GET_REPLY uint32\r\n");

	while (1) {
		while (ftrpc01_steal_peak(buf, buflen, &funcnum) != 0) {
			ftrpc_read(conn);
			buf = conn->rpcpeer.buffer;
			buflen = conn->rpcpeer.bufferpos;
		}
		if (funcnum == 0)
			break;
fprintf(stderr, "GET_REPLY funcnum=%i; skipping\r\n", funcnum);
		if (ftrpc01_steal_skip(&buf, &buflen) != 0)
			return(-1);
	}

	tmp = buflen;
	e = ftrpc01_steal_get_uint32(&buf, &buflen, ret);
fprintf(stderr, "GET_REPLY reducing bufferpos from %i by %i\r\n", conn->rpcpeer.bufferpos, (tmp - buflen));
	conn->rpcpeer.bufferpos -= (tmp - buflen);
	return(e);
}
