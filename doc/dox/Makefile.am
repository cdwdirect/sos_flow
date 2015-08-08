# Copyright (C) 2004 Oren Ben-Kiki.
# This library is distributed under a BSD-style license.
# See the COPYING file for details.

AUTOMAKE_OPTIONS = check-news gnu subdir-objects

bin_PROGRAMS = doxample
doxample_SOURCES = doxample.c

TESTS = test.sh
TESTS_ENVIRONMENT = EXE_EXT=$(EXEEXT)

include aminclude.am

if DX_COND_man

# You'd probably want to post-process man pages and installed the patched
# versions.
dist_man1_MANS = @DX_DOCDIR@/man/man1/doxample.c.1

$(dist_man1_MANS): doxygen-doc

endif

MOSTLYCLEANFILES = $(DX_CLEANFILES)

EXTRA_DIST = cleanup bootstrap $(TESTS) $(DX_CONFIG)
