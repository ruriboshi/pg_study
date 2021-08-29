# test_extension/Makefile

MODULE_big = test_extension
OBJS = \
	$(WIN32RES) \
	test_extension.o

EXTENSION = test_extension
DATA = test_extension--1.0.sql
PGFILEDESC = "test_extension - small example code for development of an extension"

REGRESS = test_extension

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/test_extension
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
