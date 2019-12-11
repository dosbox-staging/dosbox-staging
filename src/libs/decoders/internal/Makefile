##
#  Fetch the latest dependencies from upstream
#  ===========================================
OGG_ARCHIVE = ogg-master.tar.gz
OGG_URL = https://github.com/xiph/ogg/archive/master.tar.gz

OPUS_ARCHIVE = opus-master.tar.gz
OPUS_URL = https://github.com/xiph/opus/archive/master.tar.gz

OPUSFILE_ARCHIVE = opusfile-master.tar.gz
OPUSFILE_URL = https://github.com/xiph/opusfile/archive/master.tar.gz

# We use speex's pkg.m4 to work around a bug in opusfile's autotools config
# that still depends on pkgconfig even if DEPS_ env flags are present.
# Mingw 1.0 (Windows) does not provide pkgconfig, so we need to avoid it.
OPUSFILE_PKG_URL = https://raw.githubusercontent.com/xiph/speex/master/m4/pkg.m4

SPEEXDSP_ARCHIVE = speexdsp-master.tar.gz
SPEEXDSP_URL = https://github.com/xiph/speexdsp/archive/master.tar.gz

##
#  The audio-codecs rely on accurate floating-point calculations
#  and will fail to build (Opus) if they detect too agressive optimizations
#  such as Ofast or ffast-math; therefore we strip just these flags
#  and step-down -Ofast to -O3 as an acceptable fallback.
#
CFLAGS:= $(subst -Ofast,-O3,$(CFLAGS))
CFLAGS:= $(filter-out -ffast-math,$(CFLAGS))
CFLAGS:= $(filter-out -funsafe-math-optimizations,$(CFLAGS))

CXXFLAGS:= $(subst -Ofast,-O3,$(CXXFLAGS))
CXXFLAGS:= $(filter-out -ffast-math,$(CXXFLAGS))
CXXFLAGS:= $(filter-out -funsafe-math-optimizations,$(CXXFLAGS))

LDFLAGS:= $(subst -Ofast,-O3,$(LDLAGS))
LDFLAGS:= $(filter-out -ffast-math,$(LDFLAGS))
LDFLAGS:= $(filter-out -funsafe-math-optimizations,$(LDFLAGS))

##
#  Common commands and arguments
#  =============================
THREADS = $(shell nproc || echo 4)
CURL = curl -s -L
EXTRACT = tar --strip 1 -zxof

##
#  Everything-targets
#  ==================
all: ogg speexdsp opus opusfile
clean: ogg/clean speexdsp/clean opus/clean opusfile/clean
dist-clean:
	rm -rf ogg speexdsp opus opusfile include lib share pkg.m4 *.gz

##
#  Ogg Library
#  ===========
ogg: lib/libogg.a

$(OGG_ARCHIVE):
	$(CURL) "$(OGG_URL)" -o "$(OGG_ARCHIVE)"

ogg/autogen.sh: $(OGG_ARCHIVE)
	test -f $@ \
	|| ( mkdir -p ogg \
	&& $(EXTRACT) $(OGG_ARCHIVE) -C ogg )

ogg/configure: ogg/autogen.sh
	cd ogg \
	&& mkdir -p m4 \
	&& ./autogen.sh

ogg/Makefile: ogg/configure
	cd ogg \
	&& ./configure --disable-shared

lib/libogg.a: ogg/Makefile
	cd ogg \
	&& $(MAKE) -j$(THREADS) \
	&& mkdir -p ../include/ogg ../lib \
	&& cp -v include/ogg/*.h ../include/ogg \
	&& cp -v src/.libs/libogg.a ../lib

ogg/clean:
	cd ogg \
	&& $(MAKE) clean \
	&& rm -f configure Makefile

##
#  Speex DSP Library
#  =================
speexdsp: lib/libspeexdsp.a

$(SPEEXDSP_ARCHIVE):
	$(CURL) "$(SPEEXDSP_URL)" -o "$(SPEEXDSP_ARCHIVE)"

speexdsp/autogen.sh: $(SPEEXDSP_ARCHIVE)
	test -f $@ \
	|| ( mkdir -p speexdsp \
	&& $(EXTRACT) $(SPEEXDSP_ARCHIVE) -C speexdsp )

speexdsp/configure: speexdsp/autogen.sh
	cd speexdsp \
	&& mkdir -p m4 \
	&& ./autogen.sh

# Note: the strange looking sed -i.bak syntax is required for OSX compatibility
#       and is still compatible on Linux and Windows-mingGW (thankfully!)
#
speexdsp/Makefile: speexdsp/configure
	cd speexdsp \
	&& sed -i.bak 's/.*PKG_CHECK_MODULES.*//g' configure \
	&& ./configure --disable-shared --disable-examples --with-fft=smallft

