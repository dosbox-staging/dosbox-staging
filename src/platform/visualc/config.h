/* Project name, lower-case and without spaces */
#define CANONICAL_PROJECT_NAME "dosbox-staging"

// Emulator Semantic Version (MAJOR.MINOR.PATCH), incremented as follows:
//  - MAJOR version when you make incompatible API changes
//  - MINOR version when you add functionality in a backwards compatible manner
//  - PATCH version when you make backwards compatible bug fixes
// Additional labels for pre-release and build metadata are available as extensions to the MAJOR.MINOR.PATCH format.
// Ref: https://semver.org/

#define VERSION "0.81.0-alpha"

/* Copyright string - to be returned by virtual drivers, etc. */
#define DOSBOX_COPYRIGHT "(C) The DOSBox Staging Team"

/* This macro is going to be overriden via CI */
#define DOSBOX_DETAILED_VERSION "git"

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 0

/* Define to 1 to enable rendered screenshots, requires SDL_image */
#define C_SDL_IMAGE 1

/* Define to 1 to enable surface screenshots, requires libpng */
#define C_SSHOT 1

/* Define to 1 to use opengl display output support */
#define C_OPENGL 1

/* Define to 1 to enable internal modem support, requires SDL_net */
#define C_MODEM 1

/* Define to 1 to enable IPX networking support, requires SDL_net */
#define C_IPX 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

/* The type of cpu this host has */
#ifdef _M_X64
#define C_TARGETCPU X86_64
#else // _M_IX86
#define C_TARGETCPU X86
#endif
//#define C_TARGETCPU X86_64

/* Define to 1 to use x86 dynamic cpu core */
#define C_DYNAMIC_X86 1

/* Define to 1 if the target platform needs per-page dynamic core write or
 * execute (W^X) tagging */
#define C_PER_PAGE_W_OR_X 1

/* Define to 1 to use recompiling cpu core. Can not be used together with the
 * dynamic-x86 core */
#define C_DYNREC 0

/* Enable memory function inlining in */
#define C_CORE_INLINE 1

/* Define to 1 to enable FluidSynth MIDI synthesizer */
#define C_FLUIDSYNTH 1

// Define to 1 to enable MT-32 emulator
#define C_MT32EMU 1

/* Enable the FPU module, still only for beta testing */
#define C_FPU 1

/* Define to 1 to use a x86 assembly fpu core */
#ifdef _M_X64
//No support for inline asm with visual studio in x64 bit mode.
//This means that non-dynamic cores can't use the better fpu emulation.
#define C_FPU_X86 0
#else // _M_IX86
#define C_FPU_X86 1
#endif

/* Define to 1 to use a unaligned memory access */
#define C_UNALIGNED_MEMORY 1

/* Prevent <windows.h> from clobbering std::min and std::max */
#define NOMINMAX 1

/* Define to 1 if you want serial passthrough support. */
#define C_DIRECTSERIAL 1

// Enables mathematical constants under Visual Studio, such as M_PI
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/math-constants
#define _USE_MATH_DEFINES

// Modern MSVC provides POSIX-like routines, so prefer that over built-in
#define HAVE_STRNLEN

// MSVC issues pedantic warnings on POSIX functions; for portability we don't
// want to deal with these warnings, as the only way to avoid them is using
// Microsoft-specific names and functions instead of POSIX conformant ones.
// https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4996?view=vs-2019#posix-function-names
#define _CRT_NONSTDC_NO_WARNINGS

// MSVC issues warnings on what it considers "unsafe" C-calls for
// which "safer" (_s-equivalent) calls.
//
// To date, no effort has been made to transition toward these _s
// calls, in part because no static analyzer (Coverity, PVS Studio,
// or Clang) has flagged the project's use of the existing non-_s
// calls as having security implications.
//
// Likewise, if there is going to be work put into altering the use
// of these calls, it will involve transitioning toward modern C++
// constructs as mentioned in #1314 (Ref:
// https://github.com/dosbox-staging/dosbox-staging/issues/1314)
//
// Because the recommendations in these warnings will not be acted
// upon given the planned direction of the the project, they
// therefore provide no value and are being silenced.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// On *nix systems, this variable holds a (potentially) custom path
// configured during compile-time by setting Meson's "--datadir"
// On Windows, this path is not customizeable, so it's left blank here.
//
#define CUSTOM_DATADIR ""

/* Loguru-related defines
 */

// Prevent loguru from parsing command-line arguments with
// with the hosts's locale-applied, because this can foul up
// ncurses. (DOSBox also doesn't have foreign-language arguments).
//
#define LOGURU_USE_LOCALE 0
