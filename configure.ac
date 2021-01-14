dnl Init.
AC_INIT(dosbox,git)
AC_DEFINE([CONF_BRAND],["staging-git"],[Suffix of the .conf file.])
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(README)
AC_CONFIG_MACRO_DIR([m4])

dnl Turn on less verbose makefiles by default
dnl Change to 'no' or comment-out when debugging the buildsystem
AM_SILENT_RULES([yes])

dnl Check for pkg-config
PKG_PROG_PKG_CONFIG

dnl Detect the canonical host and target build environment
AC_CANONICAL_HOST
AC_CANONICAL_BUILD

dnl Setup for automake
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER(config.h)

dnl Checks for programs.
AM_PROG_AR
AC_PROG_MAKE_SET
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB

dnl Prevent autoconf from leaking non-understood tokens into the configure script
m4_pattern_forbid([PKG_PROG_PKG_CONFIG])
m4_pattern_forbid([PKG_CHECK_MODULES])
m4_pattern_forbid([^AC_])
m4_pattern_forbid([^AH_])
m4_pattern_forbid([^AX_])

dnl Configure POSIX thread support
m4_pattern_allow(AX_PTHREAD_ZOS_MISSING)
m4_pattern_allow(AX_PTHREAD_CC_IS_CLANG)
AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

dnl Check for SDL
SDL_VERSION=2.0.2
AM_PATH_SDL($SDL_VERSION,
            :,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)
PRESDL_LIBS="$LIBS"
LIBS="$LIBS $SDL_LIBS"
CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"

AH_TEMPLATE(C_MODEM,[Define to 1 to enable internal modem support, requires SDL2_net])
AH_TEMPLATE(C_IPX,[Define to 1 to enable IPX over Internet networking, requires SDL2_net])
AC_ARG_ENABLE(network,
              AS_HELP_STRING([--disable-network],
                             [Disable networking features (modem, ipx)]),,
              enable_network=yes)
AC_CHECK_HEADER(SDL_net.h, have_sdl_net_h=yes,)
AC_CHECK_LIB(SDL2_net, SDLNet_Init, have_sdl_net_lib=yes, , )
AC_MSG_CHECKING([whether networking features will be enabled])
if test x$enable_network = xyes ; then
  if test x$have_sdl_net_lib = xyes -a x$have_sdl_net_h = xyes ; then
    libtype=""
    if test x$enable_sdl_static = xyes ; then
      libtype="(static)"
      case "$host" in
        *-*-darwin*)
          LIBS="$PRESDL_LIBS /usr/local/lib/libSDL2_net.a $SDL_LIBS"
          ;;
        *)
          AC_MSG_WARN(m4_normalize([Statically linking SDL2 is unreliable.
            Please ensure that "libSDL2_net.a" is available.]))
          LIBS="$PRESDL_LIBS -Wl,-Bstatic -lSDL2_net -Wl,-Bdynamic $SDL_LIBS"
          ;;
      esac
    else
      LIBS="$PRESDL_LIBS -lSDL2_net $SDL_LIBS"
    fi
    AC_DEFINE(C_MODEM,1)
    AC_DEFINE(C_IPX,1)
    AC_MSG_RESULT([yes $libtype])
    case "$host_os" in haiku*)
      AC_CHECK_LIB(network, socket, [],
                   [AC_MSG_ERROR([Can't find a useable network libary])],
                   -l network)
      ;;
    esac
  else
    AC_MSG_WARN([Can't find SDL2_net, internal modem and ipx disabled])
  fi
else
  AC_MSG_RESULT([no])
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_STRUCT_TM
AC_CHECK_SIZEOF(int *)

dnl Require compiler with C++14 support.
dnl Allow GNU extensions to work around MinGW bugs around POSIX compatibility.
AC_LANG(C++)
AX_CXX_COMPILE_STDCXX_14(ext, mandatory)

dnl Checks for header files.

dnl some semi complex check for sys/socket so it works on darwin as well
AC_CHECK_HEADERS([stdlib.h sys/types.h])
AC_CHECK_HEADERS([sys/socket.h  netinet/in.h pwd.h], [], [],
[#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])

dnl check for the socklen_t (darwin doesn't always have it)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])],[],[AC_DEFINE([socklen_t],[int],[Define to `int` if you don't have socklen_t])])

AC_MSG_CHECKING(if environ can be included)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
#include <stdlib.h>]],[[*environ;]])],
[AC_MSG_RESULT(yes);AC_DEFINE(ENVIRON_INCLUDED,1,[environ can be included])],AC_MSG_RESULT(no))

AC_MSG_CHECKING(if environ can be linked)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[extern char ** environ;]],[[*environ;]])],
[AC_MSG_RESULT(yes);AC_DEFINE(ENVIRON_LINKED,1,[environ can be linked])],AC_MSG_RESULT(no))

