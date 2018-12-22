dnl Check if we have SDL (sdl-config, header and library) version >= 1.2.0
dnl Extra options: --with-sdl-prefix=PATH and --with-sdl={sdl12,sdl2}
dnl Output:
dnl SDL_CFLAGS and SDL_LIBS are set and AC_SUBST-ed
dnl HAVE_SDL_H is AC_DEFINE-d

AC_DEFUN([EXULT_CHECK_SDL],[
  exult_backupcppflags="$CPPFLAGS"
  exult_backupldflags="$LDFLAGS"
  exult_backuplibs="$LIBS"

  exult_sdlok=yes

  AC_ARG_WITH(sdl-prefix,[  --with-sdl-prefix=PFX   Prefix where SDL is installed (optional)], sdl_prefix="$withval", sdl_prefix="")
  AC_ARG_WITH(sdl-exec-prefix,[  --with-sdl-exec-prefix=PFX Exec prefix where SDL is installed (optional)], sdl_exec_prefix="$withval", sdl_exec_prefix="")
  AC_ARG_WITH(sdl,       [  --with-sdl=sdl12,sdl2   Select a specific version of SDL to use (optional)], sdl_ver="$withval", sdl_ver="")

  dnl First: find sdl-config or sdl2-config
  exult_extra_path=$prefix/bin:$prefix/usr/bin
  sdl_args=""
  if test x$sdl_exec_prefix != x ; then
     sdl_args="$sdl_args --exec-prefix=$sdl_exec_prefix"
     exult_extra_path=$sdl_exec_prefix/bin
  fi
  if test x$sdl_prefix != x ; then
     sdl_args="$sdl_args --prefix=$sdl_prefix"
     exult_extra_path=$sdl_prefix/bin
  fi
  if test x"$sdl_ver" = xsdl12 ; then
    exult_sdl_progs=sdl-config
  elif test x"$sdl_ver" = xsdl2 ; then
    exult_sdl_progs=sdl2-config
  else
    dnl NB: This line implies we prefer SDL 1.2 to SDL 2.0
    exult_sdl_progs="sdl-config sdl2-config"
  fi
  AC_PATH_PROGS(SDL_CONFIG, $exult_sdl_progs, no, [$exult_extra_path:$PATH])
  if test "$SDL_CONFIG" = "no" ; then
    exult_sdlok=no
  else
    SDL_CFLAGS=`$SDL_CONFIG $sdl_args --cflags`
    SDL_LIBS=`$SDL_CONFIG $sdl_args --libs`

    sdl_major_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    sdl_minor_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    sdl_patchlevel=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test $sdl_major_version -eq 1 ; then
      sdl_ver=sdl12
    else
      sdl_ver=sdl2
    fi
  fi

  if test x"$sdl_ver" = xsdl2 ; then
    REQ_MAJOR=2
    REQ_MINOR=0
    REQ_PATCHLEVEL=0
  else
    REQ_MAJOR=1
    REQ_MINOR=2
    REQ_PATCHLEVEL=0
  fi
  REQ_VERSION=$REQ_MAJOR.$REQ_MINOR.$REQ_PATCHLEVEL

  AC_MSG_CHECKING([for SDL - version >= $REQ_VERSION])


  dnl Second: include "SDL.h"

  if test x$exult_sdlok = xyes ; then
    CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
    #include "SDL.h"

    int main(int argc, char *argv[])
    {
      return 0;
    }
    ]],)],sdlh_found=yes,sdlh_found=no)

    if test x$sdlh_found = xno; then
      exult_sdlok=no
    else
      AC_DEFINE(HAVE_SDL_H, 1, [Define to 1 if you have the "SDL.h" header file])
    fi
  fi

  dnl Next: version check (cross-compile-friendly idea borrowed from autoconf)
  dnl (First check version reported by sdl-config, then confirm
  dnl  the version in SDL.h matches it)

  if test x$exult_sdlok = xyes ; then

    if test ! \( \( $sdl_major_version -gt $REQ_MAJOR \) -o \( \( $sdl_major_version -eq $REQ_MAJOR \) -a \( \( $sdl_minor_version -gt $REQ_MINOR \) -o \( \( $sdl_minor_version -eq $REQ_MINOR \) -a \( $sdl_patchlevel -gt $REQ_PATCHLEVEL \) \) \) \) \); then
      exult_sdlok="no, version < $REQ_VERSION found"
    else
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
      #include "SDL.h"

      int main(int argc, char *argv[])
      {
        static int test_array[1-2*!(SDL_MAJOR_VERSION==$sdl_major_version&&SDL_MINOR_VERSION==$sdl_minor_version&&SDL_PATCHLEVEL==$sdl_patchlevel)];
        test_array[0] = 0;
        return 0;
      }
      ]])],,[[exult_sdlok="no, version of SDL.h doesn't match that of sdl-config"]])
    fi
  fi

  dnl Next: try linking

  if test "x$exult_sdlok" = xyes; then
    LIBS="$LIBS $SDL_LIBS"

    AC_LINK_IFELSE([AC_LANG_SOURCE([[
    #include "SDL.h"

    int main(int argc, char* argv[]) {
      SDL_Init(0);
      return 0;
    }
    ]])],sdllinkok=yes,sdllinkok=no)
    if test x$sdllinkok = xno; then
      exult_sdlok=no
    fi
  fi

  AC_MSG_RESULT($exult_sdlok)

  LDFLAGS="$exult_backupldflags"
  CPPFLAGS="$exult_backupcppflags"
  LIBS="$exult_backuplibs"

  if test "x$exult_sdlok" = xyes; then
    AC_SUBST(SDL_CFLAGS)
    AC_SUBST(SDL_LIBS)
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi
]);

