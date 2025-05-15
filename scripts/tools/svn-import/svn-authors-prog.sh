#!/bin/bash

readonly user=$1

format_author () {
	echo "$1 <$user@users.sourceforge.net>"
}

case $user in
	qbix79)     format_author "Peter Veenstra" ;;
	harekiet)   format_author "Sjoerd van der Berg" ;;
	finsterr)   format_author "Ulf Wohlers" ;;
	canadacow)  format_author "Dean Beeler" ;;
	yot)        format_author "Felix Jakschitsch" ;;
	c2woody)    format_author "Sebastian Strohhäcker" ;;
	h-a-l-9000) format_author "Ralf Grillenberger" ;;
	ripsaw8080) format_author "ripsaw8080" ;;
	uid83799)   format_author "uid83799" ;;
esac