AC_MSG_CHECKING([if dirent includes d_type])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <sys/types.h>
#include <dirent.h>
void blah(){
struct dirent d_test;
d_test.d_type = 0;
}])],[AC_MSG_RESULT(yes);AC_DEFINE(HAVE_STRUCT_DIRENT_D_TYPE,1,[struct dirent has d_type])],AC_MSG_RESULT(no))

dnl Look for clock_gettime, HAVE_CLOCK_GETTIME is set when present
AH_TEMPLATE([HAVE_CLOCK_GETTIME],[Determines if the function clock_gettime is available.])
AC_SEARCH_LIBS([clock_gettime], [rt] , [found_clock_gettime=yes], [found_clock_gettime=no])
if test x$found_clock_gettime = xyes; then
  AC_DEFINE(HAVE_CLOCK_GETTIME)
fi

dnl Checks for libraries.

#Check if the compiler support attributes
AH_TEMPLATE([C_HAS_ATTRIBUTE],[Determines if the compiler supports attributes for structures.])
AC_MSG_CHECKING(if compiler allows __attribute__)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
typedef struct { } __attribute__((packed)) junk;]],
[[ ]])],[ AC_MSG_RESULT(yes);AC_DEFINE(C_HAS_ATTRIBUTE)],AC_MSG_RESULT(no))


#Check if the compiler supports certain attributes
OLDCFLAGS="$CFLAGS"
CFLAGS="-Werror"

CFLAGS="$OLDCFLAGS"


#Check if the compiler supports __builtin_expect
#Switch language to c++
AC_LANG_PUSH(C++)
AH_TEMPLATE([C_HAS_BUILTIN_EXPECT],[Determines if the compilers supports __builtin_expect for branch prediction.])
AC_MSG_CHECKING(if compiler allows __builtin_expect)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[
int x=10;if( __builtin_expect ((x==1),0) ) ;
]])], [ AC_MSG_RESULT(yes);AC_DEFINE(C_HAS_BUILTIN_EXPECT)],AC_MSG_RESULT(no))
#switch language back
AC_LANG_POP(C++)

dnl When on macOS, enable support for Apple's Core MIDI and/or Core Audio if our compiler can #include their headers
case "$host" in
  *-*-darwin*)
   AC_MSG_CHECKING(if compiler supports Apple's Core MIDI headers)
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
   #include <CoreMIDI/MIDIServices.h>
   int blah() { return 0; }
   ]])], [AC_MSG_RESULT(yes);LIBS="$LIBS -framework CoreMIDI -framework CoreFoundation";AC_DEFINE([C_SUPPORTS_COREMIDI], [],
   [Compiler supports Core MIDI headers])],
   AC_MSG_RESULT(no);AC_MSG_WARN([Compiler can't compile Apple headers. CoreMIDI functionality disabled. Please use the Apple compiler!]))

   AC_MSG_CHECKING(if compiler supports Apple's Core Audio headers)
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
   #include <AudioToolbox/AUGraph.h>
   #include <CoreServices/CoreServices.h>
   int blah() { return 0; }
   ]])], [AC_MSG_RESULT(yes);LIBS="$LIBS -framework AudioUnit -framework AudioToolbox";AC_DEFINE([C_SUPPORTS_COREAUDIO], [],
   [Compiler supports Core Audio headers])],
   AC_MSG_RESULT(no);AC_MSG_WARN([Compiler can't compile Apple headers. CoreAudio functionality disabled. Please use the Apple compiler!]))
   ;;
esac

dnl enable disable alsa and pass it's cflags to CXXFLAGS
AC_ARG_ENABLE(alsa-midi,
AC_HELP_STRING([--enable-alsa-midi],[compile with alsa midi support (default yes)]),
[ case "${enableval}" in
 yes) alsa_midi=true;;
 no)  alsa_midi=false;;
esac],
[alsa_midi=true])
if test x$alsa_midi = xtrue ; then
  AM_PATH_ALSA(0.9.0, AC_DEFINE(HAVE_ALSA,1,[Define to 1 to use ALSA for MIDI]) , : )
  CXXFLAGS="$CXXFLAGS $ALSA_CFLAGS"
