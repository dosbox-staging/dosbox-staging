.PHONY: all

all: \
	en/en_US.CP437.lng \
	fr/fr_FR.CP437.lng \
	fr/fr_FR.CP858.lng \
	pl/pl_PL.CP852.lng

# (default)
# keyb us 437
en/en_US.CP437.lng: utf-8/en/en-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP437 $< > $@

# keyb fr 437
fr/fr_FR.CP437.lng: utf-8/fr/fr-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP437 $< > $@

# keyb fr
fr/fr_FR.CP858.lng: utf-8/fr/fr-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP858 $< > $@

# keyb pl
pl/pl_PL.CP852.lng: utf-8/pl/pl-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP852 $< > $@
