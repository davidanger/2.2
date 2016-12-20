#
# Copyright (C) 2011-2014 David Bigagli
#

include $(top_srcdir)/common.mk

AM_CPPFLAGS += -I$(top_srcdir)/lsf
AM_CPPFLAGS += -D$(HOSTTYPE) -DHOST_TYPE_STRING=\"$(HOSTTYPE)\"

HOSTTYPE=LINUX64

sbin_PROGRAMS = lim
lim_SOURCES  = \
lim.cluster.c lim.control.c lim.internal.c lim.main.c lim.policy.c \
lim.xdr.c lim.conf.c lim.info.c lim.load.c lim.misc.c  lim.rload.c  \
lim.common.h  lim.conf.h  lim.h limout.h
if SOLARIS
lim_SOURCES += lim.solaris.c
else
lim_SOURCES += lim.linux.c
endif

lim_LDADD =  ../lib/liblsf.a ../intlib/liblsfint.a 
if SOLARIS
lim_LDADD += -lsocket -lnsl
endif

etags:
	etags ../*.h *.[hc] ../intlib/*.[hc] ../lib/*.[hc] \
	../res/*.[hc] ../lstools/*.[hc]