fi

#Features to enable/disable
AH_TEMPLATE(C_DEBUG,[Define to 1 to enable internal debugger, requires ncurses or pdcurses])
AH_TEMPLATE(C_HEAVY_DEBUG,[Define to 1 to enable heavy debugging, requires C_DEBUG])
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Enable internal debugger]),,)
AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug=heavy], [Enable internal debugger with heavy debugging]),,)
if test x$enable_debug = xyes -o x$enable_debug = xheavy; then
    AC_MSG_NOTICE([debugger was requested, finding curses library ...])
    PKG_CHECK_MODULES(NCURSES, ncurses, [
        curses=ncurses
        LIBS="$LIBS ${NCURSES_LIBS}"
        CPPFLAGS="$CPPFLAGS ${NCURSES_CFLAGS}"], [
    PKG_CHECK_MODULES(NCURSESW, ncursesw, [
        curses=ncursesw
        LIBS="$LIBS ${NCURSESW_LIBS}"
        CPPFLAGS="$CPPFLAGS ${NCURSESW_CFLAGS}"], [
    PKG_CHECK_MODULES(PDCURSES, pdcurses, [
        curses=pdcurses
        LIBS="$LIBS ${PDCURSES_LIBS}"
        CPPFLAGS="$CPPFLAGS ${PDCURSES_CFLAGS}"],[]) ]) ])
    if test x$enable_debug = xyes; then
        AC_DEFINE(C_DEBUG,1)
        AC_MSG_NOTICE([debugger enabled using the $curses library])
    else
        AC_DEFINE(C_DEBUG,1)
        AC_DEFINE(C_HEAVY_DEBUG,1)
        AC_MSG_NOTICE([debugger with heavy debugging enabled using the $curses library])
    fi
elif test empty$enable_debug != empty -a x$enable_debug != xno; then
    AC_MSG_ERROR(--enable-debug=$enable_debug was requested but the value "$enable_debug" is invalid)
fi

AH_TEMPLATE(C_CORE_INLINE,[Define to 1 to use inlined memory functions in cpu core])
AC_ARG_ENABLE(core-inline,AC_HELP_STRING([--disable-core-inline],[Disable inlined memory handling in CPU Core]),,enable_core_inline=yes)
AC_MSG_CHECKING(whether memory handling in the CPU Core will be inlined)
if test x$enable_core_inline = xyes ; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(C_CORE_INLINE,1)
else
    AC_MSG_RESULT(no)
fi

