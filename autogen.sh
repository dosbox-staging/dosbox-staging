#!/bin/sh

echo "Generating build information using aclocal, autoheader, automake and autoconf"
echo "This may take a while ..."

# Regenerate configuration files.

aclocal
autoheader
automake --gnits --include-deps --add-missing --copy 
autoconf

echo "Now you are ready to run ./configure, afterwards check config.h for extra build settings"
