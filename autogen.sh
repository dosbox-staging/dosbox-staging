#!/bin/sh
#
echo "Generating build information using aclocal, automake and autoconf"
echo "This may take a while ..."

# Touch the timestamps on all the files since CVS messes them up
directory=`dirname $0`
touch $directory/configure.in

# Regenerate configuration files

aclocal
automake --gnits --include-deps --add-missing --copy 
autoconf

cp $directory/settings.h.cvs $directory/settings.h
echo "Now you are ready to run ./configure also check settings.h for extra build settings"
