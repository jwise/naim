#!/bin/sh

echo '
#include <naim/naim.h>
#include "naim-int.h"
#include "cmdar.h"

cmdar_t	cmdar[] = {
'

echo '#include "commands.c"' \
        | ${CPP} -DUACPP -dD - \
	| sed 's/^UA\(....\)(\(.*\)).*$/\1,\2/g' \
	| ${AWK} -F ',' '{
		if ((inalia == 1) && ($1 != "ALIA")) {
			inalia = 0;
			printf(" NULL },");
			indesc = 1;
			descs = 0;
		}
		if ((indesc == 1) && ($1 != "DESC")) {
			indesc = 0;
			if (descs == 0)
				printf("	NULL,");
			inargs = 1;
			printf("	{");
		}
		if ((inargs == 1) && ($1 != "AREQ") && ($1 != "AOPT") && ($1 != "WHER")) {
			inargs = 0;
			printf(" { -1, -1, NULL } },");
			printf("	%d,	%d,	C_%s },\n", minarg, minarg+maxarg, funcwhere);
		}

		if ($1 == "FUNC") {
			funcn = $2;
			minarg = 0;
			maxarg = 0;
			funcwhere = "ANYWHERE";
			printf("	{ \"%s\",	ua_%s,	{", $2, $2);
			inalia = 1;
		} else if ($1 == "WHER")
			funcwhere = $2;
		else if ($1 == "ALIA")
			printf(" \"%s\",", $2);
		else if ($1 == "DESC") {
			if (descs == 0)
				printf("	\"%s\",", $2);
			descs++;
		} else if ($1 == "AREQ") {
			if ($2 == "string")
				atype = "s";
			else if ($2 == "int")
				atype = "i";
			else if ($2 == "bool")
				atype = "b";
			else if ($2 == "window")
				atype = "W";
			else if ($2 == "buddy")
				atype = "B";
			else if ($2 == "account")
				atype = "A";
			else if ($2 == "cmember")
				atype = "M";
			else if ($2 == "idiot")
				atype = "I";
			else if ($2 == "chat")
				atype = "C";
			else if ($2 == "filename")
				atype = "F";
			else if ($2 == "varname")
				atype = "V";
			else if ($2 == "entity")
				atype = "E";
			else
				atype = "?";
			printf(" { 1, '"'"'%c'"'"', \"%s\" },", atype, $3);
			minarg++;
		} else if ($1 == "AOPT") {
			if ($2 == "string")
				atype = "s";
			else if ($2 == "int")
				atype = "i";
			else if ($2 == "bool")
				atype = "b";
			else if ($2 == "window")
				atype = "W";
			else if ($2 == "buddy")
				atype = "B";
			else if ($2 == "account")
				atype = "A";
			else if ($2 == "cmember")
				atype = "M";
			else if ($2 == "idiot")
				atype = "I";
			else if ($2 == "chat")
				atype = "C";
			else if ($2 == "filename")
				atype = "F";
			else if ($2 == "varname")
				atype = "V";
			else if ($2 == "entity")
				atype = "E";
			else
				atype = "?";
			printf(" { 0, '"'"'%c'"'"', \"%s\" },", atype, $3);
			maxarg++;
		}
	}'

echo '
};
const int cmdc = sizeof(cmdar)/sizeof(*cmdar);
'
