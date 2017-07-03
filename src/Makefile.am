#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Ana.Chisca@continental-corporation.com
#         Petrica.Manoila@continental-corporation.com
#
# Makefile template for the persistence common library
#
# Process this file with automake to produce a Makefile.in.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

ACLOCAL_AMFLAGS=-I m4

# Build objects into the subdirectory of the build directory corresponding to the subdirectory of the source files
# to avoid conflicts for pers_low_level_db_access.c files for different databases
AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libpers_common.la

include_HEADERS= \
                ../inc/protected/persComTypes.h \
                ../inc/protected/persComDataOrg.h \
                ../inc/protected/persComDbAccess.h \
                ../inc/protected/persComErrors.h \
                ../inc/protected/persComRct.h \
                ../inc/protected/persComIpc.h

libpers_common_la_CFLAGS = \
                -I ../inc/private -I ../inc/protected -I ../generated\
                $(DLT_CFLAGS)           \
                $(GIO_CFLAGS)           \
                $(GIO_UNIX_CFLAGS)      \
                $(GLIB_CFLAGS)          \
                $(GOBJECT_CFLAGS)       \
                $(DBUS_CFLAGS)

if HAVE_ITZAM
libpers_common_la_CFLAGS += \
                             $(ITZAM_CFLAGS)
endif

if HAVE_SQLITE
libpers_common_la_CFLAGS += \
                            $(SQLITE_CFLAGS)
endif

if HAVE_RAWDB
libpers_common_la_CFLAGS += \
                            $(RAWDB_CFLAGS)
endif

libpers_common_la_SOURCES = \
            ../generated/PasClientNotificationGen.c\
            ../src/pers_data_organization.c\
            ../src/pers_local_shared_db_access.c\
            ../src/pers_resource_config_table.c\
            ../src/pers_ipc.c\
            ../src/pers_ipc_dbus.c

if HAVE_ITZAM
libpers_common_la_SOURCES += \
                              ../src/itzam/pers_low_level_db_access.c
endif

if HAVE_SQLITE
libpers_common_la_SOURCES += \
                              ../src/sqlite/pers_low_level_db_access.c
endif

if HAVE_RAWDB
libpers_common_la_SOURCES += \
                              ../src/rawdb/pers_low_level_db_access.c
endif

if HAVE_KVS
libpers_common_la_SOURCES += \
                              ../src/key-value-store/pers_low_level_db_access.c \
                              ../src/key-value-store/crc32.c \
                              ../src/key-value-store/database/kissdb.c \
                              ../src/key-value-store/hashtable/qhash.c \
                              ../src/key-value-store/hashtable/qhasharr.c
endif


libpers_common_la_LDFLAGS = -version-info $(PERSCOMMON_LIB_VERSION)


libpers_common_la_LIBADD = \
                            $(DLT_LIBS)             \
                            $(GIO_LIBS)             \
                            $(GIO_UNIX_LIBS)        \
                            $(GLIB_LIBS)            \
                            $(DBUS_LIBS)            \
                            $(GOBJECT_LIBS)

if HAVE_ITZAM
libpers_common_la_LIBADD += \
                             $(ITZAM_LIBS)
endif

if HAVE_SQLITE
libpers_common_la_LIBADD += \
                             $(SQLITE_LIBS)
endif

if HAVE_RAWDB
libpers_common_la_LIBADD += \
                             $(RAWDB_LIBS)
endif

# Export interface description of org.genivi.persistence.admin DBus interface
dbusinterfaces_DATA = ../dbus_specifications/org.genivi.persistence.admin.xml

EXTRA_DIST = $(dbuspolicy_DATA) \
             $(dbusinterfaces_DATA) 

# pkgconfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ../pkgconfig/libperscommon.pc

