LOBJEXT = lo

nodist_pkgdoc_DATA = commands.txt 
pkgdoc_DATA = keyboard.txt

BUILT_SOURCES = \
	commands.txt

DISTCLEANFILES = \
	cmdar.c \
	cmdar.h \
	conio_keys.h \
	${BUILT_SOURCES}
	
EXTRA_DIST = \
	${pkgdoc_DATA} \
	gencmdarc.sh \
	gencmdarh.sh \
	gencmd2.sh \
	genkeys.sh \
	main_stub.c \
	snapshot.h

bin_PROGRAMS = naim

NAIM_CORE = \
	alias.c \
	atomizer.c \
	buddy.c \
	cmdar.c \
	commands.c \
	conio.c \
	echof.c \
	events.c \
	firebind.c \
	fireio.c \
	hamster.c \
	hwprintf.c \
	linux_vsprintf.c \
	logging.c \
	main.c \
	naim-int.h \
	rc.c \
	rodents.c \
	set.c \
	win.c

if CYGWIN
NAIM_SOURCES = main_stub.c
NAIM_LDADD = libnaim_core.la

lib_LTLIBRARIES = libnaim_core.la
libnaim_core_la_SOURCES = ${NAIM_CORE}
libnaim_core_la_CFLAGS = ${AM_CFLAGS} -DDLSEARCHPATH='"${pkgmoddir}"'
libnaim_core_la_CPPFLAGS = -DDLSEARCHPATH='"${pkgmoddir}"'
libnaim_core_la_LIBADD = \
	@LIBLTDL@ ../lua/libnlua.la \
	@CURSESLIB@ @NETLIB@
libnaim_core_la_LDFLAGS = -export-dynamic -no-undefined \
	-Wl,--whole-archive -Wl,../libfiretalk/libfiretalk.a -Wl,--no-whole-archive
else
NAIM_SOURCES = ${NAIM_CORE}
NAIM_LDADD = \
	../libfiretalk/libfiretalk.a \
	@LIBLTDL@ \
	../lua/libnlua.la \
	@CURSESLIB@ @NETLIB@
naim_CPPFLAGS = -DDLSEARCHPATH='"${pkgmoddir}"'
endif

naim_SOURCES = ${NAIM_SOURCES}
naim_LDADD = ${NAIM_LDADD}
naim_LDFLAGS = -dlopen self -export-dynamic -no-undefined

INCLUDES = -I${top_srcdir}/include -I${top_srcdir}/libfiretalk ${INCLTDL}

conio_keys.h: ${srcdir}/conio.c config.h
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" SRCDIR=${srcdir} ${srcdir}/genkeys.sh > conio_keys.h

cmdar.c: ${srcdir}/commands.c config.h
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/gencmdarc.sh > cmdar.c

cmdar.h: ${srcdir}/commands.c config.h
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/gencmdarh.sh > cmdar.h

commands.txt: ${srcdir}/commands.c config.h
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/gencmd2.sh > commands.txt

naim-cmdar.${OBJEXT}: cmdar.h
libnaim_la-cmdar.${LOBJEXT}: cmdar.h
naim-commands.${OBJEXT}: cmdar.h
libnaim_la-commands.${LOBJEXT}: cmdar.h
naim-conio.${OBJEXT}: conio_keys.h
libnaim_la-conio.${LOBJEXT}: conio_keys.h
naim-events.${OBJEXT}: cmdar.h
libnaim_la-events.${LOBJEXT}: cmdar.h
naim-fireio.${OBJEXT}: cmdar.h
libnaim_la-fireio.${LOBJEXT}: cmdar.h

install: install-am install-global-symlink

install-global-symlink:
	mkdir -p $(DESTDIR)$(bindir)
	for proto in irc icq lily; do \
		if [ ! -h $(DESTDIR)$(bindir)/n$${proto} ]; then \
			rm -f $(DESTDIR)$(bindir)/n$${proto}; \
			ln -s naim $(DESTDIR)$(bindir)/n$${proto}; \
		fi; \
	done
