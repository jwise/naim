LOBJEXT = lo

DISTCLEANFILES = \
	default_lua.h

EXTRA_DIST = \
	cmpllua.sh \
	default.lua \
	examples/eliza.lua \
	examples/i-r-elite-script.lua \
	examples/lseen.lua \
	examples/printtree.lua \
	examples/stats.lua \
	examples/xkcd.lua \
	examples/localpd.lua

noinst_LTLIBRARIES = liblua.la libnlua.la
liblua_la_SOURCES = \
	lua511/lapi.c lua511/lcode.c lua511/ldebug.c lua511/ldo.c lua511/ldump.c lua511/lfunc.c lua511/lgc.c lua511/llex.c lua511/lmem.c \
	lua511/lobject.c lua511/lopcodes.c lua511/lparser.c lua511/lstate.c lua511/lstring.c lua511/ltable.c lua511/ltm.c \
	lua511/lundump.c lua511/lvm.c lua511/lzio.c lua511/lauxlib.c lua511/lbaselib.c lua511/ldblib.c lua511/liolib.c \
	lua511/lmathlib.c lua511/loslib.c lua511/ltablib.c lua511/lstrlib.c lua511/loadlib.c lua511/linit.c lua511/lapi.h \
	lua511/ldebug.h lua511/lgc.h lua511/lmem.h lua511/lparser.h lua511/ltable.h lua511/luaconf.h lua511/lvm.h lua511/lauxlib.h \
	lua511/ldo.h lua511/llex.h lua511/lobject.h lua511/lstate.h lua511/ltm.h lua511/lualib.h lua511/lzio.h lua511/lcode.h \
	lua511/lfunc.h lua511/llimits.h lua511/lopcodes.h lua511/lstring.h lua511/lua.h lua511/lundump.h
liblua_la_LIBADD = -lm

libnlua_la_SOURCES = \
	commands.c \
	conn.c \
	garbage.c \
	hooks.c \
	moon.c \
	moon-int.h \
	pd.c \
	buffer.c \
	socket.c \
	script.c \
	util.c \
	variables.c
libnlua_la_LIBADD = liblua.la
libnlua_la_CPPFLAGS = -I${srcdir}/lua511 -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -I${top_builddir}/src ${INCLTDL}

noinst_PROGRAMS = luac
luac_SOURCES = \
	lua511/luac.c \
	lua511/print.c
luac_LDADD = liblua.la

default_lua.h: ${srcdir}/default.lua
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/cmpllua.sh < ${srcdir}/default.lua > default_lua.h

libnlua_la-moon.${LOBJEXT}: default_lua.h
libnlua_la-commands.${LOBJEXT}: ../src/cmdar.h

../src/cmdar.h: ${top_srcdir}/src/commands.c
	(cd ../src && make cmdar.h)
