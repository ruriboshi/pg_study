# contrib/test_dsm/Makefile

MODULE_big = test_dsm
OBJS = test_dsm.o bgworker.o ipc.o $(WIN32RES)

EXTENSION = test_dsm
DATA = test_dsm--1.0.sql 
PGFILEDESC = "test_dsm - test extension for using the dynamic shared memory"

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/test_dsm
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