dnl The target cpu checks for dynamic cores
AH_TEMPLATE(C_TARGETCPU,[The type of cpu this target has])
AC_MSG_CHECKING(for target cpu type)
case "$host_cpu" in
  x86_64 | amd64)
    AC_DEFINE(C_TARGETCPU,X86_64)
    AC_MSG_RESULT(x86-64 bit compatible)
    c_targetcpu="x86_64"
    c_unalignedmemory=yes
    ;;
  i?86)
    AC_DEFINE(C_TARGETCPU,X86)
    AC_MSG_RESULT(x86 compatible)
    c_targetcpu="x86"
    c_unalignedmemory=yes
    ;;
   powerpc64le*)
    AC_DEFINE(C_TARGETCPU,PPC64LE)
    AC_DEFINE(PAGESIZE,65536,Non-4K page size; currently ppc64le only)
    AC_MSG_RESULT(OpenPOWER)
    c_targetcpu="ppc64le"
    c_unalignedmemory=yes
    ;;
   powerpc*)
    AC_DEFINE(C_TARGETCPU,POWERPC)
    AC_MSG_RESULT(PowerPC)
    c_targetcpu="powerpc"
    c_unalignedmemory=yes
    ;;
   m68k*)
    AC_DEFINE(C_TARGETCPU,M68K)
    AC_MSG_RESULT(Motorola 68000)
    c_targetcpu="m68k"
    c_unalignedmemory=yes
    ;;
   armv7l)
    AC_DEFINE(C_TARGETCPU,ARMV7LE)
    AC_MSG_RESULT(ARMv7 Little Endian)
    c_targetcpu="arm"
    c_unalignedmemory=yes
    ;;
   armv6l)
    AC_DEFINE(C_TARGETCPU,ARMV4LE)
    AC_MSG_RESULT(ARMv6 Little Endian)
    c_targetcpu="arm"
    dnl c_unalignedmemory=yes
    ;;
   aarch64)
    AC_DEFINE(C_TARGETCPU,ARMV8LE)
    AC_MSG_RESULT(ARMv8 Little Endian 64-bit)
    c_targetcpu="arm"
    c_unalignedmemory=yes
    ;;
   *)
    AC_DEFINE(C_TARGETCPU,UNKNOWN)
    AC_MSG_RESULT(unknown)
    c_unalignedmemory=no
    ;;
esac

dnl check for size of pointer being 4
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#if SIZEOF_INT_P != 4
#error size intp is not 4, this not something to worry about
#endif
void blah() {}
])],[c_sizep=4],[c_sizep=0])

dnl automake 1.14 and upwards rewrite the host to have always 64 bit unless i386 as host is passed
dnl this can make building a 32 bit executable a bit tricky, as dosbox relies on the host to select the
dnl dynamic/dynrec core
case "$host" in
  *-*-darwin*)
   AC_MSG_CHECKING([whether Apple user wants to override the build process to produce a 32 bit binary])
   if test x$c_targetcpu = xx86_64 -a x$c_sizep = x4 ; then
     AC_MSG_RESULT(yes)
     AC_DEFINE(C_TARGETCPU,X86)
     c_targetcpu="x86"
   else
     AC_MSG_RESULT(no)
   fi
   ;;
esac

AC_ARG_ENABLE(dynamic-core,AC_HELP_STRING([--disable-dynamic-core],[Disable all dynamic cores]),,enable_dynamic_core=yes)

AH_TEMPLATE(C_DYNAMIC_X86,[Define to 1 to use x86/x64 dynamic cpu core])
AC_ARG_ENABLE(dynamic-x86,AC_HELP_STRING([--disable-dynamic-x86],[Disable x86/x64 dynamic cpu core]),,enable_dynamic_x86=yes)
AC_MSG_CHECKING(whether x86 dynamic cpu core will be enabled)
if test x$enable_dynamic_x86 = xno -o x$enable_dynamic_core = xno; then
   AC_MSG_RESULT(no)
else
  if test x$c_targetcpu = xx86 -o x$c_targetcpu = xx86_64; then
      AC_DEFINE(C_DYNAMIC_X86,1)
      AC_MSG_RESULT(yes)
  else
      AC_MSG_RESULT(no)
  fi
fi

AH_TEMPLATE(C_DYNREC,[Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core])
AC_ARG_ENABLE(dynrec,AC_HELP_STRING([--disable-dynrec],[Disable recompiling cpu core]),,enable_dynrec=yes)
AC_MSG_CHECKING(whether recompiling cpu core will be enabled)
if test x$enable_dynrec = xno -o x$enable_dynamic_core = xno; then
   AC_MSG_RESULT(no)
