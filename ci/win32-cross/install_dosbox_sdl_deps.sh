#!/bin/bash

set -e
set -x

INSTALL_PREFIX='/usr/i686-w64-mingw32'
TOOLCHAIN='i686-w64-mingw32'

export PATH="$INSTALL_PREFIX/bin:$PATH"

#
# Adding the SDL libraries to MinGW
#
if [ ! -e 'SDL-devel-1.2.15-mingw32.tar.gz' ]; then
  wget 'http://www.libsdl.org/release/SDL-devel-1.2.15-mingw32.tar.gz'
fi
rm -Rf 'SDL-1.2.15'
tar zxvf 'SDL-devel-1.2.15-mingw32.tar.gz'
sudo make -C 'SDL-1.2.15' install-sdl prefix="$INSTALL_PREFIX"


#
# Adding Direct Draw support to DOSBox (Optional)
#
if [ ! -e 'directx-devel.tar.gz' ]; then
  wget 'http://www.libsdl.org/extras/win32/common/directx-devel.tar.gz'
fi
sudo tar -C "$INSTALL_PREFIX" -zxvf 'directx-devel.tar.gz'


#
# Adding networking support to DOSBox (Optional)
#
if [ ! -e 'SDL_net-1.2.8.tar.gz' ]; then
  wget 'https://www.libsdl.org/projects/SDL_net/release/SDL_net-1.2.8.tar.gz'
fi
rm -Rf 'SDL_net-1.2.8'
tar zxvf 'SDL_net-1.2.8.tar.gz'
cd 'SDL_net-1.2.8'
./configure --host="$TOOLCHAIN" --prefix="$INSTALL_PREFIX"
echo '#include <winerror.h>' | cat - SDLnet.c > SDLnet.c.patch
mv SDLnet.c.patch SDLnet.c
make
sudo make install
cd -


#
# Adding screenshot support (Optional)
#
if [ ! -e 'zlib-1.2.8.tar.xz' ]; then
  wget 'http://sourceforge.net/projects/libpng/files/zlib/1.2.8/zlib-1.2.8.tar.xz/download'
  mv download zlib-1.2.8.tar.xz
fi

rm -Rf 'zlib-1.2.8'
tar xvf 'zlib-1.2.8.tar.xz'
make PREFIX="$TOOLCHAIN"- -C 'zlib-1.2.8' -f 'win32/Makefile.gcc'
sudo cp -v zlib-1.2.8/libz.a "$INSTALL_PREFIX/lib/"
sudo cp -v zlib-1.2.8/{zlib.h,zconf.h} "$INSTALL_PREFIX/include/"

if [ ! -e 'libpng-1.6.18.tar.xz' ]; then
  wget 'https://sourceforge.net/projects/libpng/files/libpng16/older-releases/1.6.18/libpng-1.6.18.tar.xz/download'
  mv download libpng-1.6.18.tar.xz
fi
rm -Rf 'libpng-1.6.18'
tar xvf 'libpng-1.6.18.tar.xz'
cd 'libpng-1.6.18'
./configure --host="$TOOLCHAIN" --disable-shared --prefix="$INSTALL_PREFIX"
make
sudo make install
cd -


#
# Adding support for compressed audio on diskimages (Optional)
#
if [ ! -e 'libogg-1.3.2.tar.gz' ]; then
  wget 'http://downloads.xiph.org/releases/ogg/libogg-1.3.2.tar.gz'
fi
rm -Rf 'libogg-1.3.2'
tar xvf 'libogg-1.3.2.tar.gz'
cd 'libogg-1.3.2'
./configure --host="$TOOLCHAIN" --disable-shared --prefix="$INSTALL_PREFIX"
make
sudo make install
cd -

if [ ! -e 'libvorbis-1.3.5.tar.gz' ]; then
  wget 'http://downloads.xiph.org/releases/vorbis/libvorbis-1.3.5.tar.gz'
fi
rm -Rf 'libvorbis-1.3.5'
tar xvf 'libvorbis-1.3.5.tar.gz'
cd 'libvorbis-1.3.5'
./configure --host="$TOOLCHAIN" --disable-shared --prefix="$INSTALL_PREFIX"
make
sudo make install
cd -

if [ ! -e 'SDL_sound-1.0.3.tar.gz' ]; then
  wget 'https://www.icculus.org/SDL_sound/downloads/SDL_sound-1.0.3.tar.gz'
fi
rm -Rf 'SDL_sound-1.0.3'
tar xvf 'SDL_sound-1.0.3.tar.gz'
cd 'SDL_sound-1.0.3'
./configure --host="$TOOLCHAIN" --disable-shared --prefix="$INSTALL_PREFIX" LIBS="-lvorbisfile -lvorbis -logg"
make
sudo make install
cd -


#
# Enabling the debugger (You probably don't want this)
#
if [ ! -e 'PDCurses-3.4.tar.gz' ]; then
  wget 'http://sourceforge.net/projects/pdcurses/files/pdcurses/3.4/PDCurses-3.4.tar.gz'
fi
rm -Rf 'PDCurses-3.4'
tar xvf 'PDCurses-3.4.tar.gz'
make -C PDCurses-3.4/win32 -f gccwin32.mak DLL=N CC="$TOOLCHAIN-gcc" LINK="$TOOLCHAIN-gcc"
sudo cp PDCurses-3.4/win32/pdcurses.a "$INSTALL_PREFIX/lib/libpdcurses.a"
sudo cp PDCurses-3.4/{curses.h,panel.h} "$INSTALL_PREFIX/include/"





