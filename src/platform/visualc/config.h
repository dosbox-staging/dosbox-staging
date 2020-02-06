#define VERSION "SVN"

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 0

/* Define to 1 to enable output=ddraw */
#define C_DDRAW 1 

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1
/* Define to 1 to enable movie recording, requires zlib built without Z_SOLO */
#define C_SRECORD 1

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

/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
#define C_DYNREC 0

/* Enable memory function inlining in */
#define C_CORE_INLINE 1

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

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

/* Define to 1 if you want serial passthrough support. */
#define C_DIRECTSERIAL 1

#define GCC_ATTRIBUTE(x) /* attribute not supported */
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)

#define INLINE __forceinline
#define DB_FASTCALL __fastcall

#if defined(_MSC_VER) && (_MSC_VER >= 1400) 
#pragma warning(disable : 4996) 
#endif

typedef double            Real64;
/* The internal types */
typedef unsigned char     Bit8u;
typedef signed char       Bit8s;
typedef unsigned short    Bit16u;
typedef signed short      Bit16s;
typedef unsigned int      Bit32u;
typedef signed int        Bit32s;
typedef unsigned __int64  Bit64u;
typedef signed __int64    Bit64s;
#define sBit32t
#define sBit64t "I64"
#define sBit32fs(a) sBit32t #a
#define sBit64fs(a) sBit64t #a
#ifdef _M_X64
typedef Bit64u            Bitu;
typedef Bit64s            Bits;
#define sBitfs sBit64fs
#else // _M_IX86
typedef Bit32u            Bitu;
typedef Bit32s            Bits;
#define sBitfs sBit32fs
#endif