else
dnl x86 only enable it if dynamic-x86 is disabled.
  if test x$c_targetcpu = xx86 -o x$c_targetcpu = xx86_64; then
    if test x$enable_dynamic_x86 = xno ; then
        AC_DEFINE(C_DYNREC,1)
        AC_MSG_RESULT(yes)
    else
        AC_MSG_RESULT([no, using dynamic-x86])
    fi
  else
    if test x$c_targetcpu = xarm -o x$c_targetcpu = xppc64le -o x$c_targetcpu = xpowerpc ; then
      AC_DEFINE(C_DYNREC,1)
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
    fi
  fi
fi

AH_TEMPLATE(C_FPU,[Define to 1 to enable floating point emulation])
AC_ARG_ENABLE(fpu,AC_HELP_STRING([--disable-fpu],[Disable fpu support]),,enable_fpu=yes)
AC_MSG_CHECKING(whether fpu emulation will be enabled)
if test x$enable_fpu = xyes ; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(C_FPU,1)
else
  AC_MSG_RESULT(no)
fi

AH_TEMPLATE(C_FPU_X86,[Define to 1 to use a x86/x64 assembly fpu core])
AC_ARG_ENABLE(fpu-x86,AC_HELP_STRING([--disable-fpu-x86],[Disable x86 assembly fpu core]),,enable_fpu_x86=yes)
AC_ARG_ENABLE(fpu-x64,AC_HELP_STRING([--disable-fpu-x64],[Disable x64 assembly fpu core]),,enable_fpu_x64=yes)
AC_MSG_CHECKING(whether the x86/x64 assembly fpu core will be enabled)
if test x$enable_fpu_x86 = xno -o x$enable_fpu_x64 = xno; then
   AC_MSG_RESULT(no)
else
  if test x$enable_fpu = xyes; then
    if test x$c_targetcpu = xx86 -o x$c_targetcpu = xx86_64; then
        AC_DEFINE(C_FPU_X86,1)
        AC_MSG_RESULT(yes)
    else
        AC_MSG_RESULT(no)
    fi
  else
      AC_MSG_RESULT(no)
  fi
fi

AH_TEMPLATE(C_UNALIGNED_MEMORY,[Define to 1 to use a unaligned memory access])
AC_ARG_ENABLE(unaligned_memory,AC_HELP_STRING([--disable-unaligned-memory],[Disable unaligned memory access]),,enable_unaligned_memory=yes)
AC_MSG_CHECKING(whether to enable unaligned memory access)
if test x$enable_unaligned_memory = xyes -a x$c_unalignedmemory = xyes; then
  AC_DEFINE(C_UNALIGNED_MEMORY,1)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AH_TEMPLATE(C_SSHOT, [Define to 1 to enable screenshots, requires libpng])
AC_ARG_ENABLE(screenshots,
              AC_HELP_STRING([--disable-screenshots],
                             [Disable screenshots and movie recording]),
              enable_screenshots=no,
              enable_screenshots=yes)
AC_ARG_ENABLE(png_static,
              AC_HELP_STRING([--enable-png-static],
                             [Link libpng statically (not recommended)]),
              enable_png_static=yes,
              enable_png_static=no)
if test x"$enable_screenshots" = x"yes"; then
    if test x"$enable_png_static" = x"yes"; then
      case "$host" in
        *-*-darwin*)
          libpng_LIBS="/usr/local/lib/libpng.a"
          AC_MSG_NOTICE([checking for libpng... using macOS brew library])
          ;;
        *)
          PKG_CHECK_MODULES([libpng], [libpng],
            [libpng_LIBS="-Wl,-Bstatic $libpng_LIBS -Wl,-Bdynamic"])
          ;;
      esac
    else
      PKG_CHECK_MODULES([libpng], [libpng])
    fi
    dnl If our libs are empty, then PNG wasn't found
    if test x"$libpng_LIBS" = x""; then
      AC_MSG_RESULT([screenshots disabled... can't find libpng!])
    else
      AC_DEFINE(C_SSHOT, 1)
      LIBS="$LIBS $libpng_LIBS -lz"
      CXXFLAGS="$CXXFLAGS $libpng_CFLAGS"
      AC_MSG_NOTICE([screenshots enabled... using $libpng_LIBS])
    fi
else
  AC_MSG_RESULT([no])
fi

dnl Ogg Opus handling
dnl -----------------
dnl    We want Opus tracks supported by default, and we also want the user
dnl    to be aware if there's a problem finding the Opus dependencies so
dnl    they can either a) install the opusfile library or b) explicitly
dnl    disable Opus.

