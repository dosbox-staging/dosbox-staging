##
#  Fetch the latest dependencies from upstream
#  -------------------------------------------
FLUIDSYNTH_ARCHIVE = fluidsynth-master.tar.gz
FLUIDSYNTH_URL = https://github.com/FluidSynth/fluidsynth/archive/master.tar.gz

##
#  Common commands and arguments
#  -----------------------------
THREADS = $(shell nproc || echo 4)
CURL_FLAGS = --progress-bar
CURL = curl --location $(CURL_FLAGS)
WGET_FLAGS = --no-verbose --progress=bar
WGET = wget --no-clobber $(WGET_FLAGS)
EXTRACT = tar --strip 1 -zxof
DIR := ${CURDIR}

##
#  Everything-targets
#  ------------------
.PHONY: all clean distclean
all: fluidsynth
clean: fluidsynth/clean
distclean:
	rm -rf bin fluidsynth include lib* share *.gz

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
	     && echo "  - $(FLUIDSYNTH_URL) to ./$(FLUIDSYNTH_ARCHIVE)"          \
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

##
#  FluidSynth Library
#  -------------------
fluidsynth: fluidsynth-message

fluidsynth: lib/libfluidsynth.a

$(FLUIDSYNTH_ARCHIVE):
	$(eval URL := $(FLUIDSYNTH_URL))
	@$(download)

fluidsynth/build: $(FLUIDSYNTH_ARCHIVE)
	@test -f $@ \
	|| (mkdir -p fluidsynth/build \
	&& $(EXTRACT) $(FLUIDSYNTH_ARCHIVE) -C fluidsynth) \
	&& sed -i -E 's|^(Libs.+)|\1 $(GLIB_LIBS)|;s|^(Cflags.+)|\1 $(GLIB_CFLAGS)|' \
		fluidsynth/fluidsynth.pc.in

fluidsynth/build/Makefile: fluidsynth/build
	cd fluidsynth \
	&& cd build \
	&& cmake \
	-DBUILD_SHARED_LIBS=0 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$(DIR)" \
	-DOpenMP_gomp_LIBRARY="" \
	-DLIB_SUFFIX="" \
	-Denable-alsa=0 \
	-Denable-aufile=0 \
	-Denable-coreaudio=0 \
	-Denable-coremidi=0 \
	-Denable-dbus=0 \
	-Denable-dsound=0 \
	-Denable-floats=1 \
	-Denable-ipv6=0 \
	-Denable-jack=0 \
	-Denable-ladspa=0 \
	-Denable-lash=0 \
	-Denable-libinstpatch=0 \
	-Denable-libsndfile=0 \
	-Denable-midishare=0 \
	-Denable-network=0 \
	-Denable-oss=0 \
	-Denable-portaudio=0 \
	-Denable-pulseaudio=0 \
	-Denable-readline=0 \
	-Denable-sdl2=0 \
	-Denable-systemd=0 \
	-Denable-wasapi=0 \
	-Denable-waveout=0 \
	-Denable-winmidi=0 \
	..

lib/libfluidsynth.a: fluidsynth/build/Makefile
	cd fluidsynth/build \
	&& $(MAKE) -j$(THREADS) \
	&& $(MAKE) check \
	&& mkdir -p ../../include/ ../../lib \
	&& cp -rv ../include/* ../../include/ \
	&& cp -rv include/* ../../include/ \
	&& rm -f ../../lib/libfluidsynth.a \
	&& ar -qc ../../lib/libfluidsynth.a src/CMakeFiles/libfluidsynth-OBJ.dir/*/*.o \
	&& ranlib ../../lib/libfluidsynth.a


define FLUIDSYNTH_EXPORTS

Export the following to configure dosbox-staging without pkg-config
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
export FLUIDSYNTH_CFLAGS="-I$(CURDIR)/include $(GLIB_CFLAGS)"
export FLUIDSYNTH_LIBS="$(CURDIR)/lib/libfluidsynth.a $(GLIB_LIBS) -lm"

Export the following to configure dosbox-staging with pkg-config
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
export PKG_CONFIG_PATH="$(CURDIR)/fluidsynth/build"
endef

GLIB_LIBS = $(shell pkg-config --libs gthread-2.0 glib-2.0)
GLIB_CFLAGS = $(shell pkg-config --cflags gthread-2.0 glib-2.0)
.PHONY: fluidsynth-message
fluidsynth-message: lib/libfluidsynth.a
	$(info $(FLUIDSYNTH_EXPORTS))

fluidsynth/clean:
	rm -rf fluidsynth/build
