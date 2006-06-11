DISTCLEANFILES = \
	default_lua.h

EXTRA_DIST = \
	cmpllua.sh \
	default.lua

noinst_LTLIBRARIES = liblua.la
liblua_la_SOURCES = \
	liblua/lapi.c liblua/lcode.c liblua/ldebug.c liblua/ldo.c liblua/ldump.c liblua/lfunc.c liblua/lgc.c liblua/llex.c liblua/lmem.c \
	liblua/lobject.c liblua/lopcodes.c liblua/lparser.c liblua/lstate.c liblua/lstring.c liblua/ltable.c liblua/ltm.c \
	liblua/lundump.c liblua/lvm.c liblua/lzio.c liblua/lauxlib.c liblua/lbaselib.c liblua/ldblib.c liblua/liolib.c \
	liblua/lmathlib.c liblua/loslib.c liblua/ltablib.c liblua/lstrlib.c liblua/loadlib.c liblua/linit.c liblua/lapi.h \
	liblua/ldebug.h liblua/lgc.h liblua/lmem.h liblua/lparser.h liblua/ltable.h liblua/luaconf.h liblua/lvm.h liblua/lauxlib.h \
	liblua/ldo.h liblua/llex.h liblua/lobject.h liblua/lstate.h liblua/ltm.h liblua/lualib.h liblua/lzio.h liblua/lcode.h \
	liblua/lfunc.h liblua/llimits.h liblua/lopcodes.h liblua/lstring.h liblua/lua.h liblua/lundump.h \
	moon.c moon_conn.c moon_strings.c moon-int.h
liblua_la_LIBADD = -lm

INCLUDES = -I${srcdir}/liblua

default_lua.h: ${srcdir}/default.lua
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/cmpllua.sh < ${srcdir}/default.lua > default_lua.h

moon.lo: ${srcdir}/moon.c default_lua.h
	@echo $(LTCOMPILE) -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -c -o $@ `test -f 'moon.c' || echo '$(srcdir)/'`moon.c
	@$(LTCOMPILE) -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -c -o $@ `test -f 'moon.c' || echo '$(srcdir)/'`moon.c

moon_conn.lo: ${srcdir}/moon_conn.c
	@echo $(LTCOMPILE) -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -c -o $@ `test -f 'moon_conn.c' || echo '$(srcdir)/'`moon_conn.c
	@$(LTCOMPILE) -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -c -o $@ `test -f 'moon_conn.c' || echo '$(srcdir)/'`moon_conn.c

moon_strings.lo: ${srcdir}/moon_strings.c
	@echo $(LTCOMPILE) -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -c -o $@ `test -f 'moon_strings.c' || echo '$(srcdir)/'`moon_strings.c
	@$(LTCOMPILE) -I${top_srcdir}/include -I${top_srcdir}/src -I${top_srcdir}/libfiretalk -c -o $@ `test -f 'moon_strings.c' || echo '$(srcdir)/'`moon_strings.c
