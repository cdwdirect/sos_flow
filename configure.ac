# Process this file with autoconf to produce a configure script.


AC_INIT(sos_flow, 0.0)
AC_CONFIG_MACRO_DIR([m4])

# Initialize LibTools...
LT_INIT([shared static])


AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/sos.c])
# AC_CONFIG_AUX_DIR([config])
AC_REQUIRE_AUX_FILE([install-sh])
AM_INIT_AUTOMAKE([foreign])

AC_CANONICAL_HOST
case $host_os in
    darwin* )
        LIBSUFFIX=.dynlib
        AC_SUBST(LIBSUFFIX)
        ;;
    * )
        LIBSUFFIX=.so
        AC_SUBST(LIBSUFFIX)
        ;;
esac


## check for SQLite
AC_ARG_WITH([sqlite],
    AS_HELP_STRING([--without-sqlite], [Ignore presence of sqlite and disable it]))

AS_IF([test "x$with_sqlite" != "xno"],
      [
PKG_CHECK_MODULES([SQLITE3], [sqlite3], [have_libsqlite3=yes], [have_libsqlite3=no])
       ],
      [have_libsqlite3=no])

AS_IF([test "x$have_libsqlite3" = "xyes"],
      [],
      [AS_IF([test "x$with_libsqlite3" = "xyes"],
             [AC_MSG_ERROR([libsqlite3 requested but not found]) ])
])
AM_CONDITIONAL([LIB_SQLITE3],  [test "$have_libsqlite3" = "yes"])



# Check for pthreads
AX_PTHREAD

if test "$PTHREAD_CC" != "$CC"
then
echo -e "\e@<:@31m!!! WARNING !!!\e@<:@37m pthreads not found for $CC"
fi


#
#  Doxygen stuff...
#
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(sos_flow, doxygen.cfg, doc)

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
