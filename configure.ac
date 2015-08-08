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
