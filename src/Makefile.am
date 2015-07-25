######################################################################################
# Makefile.am : Automake configuration for SOS_FLOW project                    [START]
#
#
# NOTE: For Cray systems: ...
#
# APP_LD_FLAGS = -dynamic
#
# ########## #

AUTOMAKE_OPTIONS            = foreign subdir-objects
ACLOCAL_FLAGS               = -I m4
SUBDIRS                     =

# ---------- #

bin_PROGRAMS                = demo_app     sosd     sos_cmd

demo_app_CFLAGS             =
sosd_CFLAGS                 =
sos_val_pub_CFLAGS          =
sos_val_ann_CFLAGS          =

demo_app_LDFLAGS            = -L.libs -lsos $(APP_LD_FLAGS)
sosd_LDFLAGS                = -L.libs -lsos $(APP_LD_FLAGS)
sos_cmd_LDFLAGS             = -L.libs -lsos $(APP_LD_FLAGS)

demo_app_SOURCES            = demo_app.c
sosd_SOURCES                = sosd.c
sos_cmd_SOURCES             = sos_cmd.c

include_HEADERS             = sos.h    sos_debug.h    sos_error.h

# ---------- #

lib_LTLIBRARIES             = libsos.la
libsos_la_SOURCES           = sos.c sos_debug.h sos_error.h

demo_app_DEPENDENCIES       = $(lib_LTLIBRARIES)
sosd_DEPENDENCIES           = $(lib_LTLIBRARIES)
sos_cmd_DEPENDENCIES        = $(lib_LTLIBRARIES)

# ########## #
#
#
#
#                                                                                [END]
######################################################################################
