AUTOMAKE_OPTIONS = foreign

if DEBUG
AM_CFLAGS =$(DEPS_CFLAGS) $(CHECK_CFLAGS) -g
else
AM_CFLAGS = $(DEPS_CFLAGS) $(CHECK_CFLAGS)

endif

persdatadir = $(datadir)/persistence/test-data
persdata_DATA = data/attachToExistingCache.tar.gz data/rct_compare.tar.gz

# Add config file to distribution 
EXTRA_DIST = $(persdata_DATA) 

noinst_PROGRAMS = test_pco_key_value_store persistence_common_object_test
#persistence_sqlite_experimental
 
test_pco_key_value_store_SOURCES = test_pco_key_value_store.c
test_pco_key_value_store_CFLAGS = $(AM_CFLAGS) $(ARCHIVELIB_CFLAGS) $(ZLIB_CFLAGS) -DPERS_TEST_DATA_DIR='"$(persdatadir)"'
test_pco_key_value_store_LDADD = $(DLT_LIBS) $(SQLITE_LIBS) $(DEPS_LIBS) $(CHECK_LIBS) $(ARCHIVELIB_LIBS) $(ZLIB_LIBS) \
   $(top_srcdir)/src/libpers_common.la


persistence_common_object_test_SOURCES = persistence_common_object_test.c
persistence_common_object_test_LDADD = $(DLT_LIBS) $(SQLITE_LIBS) $(DEPS_LIBS) $(CHECK_LIBS)\
   $(top_srcdir)/src/libpers_common.la

#persistence_sqlite_experimental_SOURCES  = persistence_sqlite_experimental.c
#persistence_sqlite_experimental_LDADD = $(DLT_LIBS) $(SQLITE_LIBS) $(DEPS_LIBS) 

TESTS=test_pco_key_value_store persistence_common_object_test
