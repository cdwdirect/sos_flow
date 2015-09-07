######################################################################################
# Makefile.am : Automake configuration for SOS_FLOW project                    [START]
#
#
# NOTE: For Cray systems: ...
#
# APP_LD_FLAGS = -dynamic
#
# ########## #


bin_PROGRAMS                = demo_app     sosd     sos_cmd

#
# --> sosd
#    (daemon)
#
sosd_SOURCES                = sosd.c qhashtbl.c
sosd_CFLAGS                 = -g
sosd_LDFLAGS                = -L.libs -lsos -pthread $(APP_LD_FLAGS)


#
# --> sos_cmd
#    (command line tool)
#
sos_cmd_SOURCES             = sos_cmd.c
sos_cmd_CFLAGS              = -g
sos_cmd_LDFLAGS             = -L.libs -lsos -pthread $(APP_LD_FLAGS)


#
# --> demo_app
#    (example of use)
#
demo_app_SOURCES            = demo_app.c
demo_app_CFLAGS             = -g
demo_app_LDFLAGS            = -L.libs -lsos -pthread $(APP_LD_FLAGS)


# ---------- #


pkginclude_HEADERS          = sos.h    sos_debug.h

#
# --> libsos.la
#    (shared library)
#
lib_LTLIBRARIES             = libsos.la
libsos_la_SOURCES           = sos.c sos_debug.h sos_error.h qhashtbl.c
libsos_la_CPPFLAGS          = -I$(top_srcdir)/src -pthread


#
#
#
#
 demo_app_DEPENDENCIES       = $(lib_LTLIBRARIES)
 sosd_DEPENDENCIES           = $(lib_LTLIBRARIES)
 sos_cmd_DEPENDENCIES        = $(lib_LTLIBRARIES)
#


# ########## #
#
#
#
#                                                                                [END]
######################################################################################
