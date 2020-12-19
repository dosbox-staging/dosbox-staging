.PHONY: all

all: \
	en/english.lng \
	fr/français.lng \
	pl/polski.lng

en/english.lng: utf-8/en/en-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP437 $< > $@

fr/français.lng: utf-8/fr/fr-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP858 $< > $@

pl/polski.lng: utf-8/pl/pl-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP852 $< > $@
