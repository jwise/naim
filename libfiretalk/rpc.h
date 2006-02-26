typedef enum {
	FTRPC_TYPE_END = 0,
	FTRPC_TYPE_UINT32 = 'u',
	FTRPC_TYPE_UINT16 = 's',
	FTRPC_TYPE_STRING = 'S',
} ftrpc_type_t;

int	ftrpc_send(firetalk_t conn, uint32_t funcnum, ftrpc_type_t type, ...);
int	ftrpc01_get_type(char **buf, int *buflen);
int	ftrpc01_get_uint32(char **buf, int *buflen, uint32_t *val, int len);
int	ftrpc01_expect_uint32(char **buf, int *buflen, uint32_t *val);
int	ftrpc01_get_uint16(char **buf, int *buflen, uint16_t *val, int len);
int	ftrpc01_expect_uint16(char **buf, int *buflen, uint16_t *val);
int	ftrpc01_get_string(char **buf, int *buflen, const char **str, int len);
int	ftrpc01_expect_string(char **buf, int *buflen, const char **str);
int	ftrpc_get_reply_string(firetalk_t conn, const char **str);
int	ftrpc_get_reply_uint32(firetalk_t conn, uint32_t *ret);

