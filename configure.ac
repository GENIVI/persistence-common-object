#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Ana.Chisca@continental-corporation.com
#         Petrica.Manoila@continental-corporation.com
#
# Configure template for the persistence-common library
#
# Process this file with autoconf to produce a configure script.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

dnl **************************************************************************
dnl *** First, define all of the version numbers up front                  ***
dnl *** In particular, this allows the version macro to be used in AC_INIT ***
dnl **************************************************************************
m4_define([PERSCOMMON_PACKAGE_VERSION_S],[1.0.3])
m4_define([PERSCOMMON_LIB_VERSION_N],[1001001])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_INIT([persistence-common],[PERSCOMMON_PACKAGE_VERSION_S])
AC_COPYRIGHT([Copyright (c) 2012 Continental Automotive GmbH])
AC_PREREQ([2.50])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE()
AC_USE_SYSTEM_EXTENSIONS()
AC_CONFIG_MACRO_DIR([m4])
AC_SUBST([ACLOCAL_AMFLAGS], ["$ACLOCAL_FLAGS -I m4"])

dnl *********************************
dnl *** Shared library versioning ***
dnl *********************************
PERSCOMMON_LIB_VERSION_CURRENT=$((((PERSCOMMON_LIB_VERSION_N() / 1000) / 1000) % 1000 ))
PERSCOMMON_LIB_VERSION_REVISION=$(( (PERSCOMMON_LIB_VERSION_N() / 1000) % 1000 ))
PERSCOMMON_LIB_VERSION_AGE=$((  PERSCOMMON_LIB_VERSION_N() % 1000 ))
PERSCOMMON_LIB_VERSION=$PERSCOMMON_LIB_VERSION_CURRENT:$PERSCOMMON_LIB_VERSION_REVISION:$PERSCOMMON_LIB_VERSION_AGE
AC_SUBST(PERSCOMMON_LIB_VERSION)

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
AM_PROG_CC_C_O()
AC_PROG_CXX()
AC_PROG_INSTALL()

dnl ***************************
dnl *** Initialize lib tool ***
dnl ***************************
AC_DISABLE_STATIC()
AC_PROG_LIBTOOL()

dnl ***************************************
dnl *** Check for standard header files ***
dnl ***************************************
AC_CHECK_HEADERS([string.h])

dnl ***********************************
dnl *** Check for library functions ***
dnl ***********************************
AC_FUNC_STRNLEN
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memcpy])
AC_CHECK_FUNCS([memset])

dnl ********************************************************************
dnl *** Check for typedefs, structures, and compiler characteristics ***
dnl ********************************************************************
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
PKG_CHECK_MODULES([GIO], [gio-2.0 >= 2.30.0]) 		
PKG_CHECK_MODULES([GIO_UNIX], [gio-unix-2.0 >= 2.30.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.30.0])
PKG_CHECK_MODULES([DLT], [automotive-dlt >= 2.2.0])
PKG_CHECK_MODULES([ZLIB], [zlib >= 1.2.5])		
PKG_CHECK_MODULES([ARCHIVELIB], [libarchive >= 3.0.4])	

dnl *************************************
dnl ***      Database support         ***
dnl *************************************

AC_ARG_WITH([database],
    AS_HELP_STRING([--with-database=auto|itzam|sqlite|rawdb|key-value-store],[Choose database backend.]),[],[with_database=auto])

dnl ********** ITZAM **********
AS_IF([test "x$with_database" = "xauto" -o "x$with_database" = "xitzam"],
    [PKG_CHECK_MODULES(ITZAM, [ libitzam >= 6.0.4 ], HAVE_ITZAM=1, HAVE_ITZAM=0)],
    HAVE_ITZAM=0)
AS_IF([test "x$HAVE_ITZAM" = "x1"], with_database=itzam)
AS_IF([test "x$HAVE_ITZAM" = "x1"], ITZAM_LIBS="-litzam")
AS_IF([test "x$with_database" = "xitzam" -a "x$HAVE_ITZAM" = "x0"],
    [AC_MSG_ERROR([*** itzam not found])])

AM_CONDITIONAL([HAVE_ITZAM], [test "x$HAVE_ITZAM" = x1])

dnl ********** SQLITE **********
AS_IF([test "x$with_database" = "xauto" -o "x$with_database" = "xsqlite"],
    [PKG_CHECK_MODULES(SQLITE, [sqlite3 >= 3.7.5], HAVE_SQLITE=1, HAVE_SQLITE=0)],
    HAVE_SQLITE=0)
AS_IF([test "x$HAVE_SQLITE" = "x1"], with_database=sqlite)
AS_IF([test "x$with_database" = "xsqlite" -a "x$HAVE_SQLITE" = "x0"],
    [AC_MSG_ERROR([*** sqlite not found])])

AM_CONDITIONAL([HAVE_SQLITE], [test "x$HAVE_SQLITE" = x1])

dnl ********** RAWDB **********
AS_IF([test "x$with_database" = "xauto" -o "x$with_database" = "xrawdb"],
    HAVE_RAWDB=1,
    HAVE_RAWDB=0)
AS_IF([test "x$HAVE_RAWDB" = "x1"], with_database=rawdb)
AS_IF([test "x$with_database" = "xrawdb" -a "x$HAVE_RAWDB" = "x0"],
    [AC_MSG_ERROR([*** rawdb not found])])

AM_CONDITIONAL([HAVE_RAWDB], [test "x$HAVE_RAWDB" = x1])

