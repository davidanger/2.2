#
# Copyright (C) openlava foundation
#

include $(top_srcdir)/common.mk

AM_CPPFLAGS += -I../lsf

sbin_PROGRAMS = echkpnt erestart

echkpnt_SOURCES = echkpnt.c echkpnt.env.c echkpnt.lib.c echkpnt.lib.h echkpnt.env.h
echkpnt_LDADD = ../lsf/lib/liblsf.a

erestart_SOURCES = erestart.c echkpnt.env.c echkpnt.lib.c echkpnt.lib.h echkpnt.env.h
erestart_LDADD = ../lsf/lib/liblsf.a
if SOLARIS
echkpnt_LDADD += -lsocket -lnsl
erestart_LDADD += -lsocket -lnsl
endif

