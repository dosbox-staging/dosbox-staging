#define INLINE __forceinline

#define VERSION "0.61"


/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 1

/* Define to 1 to enable screenshots, requires libpng */
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
#define C_HOSTCPU X86

/* Define to 1 to use x86 dynamic cpu core */
#define C_DYNAMIC_X86 1

/* Enable memory function inlining in */
#define C_CORE_INLINE 0

/* Enable the FPU module, still only for beta testing */
#define C_FPU 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

/* Define to 1 if you have the <ddraw.h> header file. */
#define HAVE_DDRAW_H 1

/* Define to 1 if you want serial passthrough support (Win32 only). */
#define C_DIRECTSERIAL 1

#define GCC_ATTRIBUTE(x) /* attribute not supported */

typedef         double		Real64;
/* The internal types */
typedef  unsigned char		Bit8u;
typedef    signed char		Bit8s;
typedef unsigned short		Bit16u;
typedef   signed short		Bit16s;
typedef  unsigned long		Bit32u;
typedef    signed long		Bit32s;
typedef unsigned __int64	Bit64u;
typedef   signed __int64	Bit64s;
typedef unsigned int		Bitu;
typedef signed int			Bits;