dnl Configure Paths for Alsa
dnl Some modifications by Richard Boulton <richard-alsa@tartarus.org>
dnl Christopher Lansdown <lansdoct@cs.alfred.edu>
dnl Jaroslav Kysela <perex@suse.cz>
dnl Last modification: alsa.m4,v 1.22 2002/05/27 11:14:20 tiwai Exp
dnl AM_PATH_ALSA([MINIMUM-VERSION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for libasound, and define ALSA_CFLAGS and ALSA_LIBS as appropriate.
dnl enables arguments --with-alsa-prefix=
dnl                   --with-alsa-enc-prefix=
dnl                   --disable-alsatest  (this has no effect, as yet)
dnl
dnl For backwards compatibility, if ACTION_IF_NOT_FOUND is not specified,
dnl and the alsa libraries are not found, a fatal AC_MSG_ERROR() will result.
dnl
AC_DEFUN([AM_PATH_ALSA],
[dnl Save the original CFLAGS, LDFLAGS, and LIBS
alsa_save_CFLAGS="$CFLAGS"
alsa_save_LDFLAGS="$LDFLAGS"
alsa_save_LIBS="$LIBS"
alsa_found=yes

dnl
dnl Get the cflags and libraries for alsa
dnl
AC_ARG_WITH(alsa-prefix,
[  --with-alsa-prefix=PFX  Prefix where Alsa library is installed(optional)],
[alsa_prefix="$withval"], [alsa_prefix=""])

AC_ARG_WITH(alsa-inc-prefix,
[  --with-alsa-inc-prefix=PFX  Prefix where include libraries are (optional)],
[alsa_inc_prefix="$withval"], [alsa_inc_prefix=""])

dnl FIXME: this is not yet implemented
AC_ARG_ENABLE(alsatest,
[  --disable-alsatest      Do not try to compile and run a test Alsa program],
[enable_alsatest=no],
[enable_alsatest=yes])

dnl Add any special include directories
AC_MSG_CHECKING(for ALSA CFLAGS)
if test "$alsa_inc_prefix" != "" ; then
	ALSA_CFLAGS="$ALSA_CFLAGS -I$alsa_inc_prefix"
	CFLAGS="$CFLAGS -I$alsa_inc_prefix"
fi
AC_MSG_RESULT($ALSA_CFLAGS)

dnl add any special lib dirs
AC_MSG_CHECKING(for ALSA LDFLAGS)
if test "$alsa_prefix" != "" ; then
	ALSA_LIBS="$ALSA_LIBS -L$alsa_prefix"
	LDFLAGS="$LDFLAGS $ALSA_LIBS"
fi

dnl add the alsa library
ALSA_LIBS="$ALSA_LIBS -lasound -lm -ldl -lpthread"
LIBS=`echo $LIBS | sed 's/-lm//'`
LIBS=`echo $LIBS | sed 's/-ldl//'`
LIBS=`echo $LIBS | sed 's/-lpthread//'`
LIBS=`echo $LIBS | sed 's/  //'`
LIBS="$ALSA_LIBS $LIBS"
AC_MSG_RESULT($ALSA_LIBS)

dnl Check for a working version of libasound that is of the right version.
min_alsa_version=ifelse([$1], ,0.1.1,$1)
AC_MSG_CHECKING(for libasound headers version >= $min_alsa_version)
no_alsa=""
    alsa_min_major_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    alsa_min_minor_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    alsa_min_micro_version=`echo $min_alsa_version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

AC_LANG_SAVE
AC_LANG_C
AC_TRY_COMPILE([
#include <alsa/asoundlib.h>
], [
/* ensure backward compatibility */
#if !defined(SND_LIB_MAJOR) && defined(SOUNDLIB_VERSION_MAJOR)
#define SND_LIB_MAJOR SOUNDLIB_VERSION_MAJOR
#endif
#if !defined(SND_LIB_MINOR) && defined(SOUNDLIB_VERSION_MINOR)
#define SND_LIB_MINOR SOUNDLIB_VERSION_MINOR
#endif
#if !defined(SND_LIB_SUBMINOR) && defined(SOUNDLIB_VERSION_SUBMINOR)
#define SND_LIB_SUBMINOR SOUNDLIB_VERSION_SUBMINOR
#endif

#  if(SND_LIB_MAJOR > $alsa_min_major_version)
  exit(0);
#  else
#    if(SND_LIB_MAJOR < $alsa_min_major_version)
#       error not present
#    endif

#   if(SND_LIB_MINOR > $alsa_min_minor_version)
  exit(0);
#   else
#     if(SND_LIB_MINOR < $alsa_min_minor_version)
#          error not present
#      endif

#      if(SND_LIB_SUBMINOR < $alsa_min_micro_version)
#        error not present
#      endif
#    endif
#  endif
exit(0);
],
  [AC_MSG_RESULT(found.)],
  [AC_MSG_RESULT(not present.)
   ifelse([$3], , [AC_MSG_ERROR(Sufficiently new version of libasound not found.)])
   alsa_found=no]
)
AC_LANG_RESTORE

dnl Now that we know that we have the right version, let's see if we have the library and not just the headers.
AC_CHECK_LIB([asound], [snd_ctl_open],,
	[ifelse([$3], , [AC_MSG_ERROR(No linkable libasound was found.)])
	 alsa_found=no]
)

if test "x$alsa_found" = "xyes" ; then
   ifelse([$2], , :, [$2])
   LIBS=`echo $LIBS | sed 's/-lasound//g'`
   LIBS=`echo $LIBS | sed 's/  //'`
   LIBS="-lasound $LIBS"
fi
if test "x$alsa_found" = "xno" ; then
   ifelse([$3], , :, [$3])
   CFLAGS="$alsa_save_CFLAGS"
   LDFLAGS="$alsa_save_LDFLAGS"
   LIBS="$alsa_save_LIBS"
   ALSA_CFLAGS=""
   ALSA_LIBS=""
fi

dnl That should be it.  Now just export out symbols:
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
])

dnl Configure platform for SDL_cdrom compatibility layer.
dnl MUST be called after SDL version check (defined only for SDL 2.0).
dnl Taken off configure.in from SDL 1.2 and then modified
AC_DEFUN([COMPAT_SDL_CDROM_GET_PLATFORM],[

if test x"$sdl_ver" = xsdl12 ; then
    compat_sdl_cdrom_arch=undefined
elif test x"$sdl_ver" = xsdl2 ; then
    case "$host" in
        arm-*-elf*) # FIXME: Can we get more specific for iPodLinux?
            compat_sdl_cdrom_arch=linux
            ;;
        *-*-linux*|*-*-uclinux*|*-*-gnu*|*-*-k*bsd*-gnu|*-*-bsdi*|*-*-freebsd*|*-*-dragonfly*|*-*-netbsd*|*-*-openbsd*|*-*-sysv5*|*-*-solaris*|*-*-hpux*|*-*-irix*|*-*-aix*|*-*-osf*)
            case "$host" in
                *-*-linux*)         compat_sdl_cdrom_arch=linux ;;
                *-*-uclinux*)       compat_sdl_cdrom_arch=linux ;;
                *-*-kfreebsd*-gnu)  compat_sdl_cdrom_arch=COMPAT_SDL_CDROM_PLATFORM_KFREEBSD_GNU ;;
                *-*-knetbsd*-gnu)   compat_sdl_cdrom_arch=knetbsd-gnu ;;
                *-*-kopenbsd*-gnu)  compat_sdl_cdrom_arch=kopenbsd-gnu ;;
                *-*-gnu*)           compat_sdl_cdrom_arch=gnu ;; # must be last of the gnu variants
                *-*-bsdi*)          compat_sdl_cdrom_arch=bsdi ;;
                *-*-freebsd*)       compat_sdl_cdrom_arch=freebsd ;;
                *-*-dragonfly*)     compat_sdl_cdrom_arch=freebsd ;;
                *-*-netbsd*)        compat_sdl_cdrom_arch=netbsd ;;
                *-*-openbsd*)       compat_sdl_cdrom_arch=openbsd ;;
                *-*-sysv5*)         compat_sdl_cdrom_arch=sysv5 ;;
                *-*-solaris*)       compat_sdl_cdrom_arch=solaris ;;
                *-*-hpux*)          compat_sdl_cdrom_arch=hpux ;;
                *-*-irix*)          compat_sdl_cdrom_arch=irix ;;
                *-*-aix*)           compat_sdl_cdrom_arch=aix ;;
                *-*-osf*)           compat_sdl_cdrom_arch=osf ;;
            esac
            ;;
        *-*-qnx*)
            compat_sdl_cdrom_arch=qnx
            ;;
        *-*-cygwin* | *-*-mingw32*)
            compat_sdl_cdrom_arch=win32
            ;;
        *-wince*)
            compat_sdl_cdrom_arch=win32
            ;;
        *-*-beos* | *-*-haiku*)
            compat_sdl_cdrom_arch=beos
            ;;
        *-*-darwin* )
            # This could be either full "Mac OS X", or plain "Darwin" which is
            # just the OS X kernel sans upper layers like Carbon and Cocoa.
            # Next line is broken
            compat_sdl_cdrom_arch=macosx
            ;;
        *-*-mint*)
            compat_sdl_cdrom_arch=mint
            ;;
        *-riscos)
            compat_sdl_cdrom_arch=riscos
            ;;
        *)
            compat_sdl_cdrom_arch=undefined
            ;;
    esac
else
    AC_MSG_ERROR([Compatible SDL version not found])
fi

]);

AH_TOP([
/*
 *  Copyright (C) 2002-2018  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
])

AH_BOTTOM([#if C_ATTRIBUTE_ALWAYS_INLINE
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif])

AH_BOTTOM([#if C_ATTRIBUTE_FASTCALL
#define DB_FASTCALL __attribute__((fastcall))
#else
#define DB_FASTCALL
#endif])


AH_BOTTOM([#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif])

AH_BOTTOM([#if C_HAS_BUILTIN_EXPECT
#define GCC_UNLIKELY(x) __builtin_expect((x),0)
#define GCC_LIKELY(x) __builtin_expect((x),1)
#else
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)
#endif])

AH_BOTTOM([
typedef         double     Real64;

#if SIZEOF_UNSIGNED_CHAR != 1
#  error "sizeof (unsigned char) != 1"
#else
  typedef unsigned char Bit8u;
  typedef   signed char Bit8s;
#endif

#if SIZEOF_UNSIGNED_SHORT != 2
#  error "sizeof (unsigned short) != 2"
#else
  typedef unsigned short Bit16u;
  typedef   signed short Bit16s;
#endif

#if SIZEOF_UNSIGNED_INT == 4
  typedef unsigned int Bit32u;
  typedef   signed int Bit32s;
#elif SIZEOF_UNSIGNED_LONG == 4
  typedef unsigned long Bit32u;
  typedef   signed long Bit32s;
#else
#  error "can't find sizeof(type) of 4 bytes!"
#endif

#if SIZEOF_UNSIGNED_LONG == 8
  typedef unsigned long Bit64u;
  typedef   signed long Bit64s;
#elif SIZEOF_UNSIGNED_LONG_LONG == 8
  typedef unsigned long long Bit64u;
  typedef   signed long long Bit64s;
#else
#  error "can't find data type of 8 bytes"
#endif

#if SIZEOF_INT_P == 4
  typedef Bit32u Bitu;
  typedef Bit32s Bits;
#else
  typedef Bit64u Bitu;
  typedef Bit64s Bits;
#endif

])
