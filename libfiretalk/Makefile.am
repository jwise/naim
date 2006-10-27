EXTRA_DIST = firetalk.m4

noinst_LIBRARIES = libfiretalk.a
libfiretalk_a_SOURCES = \
	firetalk.c \
	firetalk-int.h \
	irc.c \
	lily.c \
	pdutil.c \
	toc2.c \
	toc2_uuids.h

check_PROGRAMS = tester toctest
tester_SOURCES = \
	tester.c
tester_LDADD = \
	libfiretalk.a
toctest_SOURCES = \
	toctest.c
toctest_LDADD = \
	libfiretalk.a

#TESTS = ${check_PROGRAMS}
TESTS = tester

include_HEADERS = firetalk.h
