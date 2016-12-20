#
# Copyright (C) 2011-2014 David Bigagli
#

include $(top_srcdir)/common.mk

AM_CPPFLAGS += -I..

bin_PROGRAMS = lsacct lseligible lshosts lsid lsinfo lsloadadj \
 		lsload lsplace lsrcp lsrun lsaddhost lsrmhost lsmon

lsacct_SOURCES = lsacct.c  
lsacct_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lsacct_LDADD += -lnsl -lsocket
endif

lseligible_SOURCES = lseligible.c  
lseligible_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lseligible_LDADD += -lnsl -lsocket
endif

lshosts_SOURCES = lshosts.c  
lshosts_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lshosts_LDADD += -lnsl -lsocket
endif

lsid_SOURCES = lsid.c  
lsid_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lsid_LDADD += -lnsl -lsocket
endif

lsinfo_SOURCES = lsinfo.c  
lsinfo_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lsinfo_LDADD += -lnsl -lsocket
endif

lsloadadj_SOURCES = lsloadadj.c  
lsloadadj_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lsloadadj_LDADD += -lnsl -lsocket
endif

lsload_SOURCES = lsload.c load.c
lsload_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lsload_LDADD += -lnsl -lsocket
endif

lsmon_SOURCES = lsmon.c  
lsmon_LDADD = load.o ../lib/liblsf.a ../intlib/liblsfint.a 
if SOLARIS
lsmon_LDADD += -lnsl -lsocket -lcurses
else
lsmon_LDADD += -lncurses
endif

lsplace_SOURCES = lsplace.c  
lsplace_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a
if SOLARIS
lsplace_LDADD += -lnsl -lsocket
endif

lsrcp_SOURCES = lsrcp.c
lsrcp_LDADD = ../lib/liblsf.a ../intlib/liblsfint.a 
if !CYGWIN
lsrcp_LDADD += -lnsl
endif
if SOLARIS
lsrcp_LDADD += -lsocket
endif

lsrun_SOURCES = lsrun.c
lsrun_LDADD = ../lib/liblsf.a 
if SOLARIS
lsrun_LDADD += -lnsl -lsocket
endif

lsaddhost_SOURCES = lsaddhost.c
lsaddhost_LDADD = ../lib/liblsf.a 
if SOLARIS
lsaddhost_LDADD += -lnsl -lsocket
endif

lsrmhost_SOURCES = lsrmhost.c
lsrmhost_LDADD = ../lib/liblsf.a 
if SOLARIS
lsrmhost_LDADD += -lnsl -lsocket
endif

etags :
	etags *.[hc] ../*.h ../lib/*.[hc] 

