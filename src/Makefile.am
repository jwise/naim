pkgdoc_DATA = commands.txt keyboard.txt ${top_srcdir}/FAQ ${top_srcdir}/README
noinst_HEADERS = ${top_srcdir}/doc/naim.hlp

EXTRA_DIST = \
	${pkgdoc_DATA} \
	cmplhlp2.sh \
	cmplhelp.sh \
	gencmds.sh \
	gencmd2.sh \
	genkeys.sh \
	snapshot.h \
	main_stub.c

bin_PROGRAMS = naim

NAIM_CORE = \
	buddy.c \
	conio.c \
	conio_cmds.h \
	conio_keys.h \
	echof.c \
	events.c \
	fireio.c \
	hamster.c \
	help.h \
	help.c \
	helpcmd.c \
	hwprintf.c \
	linux_vsprintf.c \
	linux_vsprintf.h \
	main.c \
	modutil.c \
	naim-int.h \
	naimrc_sample.h \
	rc.c \
	rodents.c \
	script.c \
	set.c \
	win.c

if CYGWIN
NAIM_SOURCES = main_stub.c
NAIM_LDADD = libnaim_core.la

lib_LTLIBRARIES = libnaim_core.la
libnaim_core_la_SOURCES = ${NAIM_CORE}
libnaim_core_la_CFLAGS = ${AM_CFLAGS} -DDLSEARCHPATH='"${pkgmoddir}"'
libnaim_core_la_LIBADD = \
	@LIBLTDL@ \
	@CURSESLIB@ @MATHLIB@ @NETLIB@ @DLLIB@
libnaim_core_la_LDFLAGS = -export-dynamic -no-undefined \
	-Wl,--whole-archive -Wl,../firetalk/libfiretalk.a -Wl,../secs/libsecs.a -Wl,--no-whole-archive
else
NAIM_SOURCES = ${NAIM_CORE}
NAIM_LDADD = \
	../firetalk/libfiretalk.a \
	@LIBLTDL@ \
	../secs/libsecs.a \
	@CURSESLIB@ @MATHLIB@ @NETLIB@ @DLLIB@
endif

naim_SOURCES = ${NAIM_SOURCES}
naim_LDADD = ${NAIM_LDADD}
naim_LDFLAGS = -dlopen self -export-dynamic -no-undefined

INCLUDES = -I${top_srcdir}/include -I${top_srcdir}/firetalk ${INCLTDL}


${srcdir}/conio_keys.h: ${srcdir}/conio.c
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" SRCDIR=${srcdir} ${srcdir}/genkeys.sh > ${srcdir}/conio_keys.h

${srcdir}/conio_cmds.h: ${srcdir}/conio.c config.h
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/gencmds.sh > ${srcdir}/conio_cmds.h

${srcdir}/commands.txt: ${srcdir}/conio.c config.h
	AWK="${AWK}" CPP="${CPP} ${CPPFLAGS} ${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" ${srcdir}/gencmd2.sh > ${srcdir}/commands.txt

conio.o: ${srcdir}/conio.c ${srcdir}/conio_keys.h ${srcdir}/conio_cmds.h ${srcdir}/commands.txt
	@echo $(COMPILE) -c `test -f 'conio.c' || echo '$(srcdir)/'`conio.c
	@$(COMPILE) -c `test -f 'conio.c' || echo '$(srcdir)/'`conio.c
conio.lo: ${srcdir}/conio.c ${srcdir}/conio_keys.h ${srcdir}/conio_cmds.h ${srcdir}/commands.txt
	@echo $(LTCOMPILE) -c -o $@ `test -f 'conio.c' || echo '$(srcdir)/'`conio.c
	@$(LTCOMPILE) -c -o $@ `test -f 'conio.c' || echo '$(srcdir)/'`conio.c

helpcmd.o: ${srcdir}/helpcmd.c ${srcdir}/conio_cmds.h
	@echo $(COMPILE) -c `test -f 'helpcmd.c' || echo '$(srcdir)/'`helpcmd.c
	@$(COMPILE) -c `test -f 'helpcmd.c' || echo '$(srcdir)/'`helpcmd.c
helpcmd.lo: ${srcdir}/helpcmd.c ${srcdir}/conio_cmds.h
	@echo $(LTCOMPILE) -c -o $@ `test -f 'helpcmd.c' || echo '$(srcdir)/'`helpcmd.c
	@$(LTCOMPILE) -c -o $@ `test -f 'helpcmd.c' || echo '$(srcdir)/'`helpcmd.c

main.o: ${srcdir}/main.c
	@echo $(COMPILE) -DDLSEARCHPATH='"${pkgmoddir}"' -c `test -f 'main.c' || echo '$(srcdir)/'`main.c
	@$(COMPILE) -DDLSEARCHPATH='"${pkgmoddir}"' -c `test -f 'main.c' || echo '$(srcdir)/'`main.c
main.lo: ${srcdir}/main.c
	@echo $(LTCOMPILE) -DDLSEARCHPATH='"${pkgmoddir}"' -c -o $@ `test -f 'main.c' || echo '$(srcdir)/'`main.c
	@$(LTCOMPILE) -DDLSEARCHPATH='"${pkgmoddir}"' -c -o $@ `test -f 'main.c' || echo '$(srcdir)/'`main.c

.c.o: $<
	@echo $(COMPILE) -c `test -f '$<' || echo '$(srcdir)/'`$<
	@$(COMPILE) -c `test -f '$<' || echo '$(srcdir)/'`$<
.c.lo: $<
	@echo $(LTCOMPILE) -c -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
	@$(LTCOMPILE) -c -o $@ `test -f '$<' || echo '$(srcdir)/'`$<

help.h: ${top_srcdir}/doc/naim.hlp
	AWK="${AWK}" "${srcdir}/cmplhlp2.sh" < ${top_srcdir}/doc/naim.hlp > help.h

help.c: ${top_srcdir}/doc/naim.hlp
	AWK="${AWK}" "${srcdir}/cmplhelp.sh" < ${top_srcdir}/doc/naim.hlp > help.c

install: install-am install-global-symlink

install-global-symlink:
	mkdir -p $(DESTDIR)$(bindir)
	for proto in irc icq lily; do \
		if [ ! -h $(DESTDIR)$(bindir)/n$${proto} ]; then \
			rm -f $(DESTDIR)$(bindir)/n$${proto}; \
			ln -s naim $(DESTDIR)$(bindir)/n$${proto}; \
		fi; \
	done
