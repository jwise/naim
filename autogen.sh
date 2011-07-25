#!/bin/bash

set -x
libtoolize -f || exit 1
aclocal-1.9 || exit 1
automake-1.9 || exit 1
autoconf || exit 1
