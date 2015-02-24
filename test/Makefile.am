AUTOMAKE_OPTIONS = foreign

if DEBUG
AM_CFLAGS =$(DEPS_CFLAGS) $(CHECK_CFLAGS) -g
else
AM_CFLAGS = $(DEPS_CFLAGS) $(CHECK_CFLAGS)

endif

noinst_PROGRAMS = test_pco_key_value_store
 
test_pco_key_value_store_SOURCES = test_pco_key_value_store.c
test_pco_key_value_store_LDADD = $(DLT_LIBS) $(DEPS_LIBS) $(CHECK_LIBS)\
   $(top_srcdir)/src/libpers_common.la
   
TESTS=test_pco_key_value_store
