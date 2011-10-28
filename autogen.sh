#!/bin/bash

set -x
libtoolize -f || exit 1
aclocal-1.11 || exit 1
autoconf || exit 1
autoheader || exit 1
automake-1.11 -a || exit 1
