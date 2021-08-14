/* String appended to the .conf file. */
#define CONF_SUFFIX "-staging-git"

/* Version number of package */
#define VERSION "0.78.0"

/* This macro is going to be overriden via CI */
#define DOSBOX_DETAILED_VERSION "git"

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 0

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Define to 1 to use opengl display output support */
#define C_OPENGL 1

/* Define to 1 to enable internal modem support, requires SDL_net */
#define C_MODEM 1

/* Define to 1 to enable NE2000 ethernet passthrough, requires libpcap */
#define C_NE2000 0

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

/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
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

// MSVC issues pedantic warnings on POSIX functions; for portability we don't
// want to deal with these warnings, as the only way to avoid them is using
// Microsoft-specific names and functions instead of POSIX conformant ones.
// https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-level-3-c4996?view=vs-2019#posix-function-names
#define _CRT_NONSTDC_NO_WARNINGS