lib/libspeexdsp.a: speexdsp/Makefile
	cd speexdsp \
	&& sed -i.bak 's/@FFT_CFLAGS@//g' libspeexdsp/Makefile \
	&& $(MAKE) -j$(THREADS) \
	&& mkdir -p ../include/speex ../lib \
	&& cp -v include/speex/*.h ../include/speex \
	&& cp -v libspeexdsp/.libs/libspeexdsp.a ../lib

speexdsp/clean:
	cd speexdsp \
	&& $(MAKE) clean \
	&& rm -f configure Makefile

##
#  Opus Library
#  ============
opus: lib/libopus.a

$(OPUS_ARCHIVE):
	$(CURL) "$(OPUS_URL)" -o "$(OPUS_ARCHIVE)"

opus/autogen.sh: $(OPUS_ARCHIVE)
	test -f $@ \
	|| ( mkdir -p opus \
	&& $(EXTRACT) $(OPUS_ARCHIVE) -C opus )

opus/configure: opus/autogen.sh
	cd opus \
	&& mkdir -p m4 \
	&& ./autogen.sh

opus/Makefile: opus/configure
	cd opus \
	&& ./configure --disable-shared --disable-extra-programs --disable-doc

lib/libopus.a: opus/Makefile
	cd opus \
	&& $(MAKE) -j$(THREADS) \
	&& mkdir -p ../include/opus ../lib \
	&& cp -v include/*.h ../include/opus \
	&& cp -v .libs/libopus.a ../lib

opus/clean:
	cd opus \
	&& $(MAKE) clean \
	&& rm -f configure Makefile

##
#  Opusfile Library
#  ================
opusfile: lib/libopusfile.a

pkg.m4:
	$(CURL) "$(OPUSFILE_PKG_URL)" -o "pkg.m4"

$(OPUSFILE_ARCHIVE):
	$(CURL) "$(OPUSFILE_URL)" -o "$(OPUSFILE_ARCHIVE)"

opusfile/autogen.sh: $(OPUSFILE_ARCHIVE)
	test -f $@ \
	|| ( mkdir -p opusfile \
	&& $(EXTRACT) $(OPUSFILE_ARCHIVE) -C opusfile )

opusfile/configure: opusfile/autogen.sh pkg.m4
	cd opusfile \
	&& mkdir -p m4 \
	&& cp ../pkg.m4 m4/ \
	&& ./autogen.sh

opusfile/Makefile: opusfile/configure lib/libopus.a lib/libogg.a
	cd opusfile && \
	DEPS_LIBS="-L../lib -lopus -logg" \
	DEPS_CFLAGS="-I../include -I../include/opus -I../include/ogg" \
	./configure --disable-shared --disable-doc --disable-http --disable-examples

# We run make twice below. We hide the output of the first run because the
# auto-tools generated Makefile launches sed and libtool without quoting the
# current working directory, so those fail when run inside a directory structure
# containing one or more spaces.
#
# Regardless of the sed/libtool issue, if all went well the resulting library will
# exist, which is why we run make a second time. The second make pass doesn't
# rerun the sed and libtool comands, so it acts as a safety check. If there's a
# actual build problem with the source code then it will be caught in this
# second make run and fail without producing a library.
#
lib/libopusfile.a: opusfile/Makefile
	cd opusfile \
	&& ( $(MAKE) -j$(THREADS) > make.log 2>&1 || $(MAKE) ) \
	&& mkdir -p ../include/ ../lib \
	&& cp -v include/*.h ../include/opus \
	&& cp -v .libs/*.a ../lib

opusfile/clean:
	cd opusfile \
	&& $(MAKE) clean \
	&& rm -f configure Makefile
