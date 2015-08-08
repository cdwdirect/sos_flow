dnl Copyright (C) 2004 Oren Ben-Kiki.
dnl This library is distributed under a BSD-style license.
dnl See the COPYING file for details.

AC_INIT([lasy],[0.1],[oren@ben-kiki.org])

AC_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR(auxdir)

AC_PROG_CC
AM_PROG_CC_C_O

AC_CHECK_HEADER(stdio.h, [],
                [AC_MSG_WARN(missing required header file <stdio.h>)])

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(ON)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(ON)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN(doxample, doxygen.cfg)

AM_INIT_AUTOMAKE(doxample, 0.1)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
