#!/bin/sh

echo "Generating build information using aclocal, autoheader, automake and autoconf"
echo "This may take a while ..."

# Regenerate configuration files.

aclocal
autoheader
automake --gnits --include-deps --add-missing --copy 
autoconf

#Copy settings.h.cvs to settings.h and that's it,

directory=`dirname $0`
cp $directory/settings.h.cvs $directory/settings.h
echo "Now you are ready to run ./configure also check settings.h for extra build settings"
