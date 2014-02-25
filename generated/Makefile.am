#######################################################################################################################
#
# Copyright (C) 2012 Continental Automotive Systems, Inc.
#
# Author: Ana.Chisca@continental-corporation.com
#
# Makefile template for the Persistence Administration Service generated sources
#
# Process this file with automake to produce a Makefile.in.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#######################################################################################################################

# List generated sources here to assert that they are available at build start
BUILT_SOURCES = PasClientNotificationGen.c  \
                PasClientNotificationGen.h

# Delete all generated files on 'make clean'
CLEANFILES    = PasClientNotificationGen.c  \
                PasClientNotificationGen.h
                

# Targets to create generated sources during the build

PasClientNotificationGen.c PasClientNotificationGen.h: $(top_srcdir)/dbus_specifications/org.genivi.persistence.admin.xml
	gdbus-codegen --interface org.genivi.persistence. --c-namespace OipPersistence --generate-c-code PasClientNotificationGen $(top_srcdir)/dbus_specifications/org.genivi.persistence.admin.xml
