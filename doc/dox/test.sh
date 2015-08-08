# Copyright (C) 2004 Oren Ben-Kiki.
# This library is distributed under a BSD-style license.
# See the COPYING file for details.

GOT=`./doxample` || {
    echo 'FAILED to run doxample'
    exit 1
}

test "$GOT" = 'Hello, world!' || {
    echo "Expected 'Hello, world!', got '$GOT'"
    exit 1
}
