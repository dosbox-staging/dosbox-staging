.PHONY: help clean favicons

help:
	@echo "Re-generate icons in platform specific formats or install pre-rendered png icons"
	@echo ""
	@echo "Usage:"
	@echo "  make install             # install icons to \$${DESTDIR}/usr/local/share/icons/..."
	@echo "  make install prefix=/usr # install icons to \$${DESTDIR}/usr/share/icons/..."
	@echo "  make install datadir=foo # install icons to \$${DESTDIR}foo/icons/..."
	@echo ""
	@echo "Other available targets:"
	@echo "  hicolor             - generate freedesktop.org icon directory structure"
	@echo "  dosbox-staging.ico  - Windows format"
	@echo "  dosbox-staging.icns - macOS format"
	@echo "  icon_<size>.png     - render icon in specific size, e.g. icon_24.png"
	@echo "  clean               - clean intermediate files"

##
#  Create an icon using Windows .ico format.
#
#  Windows users are expected to create icons using GUI applications, and
#  there's no good Windows-native command line program for this usecase.
#
#  This target is using icotool (package icoutils) to create an .ico file.
#  Uses librsvg for rendering vectors (package available in most repos).
#
dosbox-staging.ico: \
		icon_256.png \
		icon_48.png \
		icon_32.png \
		icon_24.png \
		icon_16.png
	icotool -c $^ -o $@

##
# Favicons for a web page. Usage:
#
# <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
# <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
# <link rel="icon" type="image/png" sizes="96x96" href="/favicon-96x96.png">
#
favicons: \
		favicon-16x16.png \
		favicon-32x32.png \
		favicon-96x96.png

##
#  Create an icon using macOS .icns format.
#
#  iconutil is avaliable on macOS out of the box.
#  Uses librsvg for rendering vectors (package available via brew).
#
dosbox-staging.icns: dosbox-staging.iconset
	iconutil -c icns dosbox-staging.iconset

dosbox-staging.iconset: \
		icon_1024.png \
		icon_512.png \
		icon_256.png \
		icon_128.png \
		icon_64.png \
		icon_32.png \
		icon_16.png
	mkdir -p "$@"
	cp icon_16.png   $@/icon_16x16.png
	cp icon_32.png   $@/icon_16x16@2x.png
	cp icon_32.png   $@/icon_32x32.png
	cp icon_64.png   $@/icon_32x32@2x.png
	cp icon_128.png  $@/icon_128x128.png
	cp icon_256.png  $@/icon_128x128@2x.png
	cp icon_256.png  $@/icon_256x256.png
	cp icon_512.png  $@/icon_256x256@2x.png
	cp icon_512.png  $@/icon_512x512.png
	cp icon_1024.png $@/icon_512x512@2x.png

##
# Freedesktop icons (for Linux or other OSes) often are placed in 'hicolor'
# directory in /usr/share/icons/ or /usr/local/share/icons/.
#
# Users can override them by placing icons in ~/.local/share/icons/
#
# This is not strictly necessary (just placing scalable icon is enough,
# but some there are some corner cases where small raster icon looks better).
#
hicolor: \
		dosbox-staging.svg \
		icon_32.png \
		icon_24.png \
		icon_22.png \
		icon_16.png
	install -DT -m 644 $<          $@/scalable/apps/dosbox-staging.svg
	install -DT -m 644 icon_32.png $@/32x32/apps/dosbox-staging.png
	install -DT -m 644 icon_24.png $@/24x24/apps/dosbox-staging.png
	install -DT -m 644 icon_22.png $@/22x22/apps/dosbox-staging.png
	install -DT -m 644 icon_16.png $@/16x16/apps/dosbox-staging.png

icon_16.png: small-svg/dosbox-staging-16.svg
	rsvg-convert -h 16 $< > $@

icon_24.png: small-svg/dosbox-staging-24.svg
	rsvg-convert -h 24 $< > $@

# Some desktop environments expect 22x22 icon (e.g. KDE Plasma).
# Rendering 24px source to 22px looks good enough.
icon_22.png: small-svg/dosbox-staging-24.svg
	rsvg-convert -h 22 $< > $@

icon_32.png: small-svg/dosbox-staging-32.svg
	rsvg-convert -h 32 $< > $@

icon_48.png: dosbox-staging.svg
	rsvg-convert -h 48 $< > $@

icon_64.png: dosbox-staging.svg
	rsvg-convert -h 64 $< > $@

icon_96.png: dosbox-staging.svg
	rsvg-convert -h 96 $< > $@

icon_128.png: dosbox-staging.svg
	rsvg-convert -h 128 $< > $@

icon_256.png: dosbox-staging.svg
	rsvg-convert -h 256 $< > $@

icon_512.png: dosbox-staging.svg
	rsvg-convert -h 512 $< > $@

icon_1024.png: dosbox-staging.svg
	rsvg-convert -h 1024 $< > $@

favicon-16x16.png: icon_16.png
	cp $< $@

favicon-32x32.png: icon_32.png
	cp $< $@

favicon-96x96.png: icon_96.png
	cp $< $@

prefix = /usr/local
datadir = $(prefix)/share
install:
	install -p -m 0644 -Dt "$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps" small-png/16x16/dosbox-staging.png
	install -p -m 0644 -Dt "$(DESTDIR)$(datadir)/icons/hicolor/22x22/apps" small-png/22x22/dosbox-staging.png
	install -p -m 0644 -Dt "$(DESTDIR)$(datadir)/icons/hicolor/24x24/apps" small-png/24x24/dosbox-staging.png
	install -p -m 0644 -Dt "$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps" small-png/32x32/dosbox-staging.png
	install -p -m 0644 -Dt "$(DESTDIR)$(datadir)/icons/hicolor/scalable/apps" dosbox-staging.svg

clean:
	rm -rf dosbox-staging.iconset
	rm -rf hicolor
	rm -f icon_*.png
	rm -f favicon*
