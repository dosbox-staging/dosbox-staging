.PHONY: all

all: \
	en/en_US.lng \
	fr/fr_FR.lng \
	pl/pl_PL.lng

# (default)
# keyb us 437
# keyb us 858
en/en_US.lng: utf-8/en/en-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP437 $< > $@

# keyb fr
# keyb fr 437
# keyb fr 858
fr/fr_FR.lng: utf-8/fr/fr-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP858 $< > $@

# keyb pl
# keyb pl 852
pl/pl_PL.lng: utf-8/pl/pl-0.77.0-alpha.lng
	iconv -f UTF-8 -t CP852 $< > $@
