AUTOMAKE_OPTIONS = foreign

if DEBUG
AM_CFLAGS =$(DEPS_CFLAGS) $(CHECK_CFLAGS) -g
else
AM_CFLAGS = $(DEPS_CFLAGS) $(CHECK_CFLAGS)
#AM_CFLAGS = -fprofile-arcs -ftest-coverage  $(DEPS_CFLAGS) $(CHECK_CFLAGS)

endif

noinst_PROGRAMS = persistence_common_object_test
 
persistence_common_object_test_SOURCES = persistence_common_object_test.c
persistence_common_object_test_LDADD = $(DLT_LIBS) $(DEPS_LIBS) $(CHECK_LIBS)\
   $(top_srcdir)/src/libpers_common.la
   
TESTS=persistence_common_object_test
