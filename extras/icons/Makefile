.PHONY: help png windows macos linux clean

PNG_DIR = png

WINDOWS_DIR = windows

MACOS_DIR = macos
MACOS_ICON_1024 = $(MACOS_DIR)/dosbox-staging-1024.png
MACOS_ICONSET_DIR = $(MACOS_DIR)/dosbox-staging.iconset

help:
	@echo "Re-generate icons in platform specific formats, or install icons on Linux."
	@echo ""
	@echo "Available targets:"
	@echo "  png     - re-generate small PNG icons from the SVG sources"
	@echo "  windows - re-generate Windows .ico file from the PNG icons"
	@echo "  macos   - re-generate macOS .icns file from the macOS source PNG"
	@echo "  all     - re-generate all (in above the listed order)"
	@echo ""
	@echo "To install the icons on Linux:"
	@echo "  make install              # install icons to \$${DESTDIR}/usr/local/share/icons/..."
	@echo "  make install prefix=/usr  # install icons to \$${DESTDIR}/usr/share/icons/..."
	@echo "  make install datadir=foo  # install icons to \$${DESTDIR}foo/icons/..."

all: png windows macos

##
#  Re-generate small PNG icons from the SVG sources.
#  These are used on Linux as fallback bitmap icons and to build the Windows
#  .ico file.
#
#  Needs rsvg-convert from the librsvg package.
#
png:
	rsvg-convert -h  16 svg/dosbox-staging-16.svg > $(PNG_DIR)/icon_16.png
	rsvg-convert -h  24 svg/dosbox-staging-24.svg > $(PNG_DIR)/icon_24.png
	rsvg-convert -h  32 svg/dosbox-staging-32.svg > $(PNG_DIR)/icon_32.png

	# Some desktop environments expect a 22x22 icon (e.g., KDE Plasma).
	# Rendering this from the SVG optimised for 24x24px looks good enough.
	rsvg-convert -h  22 svg/dosbox-staging-24.svg > $(PNG_DIR)/icon_22.png

	rsvg-convert -h   48 svg/dosbox-staging.svg   > $(PNG_DIR)/icon_48.png
	rsvg-convert -h   96 svg/dosbox-staging.svg   > $(PNG_DIR)/icon_96.png
	rsvg-convert -h  128 svg/dosbox-staging.svg   > $(PNG_DIR)/icon_128.png
	rsvg-convert -h  256 svg/dosbox-staging.svg   > $(PNG_DIR)/icon_256.png
	rsvg-convert -h  512 svg/dosbox-staging.svg   > $(PNG_DIR)/icon_512.png
	rsvg-convert -h 1024 svg/dosbox-staging.svg   > $(PNG_DIR)/icon_1024.png

##
#  Re-generate Window .ico file from the PNG icons.
#
#  Needs icotool from the icoutils packags (there's no good Windows native
#  CLI tool for this).
#
windows:
	icotool -c $(PNG_DIR)/icon_256.png \
	           $(PNG_DIR)/icon_48.png \
	           $(PNG_DIR)/icon_32.png \
	           $(PNG_DIR)/icon_24.png \
	           $(PNG_DIR)/icon_16.png \
	        -o $(WINDOWS_DIR)/dosbox-staging.ico

##
#  Re-generate macOS .icns file from the 1024x1024 macOS source PNG.
#
#  Needs sips & iconutil (available on macOS out-of-the-box).
#
macos:
	mkdir -p $(MACOS_ICONSET_DIR)

	sips -z  16  16 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_16x16.png"
	sips -z  32  32 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_16x16@2x.png"
	sips -z  32  32 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_32x32.png"
	sips -z  64  64 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_32x32@2x.png"
	sips -z 128 128 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_128x128.png"
	sips -z 256 256 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_128x128@2x.png"
	sips -z 256 256 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_256x256.png"
	sips -z 512 512 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_256x256@2x.png"
	sips -z 512 512 $(MACOS_ICON_1024) --out "$(MACOS_ICONSET_DIR)/icon_512x512.png"

	cp $(MACOS_ICON_1024) $(MACOS_ICONSET_DIR)/icon_512x512@2x.png
	cp $(MACOS_ICON_1024) $(MACOS_ICONSET_DIR)/icon_1024x1024.png

	iconutil -c icns $(MACOS_ICONSET_DIR)

##
#  Install icons on Linux.
##
#  Freedesktop icons (for Linux or other OSes) often are placed in 'hicolor'
#  directory in /usr/share/icons/ or /usr/local/share/icons/.
#
#  Users can override them by placing icons in ~/.local/share/icons/
#
#  This is not strictly necessary (just placing scalable icon is enough, but
#  some there are some corner cases where small PNG bitmap icons look better).
#
prefix = /usr/local
datadir = $(prefix)/share
install:
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/22x22/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/96x96/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/128x128/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/256x256/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/512x512/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/1024x1024/apps/"
	mkdir -m 0755 -p "$(DESTDIR)$(datadir)/icons/hicolor/scalable/apps"

	install -p -m 0644 $(PNG_DIR)/icon_16.png "$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_22.png "$(DESTDIR)$(datadir)/icons/hicolor/22x22/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_24.png "$(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_32.png "$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_48.png "$(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_96.png "$(DESTDIR)$(datadir)/icons/hicolor/96x96/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_128.png "$(DESTDIR)$(datadir)/icons/hicolor/128x128/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_256.png "$(DESTDIR)$(datadir)/icons/hicolor/256x256/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_512.png "$(DESTDIR)$(datadir)/icons/hicolor/512x512/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 $(PNG_DIR)/icon_1024.png "$(DESTDIR)$(datadir)/icons/hicolor/1024x1024/apps/org.dosbox_staging.dosbox_staging.png"
	install -p -m 0644 svg/dosbox-staging.svg "$(DESTDIR)$(datadir)/icons/hicolor/scalable/apps/org.dosbox_staging.dosbox_staging.svg"

clean:
	rm -rf $(MACOS_ICONSET_DIR)

