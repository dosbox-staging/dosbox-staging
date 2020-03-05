.PHONY: help clean

help:
	@echo "Re-generate icons in platform specific formats."
	@echo
	@echo "Available targets:"
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

icon_16.png: small-svg/dosbox-staging-16.svg
	rsvg-convert -h 16 $< > $@

icon_24.png: small-svg/dosbox-staging-24.svg
	rsvg-convert -h 24 $< > $@

icon_32.png: small-svg/dosbox-staging-32.svg
	rsvg-convert -h 32 $< > $@

icon_48.png: dosbox-staging.svg
	rsvg-convert -h 48 $< > $@

icon_64.png: dosbox-staging.svg
	rsvg-convert -h 64 $< > $@

icon_128.png: dosbox-staging.svg
	rsvg-convert -h 128 $< > $@

icon_256.png: dosbox-staging.svg
	rsvg-convert -h 256 $< > $@

icon_512.png: dosbox-staging.svg
	rsvg-convert -h 512 $< > $@

icon_1024.png: dosbox-staging.svg
	rsvg-convert -h 1024 $< > $@

clean:
	rm -rf dosbox-staging.iconset
	rm -f icon_*.png