dnl    To achieve this, we provide a --disable-opus-cdda configure option
dnl    but by default we look for Opus and fail if we can't find it.
dnl    We only skip supporting Opus if the user explicitly passes the
dnl    --disable-opus-cdda argument.
AC_ARG_ENABLE(opus-cdda,
              AS_HELP_STRING([--disable-opus-cdda],
                             [Disable Opus CD audio track support]))
AS_IF([test "x${enable_opus_cdda}" != "xno"],
      AC_MSG_NOTICE([OPUSFILE dependency can be disabled with --disable-opus-cdda])
      [PKG_CHECK_MODULES([OPUSFILE],
                         [opusfile],
                         [LIBS="$LIBS $OPUSFILE_LIBS" HAVE_OPUS=yes])])

AM_CONDITIONAL(USE_OPUS, test "${HAVE_OPUS}" = "yes")


dnl FluidSynth software MIDI synthesizer
dnl ------------------------------------
dnl
AH_TEMPLATE(C_FLUIDSYNTH, [Define to 1 to enable FluidSynth MIDI synthesizer])
AC_ARG_ENABLE(fluidsynth,
              AS_HELP_STRING([--disable-fluidsynth],
                             [Disable FluidSynth integration]))

if test "${enable_fluidsynth}" != "no" ; then
    AC_MSG_NOTICE([FLUIDSYNTH dependency can be disabled with --disable-fluidsynth])
    PKG_CHECK_MODULES(FLUIDSYNTH,
                      fluidsynth >= 2,
                      [LIBS="$LIBS $FLUIDSYNTH_LIBS $GLIB_LIBS"
                       CPPFLAGS="$CPPFLAGS $FLUIDSYNTH_CFLAGS"
                       USE_FLUIDSYNTH="yes"])
    AC_DEFINE(C_FLUIDSYNTH,1)
else
    AC_DEFINE(C_FLUIDSYNTH,0)
    AC_MSG_WARN([FluidSynth integration disabled])
fi


dnl mt32emu Roland MT-32 audio synthesizer
dnl --------------------------------------
dnl
AH_TEMPLATE(C_MT32EMU, [Define to 1 to enable mt32emu, an MT-32 audio synthesizer])
AC_ARG_ENABLE(mt32,
              AS_HELP_STRING(--enable-mt32, Enable Roland MT-32 audio synthesizer support),
              disable_mt32=no)
if test "${disable_mt32}" != "no" -o "${enable_mt32}" != "yes"; then
    AC_DEFINE(C_MT32EMU, 0)
    AC_MSG_WARN([Roland MT-32 audio synthesizer support disabled. Enable it with --enable-mt32])