dnl ********** KEY-VALUE-STORE **********
AS_IF([test "x$with_database" = "xauto" -o "x$with_database" = "xkey-value-store"],
    HAVE_KVS=1,
    HAVE_KVS=0)
AS_IF([test "x$HAVE_KVS" = "x1"], with_database=key-value-store)
AS_IF([test "x$with_database" = "xkey-value-store" -a "x$HAVE_KVS" = "x0"],
    [AC_MSG_ERROR([*** key-value-store not found])])

AM_CONDITIONAL([HAVE_KVS], [test "x$HAVE_KVS" = x1])

AS_IF([test "x$HAVE_ITZAM" != x1 -a "x$HAVE_SQLITE" != x1 -a "x$HAVE_RAWDB" != x1 -a "x$HAVE_KVS" != x1],
    AC_MSG_ERROR([*** missing database backend]))



dnl ***************************************
dnl *** Check if test should be enabled ***
dnl ***************************************
AC_ARG_ENABLE([tests],
              [AS_HELP_STRING([--enable-tests],[Enable tests])],
              [enable_tests=$enableval],[enable_tests="no"])

AM_CONDITIONAL([WANT_TESTS], [test x"$enable_tests" = "xyes"])
# look if check framework exists
AC_ARG_WITH([localcheck],
              [AS_HELP_STRING([--with-localcheck],[Path to local check])],
              [localcheck=$withval],[localcheck=""])

if test "x$enable_tests" != "xno" ; then
   if test -z "$localcheck" ; then
      PKG_CHECK_MODULES(CHECK, check)
   else
      CHECK_CFLAGS="-I$localcheck/include"
      CHECK_LIBS="-L$localcheck/lib -lcheck"
   fi
   AC_SUBST(CHECK_CFLAGS)
   AC_SUBST(CHECK_LIBS)

   AC_DEFINE_UNQUOTED([TESTS_ENABLED], [1], [tests enabled])
else
   AC_DEFINE_UNQUOTED([TESTS_ENABLED], [0], [tests enabled])
fi
AC_MSG_NOTICE([Tests enabled: $enable_tests])
AC_MSG_NOTICE([Local check enabled: $localcheck])


dnl **********************************************
dnl *** compile with debug information enabled ***
dnl **********************************************
AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
[case "${enableval}" in
             yes) debug=true ;;
             no)  debug=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")


dnl *************************************
dnl *** enable to use fsync instead of fdatasync for write through
dnl *************************************
AC_ARG_ENABLE([fsync],
            [AS_HELP_STRING([--enable-fsync],[Enable fsync instead of fdatasync])],
            [use_fsync=$enableval],
            [use_fsync="no"])

AM_CONDITIONAL([USE_FSYNC], [test x"$use_fsync" = "no"])

if test "$use_fsync" != "yes" -a "$use_fsync" != "no"; then   
   AC_MSG_ERROR([Invalid fsync check check: $use_fsync. Only "yes" or "no" is valid])
else   
   AC_MSG_NOTICE([Use fsync: $use_fsync])

   if test "$use_fsync" = "yes"; then
      AC_DEFINE_UNQUOTED([USE_FSYNC], [1], [fsync is anabled])   
   fi
fi



dnl **********************************************
dnl *** location of pidfile, default is /var/run *
dnl **********************************************
AC_ARG_WITH([pidfilelocation],
              [AS_HELP_STRING([--with-pidfilelocation=path of pidfile],[Path to the pifile])],
              [with_pidfilelocation=$withval],[with_pidfilelocation="/var/run"])

AC_SUBST([pidfilelocationpath], [$with_pidfilelocation])
AC_MSG_NOTICE([PID File Path: $pidfilelocationpath])
AC_DEFINE_UNQUOTED(PIDFILEDIR, "$pidfilelocationpath", "location for pidfile")



######################################################################
### max numberr of database slots, default is 100.000
######################################################################
AC_ARG_WITH([cachemaxslots],
              [AS_HELP_STRING([--with-cachemaxslots=numberOfMaxSlots],[NUmber of max db slots])],
              [with_cachemaxslots=$withval],[with_cachemaxslots=100000])

AC_SUBST([cachemaxslots], [$with_cachemaxslots])
AC_MSG_NOTICE([Cache Max slots is: $cachemaxslots])
AC_DEFINE_UNQUOTED(PERS_CACHE_MAX_SLOTS, $cachemaxslots, "max db slots for cache")



dnl *************************************
dnl *** Define extra paths            ***
dnl *************************************
AC_ARG_WITH([dbuspolicydir],
        AS_HELP_STRING([--with-dbuspolicydirdir=DIR], [Directory for D-Bus system policy files]),
        [],
        [with_dbuspolicydir=$(pkg-config --silence-errors --variable=sysconfdir dbus-1)/dbus-1/system.d])
AC_SUBST([dbuspolicydir], [$with_dbuspolicydir])

# Derive path for storing 'dbus' interface files (e. g. /usr/share/dbus-1/interfaces)
AC_ARG_WITH([dbusinterfacesdir],
        AS_HELP_STRING([--with-dbusinterfacesdir=DIR], [Directory for D-Bus interface files]),
        [],
        [with_dbusinterfacesdir=$(pkg-config --silence-errors --variable=interfaces_dir dbus-1)])
AC_SUBST([dbusinterfacesdir], [$with_dbusinterfacesdir])

dnl *******************************
dnl *** Define configure output ***
dnl *******************************
AC_CONFIG_FILES(Makefile src/Makefile test/Makefile generated/Makefile pkgconfig/libperscommon.pc)

AC_OUTPUT


