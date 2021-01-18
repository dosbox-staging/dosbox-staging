##
#  Fetch the latest dependencies from upstream
#  -------------------------------------------
GLIB_ARCHIVE = glib.tar.xz
GLIB_URL = https://download.gnome.org/sources/glib/2.67/glib-2.67.2.tar.xz
GLIB_SHA256 = b41d42d6c572e1e420ffc38a077e0157e0b53b5a94c9647a3dc3701043c3b69b

##
#  Common commands and arguments
#  -----------------------------
THREADS = $(shell nproc || echo 4)
CURL_FLAGS = --progress-bar
CURL = curl --location $(CURL_FLAGS)
WGET_FLAGS = --no-verbose --progress=bar
WGET = wget --no-clobber $(WGET_FLAGS)
EXTRACT = tar --strip 1 -Jxof
DIR := ${CURDIR}

##
#  Everything-targets
#  ------------------
.PHONY: all clean distclean
all: glib
clean: glib/clean
distclean:
	rm -rf bin glib include lib* share *.xz

##
#  Re-useable download function that tries curl, then wget, and then
#  prompts the user to manually download the files.  Note that if
#  one download fails then we assume they all will and therefore
#  give the user the full list up-front.
#
define download
	echo "Downloading $(URL) to ./$@"                                        \
	&& $(CURL) "$(URL)" -o "$@"                                              \
	|| $(WGET) "$(URL)" -O "$@"                                              \
	|| ( rm -f "$@"                                                          \
	     && echo ""                                                          \
	     && echo "DOWNLOAD FAILURE"                                          \
	     && echo "~~~~~~~~~~~~~~~~"                                          \
	     && echo "Please manually download the following, then re-run make:" \
	     && echo "  - $(GLIB_URL) to ./$(GLIB_ARCHIVE)"                      \
	     && echo ""                                                          \
	     && echo "Alternatively, you can use your own curl or wget"          \
	     && echo "arguments by passing CURL_FLAGS=\"--my-args\" and"         \
	     && echo "WGET_FLAGS=\"--my-flags\" to the make command."            \
	     && echo ""                                                          \
	     && echo "For example, disable certificate checking:"                \
	     && echo "    make CURL_FLAGS=\"-k\""                                \
	     && echo "    make WGET_FLAGS=\"--no-check-certificate\""            \
	     && echo ""                                                          \
	   )
endef

define checksum
	echo $(SUM) $(FILENAME) | sha256sum --check
endef

##
#  GLib Library
#  ------------
glib: glib-message

glib: lib/libglib-2.0.a

$(GLIB_ARCHIVE):
	$(eval URL := $(GLIB_URL))
	@$(download)
	$(eval SUM := $(GLIB_SHA256))
	$(eval FILENAME := $(GLIB_ARCHIVE))
	@$(checksum)

glib/build: $(GLIB_ARCHIVE)
	@test -f $@ \
	|| (mkdir -p glib/build \
	&& $(EXTRACT) $(GLIB_ARCHIVE) -C glib)

glib/build/build.ninja: glib/build
	cd glib \
	&& cd build \
	&& meson \
	--auto-features=disabled \
	--buildtype=release \
	--default-library=static \
	--prefix="$(DIR)" \
	--strip \
	-Dbsymbolic_functions=false \
	-Dfam=false \
	-Dglib_assert=false \
	-Dinstalled_tests=false \
	-Dinternal_pcre=true \
	-Dlibmount=disabled \
	-Dman=false \
	-Dnls=disabled \
	-Dselinux=disabled \
	-Dxattr=false \
	..

lib/libglib-2.0.a: glib/build/build.ninja
	cd glib/build \
	&& strings config.h \
	&& ninja \
	&& ninja install \
	&& cd "$(DIR)"/lib \
	&& ln -sf */* .

define GLIB_EXPORTS

Export the following to configure dosbox-staging without pkg-config
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
export GLIB_CFLAGS="-I$(CURDIR)/include/glib-2.0"
export GLIB_LIBS="$(CURDIR)/lib/libglib-2.0.a"
export GTHREAD_CFLAGS="-I$(CURDIR)/include/glib-2.0"
export GTHREAD_LIBS="$(CURDIR)/lib/libgthread-2.0.a"
endef

.PHONY: glib-message
glib-message: lib/libglib-2.0.a
	$(info $(GLIB_EXPORTS))

glib/clean:
	rm -rf glib/build