else
    LIBS="$LIBS -lmt32emu"
    AC_CHECK_HEADER(mt32emu/mt32emu.h, HAVE_MT32EMU_H=yes, HAVE_MT32EMU_H=no)
    AS_IF(test "${HAVE_MT32EMU_H}" != "yes",
          AC_MSG_ERROR(Munt's mt32emu header <mt32emu/mt32emu.h> not found in include path))
    AC_DEFINE(C_MT32EMU, 1)
    AC_MSG_NOTICE([Roland MT-32 audio synthesizer support enabled])
fi


dnl OpenGL support
dnl --------------
AH_TEMPLATE(C_OPENGL,[Define to 1 to use opengl display output support])
AC_CHECK_LIB(GL, main, have_gl_lib=yes, have_gl_lib=no , )
AC_CHECK_LIB(opengl32, main, have_opengl32_lib=yes,have_opengl32_lib=no , )
AC_CHECK_HEADER(GL/gl.h, have_gl_h=yes , have_gl_h=no , )
AC_ARG_ENABLE(opengl,AC_HELP_STRING([--disable-opengl],[Disable opengl support]),,enable_opengl=yes)
AC_MSG_CHECKING(whether opengl display output will be enabled)
if test x$enable_opengl = xyes; then
case "$host" in
    *-*-darwin*)
       AC_MSG_RESULT(yes)
       LIBS="$LIBS -framework OpenGL -framework CoreFoundation"
       AC_DEFINE(C_OPENGL,1)
       ;;
    *)
       if test x$have_gl_h = xyes -a x$have_gl_lib = xyes ; then
         AC_MSG_RESULT(yes)
         LIBS="$LIBS -lGL"
         AC_DEFINE(C_OPENGL,1)
       elif test x$have_gl_h = xyes -a x$have_opengl32_lib = xyes ; then
         AC_MSG_RESULT(yes)
         LIBS="$LIBS -lopengl32"
         AC_DEFINE(C_OPENGL,1)
       else
         AC_MSG_RESULT(no)
       fi
       ;;
esac
else
   AC_MSG_RESULT(no)
fi

dnl Unit Tests
dnl ----------
dnl 
AC_ARG_ENABLE([tests],
              AS_HELP_STRING([--enable-tests],
                             [Build Unit Tests (requires Google Test framework)]))

if test "${enable_tests}" = "yes" ; then
    PREGTEST_CXXFLAGS="$CXXFLAGS"
    dnl Start with checking if gtest is available as a static lib
    dnl this will work e.g. on Debian or Ubuntu
    AC_MSG_CHECKING(for Google Test framework (static lib))
    CXXFLAGS="-Wl,-Bstatic -lgtest_main -lgtest -Wl,-Bdynamic"
    AC_LINK_IFELSE(
        [AC_LANG_SOURCE([
            #include <gtest/gtest.h>
	    namespace { TEST(Foo, Bar) { ASSERT_TRUE(true); } }
        ])],
        [AC_MSG_RESULT(yes); HAVE_GTEST=yes; HAVE_GTEST_STATIC=yes],
        [AC_MSG_RESULT(no)])
    dnl Also check if maybe it's available as a shared lib
    dnl this will work e.g. on Fedora or Arch
    AC_MSG_CHECKING(for Google Test framework (shared lib))
    CXXFLAGS="-lgtest_main -lgtest"
    AC_LINK_IFELSE(
        [AC_LANG_SOURCE([
            #include <gtest/gtest.h>
	    namespace { TEST(Foo, Bar) { ASSERT_TRUE(true); } }
        ])],
        [AC_MSG_RESULT(yes); HAVE_GTEST=yes],
        [AC_MSG_RESULT(no)])
    dnl Fail loudly if gtest is missing
    AS_IF([test "${HAVE_GTEST}" != "yes"],
        [AC_MSG_ERROR(Google Test framework not found!)])
    dnl Restore C++ compilation flags
    CXXFLAGS="$PREGTEST_CXXFLAGS"
fi

AM_CONDITIONAL(BUILD_TESTS, test "${HAVE_GTEST}" = "yes")
AM_CONDITIONAL(USE_GTEST_STATIC, test "${HAVE_GTEST_STATIC}" = "yes")

dnl Check for mprotect. Needed for 64 bits linux
AH_TEMPLATE(HAVE_MPROTECT,[Define to 1 if you have the mprotect function])
AC_CHECK_HEADER([sys/mman.h], [
AC_CHECK_FUNC([mprotect],[AC_DEFINE(HAVE_MPROTECT,1)])
])

dnl Check for realpath. Used on Linux
AC_CHECK_FUNCS([realpath])

