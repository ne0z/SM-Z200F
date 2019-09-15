#!/bin/sh
libtoolize --force --copy --automake
aclocal -I M4
autoheader
automake --foreign --copy --add-missing
autoconf
