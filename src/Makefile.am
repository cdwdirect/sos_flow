######################################################################################
#  Makefile.am : Automake configuration for SOS_FLOW project                   [START]
#
#
#  Target: CRAY 
#
#  APP_LD_FLAGS = -dynamic
#
#
#
#
#  Target: ACISS
#
#  SQLITE3_CFLAGS = -I/usr/local/packages/sqlite/3.8/include
#  SQLITE3_LD_FLAGS = -L/usr/local/packages/sqlite/3.8/lib

# ########## #


bin_PROGRAMS                = demo_app     sosd     sos_cmd

#
# --> sosd
#    (daemon)
#
sosd_SOURCES                = sosd.c qhashtbl.c sosd_db_sqlite.c
sosd_CFLAGS                 = -g $(SQLITE3_CFLAGS)
sosd_LDFLAGS                = -L.libs -lsos -pthread $(APP_LD_FLAGS) $(SQLITE3_LDFLAGS)


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


pkginclude_HEADERS          = sos.h    sos_debug.h    sos_error.h   sosd.h   sosd_db_sqlite.h

#
# --> libsos.la
#    (shared library)
#
lib_LTLIBRARIES             = libsos.la
libsos_la_SOURCES           = sos.c qhashtbl.c
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