dnl Setpriority
AH_TEMPLATE(HAVE_SETPRIORITY,[Define to 1 if you have setpriority support])
AC_MSG_CHECKING(for setpriority support)
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <sys/resource.h>
int main(int argc,char * argv[]) {
	return setpriority (PRIO_PROCESS, 0,PRIO_MIN+PRIO_MAX);
};
])],AC_MSG_RESULT(yes);AC_DEFINE(HAVE_SETPRIORITY,1),AC_MSG_RESULT(no))


dnl Some target detection and actions for them
case "$host" in
    *-*-cygwin* | *-*-mingw* | *-*-msys*)
       LIBS="$LIBS -lwinmm"
       AC_DEFINE(WIN32, 1, [Compiling on Windows])
       AC_DEFINE(NOMINMAX, 1, [Prevent <windows.h> from clobbering std::min and std::max])
       AC_DEFINE(C_DIRECTSERIAL, 1, [ Define to 1 if you want serial passthrough support (Win32 and Posix).])
       if test x$have_sdl_net_lib = xyes -a x$have_sdl_net_h = xyes ; then
         LIBS="$LIBS -lws2_32"
         if test x$enable_sdl_static = xyes ; then
           LIBS="$LIBS -liphlpapi"
         fi
       fi
       ;;
    *-*-darwin*)
       dnl We have a problem here: both Mac OS X and Darwin report
       dnl the same signature "powerpc-apple-darwin*" - so we have
       dnl to do more to distinguish them.
       dnl For now I am lazy and do not add proper detection code.
       AC_DEFINE(MACOSX, 1, [Compiling on Mac OS X])
       AC_DEFINE(C_DIRECTSERIAL, 1, [ Define to 1 if you want serial passthrough support (Win32 and Posix).])
       ;;
    *-*-linux*)
       AC_DEFINE(LINUX, 1, [Compiling on GNU/Linux])
       AC_DEFINE(C_DIRECTSERIAL, 1, [ Define to 1 if you want serial passthrough support (Win32 and Posix).])
       ;;
    *-*-freebsd* | *-*-dragonfly* | *-*-netbsd* | *-*-openbsd*)
       dnl Disabled directserial for now. It doesn't do anything without
       dnl specifying an extra ifdef in directserial_posix.*
       dnl directserial detection should be rewritten to test for the needed
       dnl functions and headers. I currently do not know
       dnl which ones are needed for BSD
       AC_DEFINE(BSD, 1, [Compiling on BSD])
       AC_DEFINE(C_DIRECTSERIAL, 1, [ Define to 1 if you want serial passthrough support (Win32 and Posix).])
       ;;
esac

dnl Some stuff for the icon.
case "$host" in
    *-*-cygwin* | *-*-mingw* | *-*-msys*)
       dnl Some stuff for the ico
       AC_CHECK_TOOL(WINDRES, windres, :)
    ;;
    *)
       WINDRES=":"
    ;;
esac
       AM_CONDITIONAL(HAVE_WINDRES, test "x$WINDRES" != "x:")
       AC_SUBST(WINDRES)

AH_TOP([
/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2021  The DOSBox Staging Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
])

AC_CONFIG_FILES([
Makefile
docs/Makefile
include/Makefile
src/Makefile
src/cpu/Makefile
src/cpu/core_full/Makefile
src/cpu/core_normal/Makefile
src/cpu/core_dyn_x86/Makefile
src/cpu/core_dynrec/Makefile
src/debug/Makefile
src/dos/Makefile
src/fpu/Makefile
src/gui/Makefile
src/hardware/Makefile
src/hardware/mame/Makefile
src/hardware/serialport/Makefile
src/ints/Makefile
src/libs/Makefile
src/libs/zmbv/Makefile
src/libs/gui_tk/Makefile
src/libs/decoders/Makefile
src/libs/nuked/Makefile
src/libs/ppscale/Makefile
src/midi/Makefile
src/misc/Makefile
src/shell/Makefile
src/platform/Makefile
src/platform/visualc/Makefile
tests/Makefile
])
AC_OUTPUT
