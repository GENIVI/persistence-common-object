#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Ana.Chisca@continental-corporation.com
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

lib_LTLIBRARIES = libpers_common.la

include_HEADERS= \
                $(top_srcdir)/inc/protected/persComTypes.h \
                $(top_srcdir)/inc/protected/persComDataOrg.h \
                $(top_srcdir)/inc/protected/persComDbAccess.h \
                $(top_srcdir)/inc/protected/persComErrors.h \
                $(top_srcdir)/inc/protected/persComRct.h \
                $(top_srcdir)/inc/protected/persComIpc.h

libpers_common_la_CFLAGS = \
                -I $(top_srcdir)/inc/private -I $(top_srcdir)/inc/protected -I $(top_srcdir)/generated\
                $(DLT_CFLAGS)           \
                $(GIO_CFLAGS)           \
                $(GIO_UNIX_CFLAGS)      \
                $(GLIB_CFLAGS)          \
                $(GOBJECT_CFLAGS)       \
                $(DBUS_CFLAGS)          \
                $(ITZAM_CFLAGS)

libpers_common_la_SOURCES = \
            $(top_srcdir)/generated/PasClientNotificationGen.c\
            $(top_srcdir)/src/pers_data_organization.c\
            $(top_srcdir)/src/pers_local_shared_db_access.c\
            $(top_srcdir)/src/pers_low_level_db_access.c\
            $(top_srcdir)/src/pers_resource_config_table.c\
            $(top_srcdir)/src/pers_ipc.c\
            $(top_srcdir)/src/pers_ipc_dbus.c

libpers_common_la_LDFLAGS = -version-info $(GENERIC_LIBRARY_VERSION)

libpers_common_la_LIBADD = \
                            $(DLT_LIBS)             \
                            $(GIO_LIBS)             \
                            $(GIO_UNIX_LIBS)        \
                            $(GLIB_LIBS)            \
                            $(DBUS_LIBS)            \
                            $(GOBJECT_LIBS)         \
                            $(ITZAM_LIBS)

dbuspolicy_DATA = $(top_srcdir)/dbus_config/org.genivi.persistence.admin.conf

# Export interface description of org.genivi.persistence.admin DBus interface
dbusinterfaces_DATA = $(top_srcdir)/dbus_specifications/org.genivi.persistence.admin.xml

EXTRA_DIST = $(dbuspolicy_DATA) \
             $(dbusinterfaces_DATA) 

# pkgconfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(top_srcdir)/pkgconfig/libperscommon.pc

