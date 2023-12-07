##
#  Fetch the latest dependencies from upstream
#  -------------------------------------------
ZLIB-NG_ARCHIVE = zlib-ng.tar.gz
ZLIB-NG_URL = https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.1.5.tar.gz
ZLIB-NG_SHA256 = 3f6576971397b379d4205ae5451ff5a68edf6c103b2f03c4188ed7075fbb5f04

##
#  Common commands and arguments
#  -----------------------------
PLATFORM = $(shell uname -s)-$(shell uname -m)
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
all: zlib-ng
clean: zlib-ng/clean
distclean:
	rm -rf $(PLATFORM) *.gz

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
	     && echo "  - $(ZLIB-NG_URL) to ./$(ZLIB-NG_ARCHIVE)"                \
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
#  Zlib-NG Library
#  ---------------
zlib-ng: zlib-ng-message

zlib-ng: $(PLATFORM)/lib/libz.a

$(ZLIB-NG_ARCHIVE):
	$(eval URL := $(ZLIB-NG_URL))
	@$(download)
	$(eval SUM := $(ZLIB-NG_SHA256))
	$(eval FILENAME := $(ZLIB-NG_ARCHIVE))
	@$(checksum)

$(PLATFORM)/zlib-ng/build: $(ZLIB-NG_ARCHIVE)
	@test -f $@ \
	|| (mkdir -p $(PLATFORM)/zlib-ng/build \
	&& $(EXTRACT) $(ZLIB-NG_ARCHIVE) -C $(PLATFORM)/zlib-ng)

$(PLATFORM)/zlib-ng/build/CMakeCache.txt: $(PLATFORM)/zlib-ng/build
	cd $(PLATFORM)/zlib-ng \
	&& cd build \
	&& cmake \
	-DBUILD_SHARED_LIBS=off \
	-DCMAKE_INSTALL_PREFIX="$(DIR)/$(PLATFORM)" \
	-DWITH_GTEST=off \
	-DWITH_GZFILEOP=off \
	-DWITH_SANITIZER=off \
	-DZLIB_COMPAT=on \
	-DZLIB_ENABLE_TESTS=off \
	-DZLIBNG_ENABLE_TESTS=off \
	..

$(PLATFORM)/lib/libz.a: $(PLATFORM)/zlib-ng/build/CMakeCache.txt
	cd $(PLATFORM)/zlib-ng/build \
	&& cmake \
	--build . \
	--target install

define ZLIB-NG_EXPORTS

Export the following to use zlib-ng in pkgconfig
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
export PKG_CONFIG_PATH="$(CURDIR)/$(PLATFORM)/lib/pkgconfig"
endef

.PHONY: zlib-ng-message
zlib-ng-message: $(PLATFORM)/lib/libz.a
	$(info $(ZLIB-NG_EXPORTS))

zlib-ng/clean:
	rm -rf $(PLATFORM)/zlib-ng/build
