#!/bin/sh

echo '
#define C_STATUS	0x00
#define C_INCHAT	0x01
#define C_INUSER	0x02
#define C_NOTSTATUS	(C_INCHAT|C_INUSER)
#define C_ANYWHERE	0xFF

#define UAARGS	(conn_t *conn, int argc, const char **args)
#define UAFUNC2(x)	void x UAARGS

#ifndef UACPP
# define UAFUNC(x)	void ua_ ## x UAARGS
# define UAALIA(x)
# define UAWHER(x)
# define UADESC(x)
# define UAAREQ(x,y)
# define UAAOPT(x,y)
#endif

'
echo '#include "commands.c"' \
        | ${CPP} -DUACPP -dD - \
	| grep '^UAFUNC(.*).*$' \
	| sed 's/^\(UAFUNC(.*)\).*$/\1;/g'

echo '

#define NAIM_COMMANDS \'

echo '#include "commands.c"' \
        | ${CPP} -DUACPP -dD - \
	| grep '^UAFUNC(.*).*$' \
	| sed 's/^UAFUNC\((.*)\).*$/UAFUNC3\1 \\/g'


echo '
extern cmdar_t cmdar[];
extern const int cmdc;
'
