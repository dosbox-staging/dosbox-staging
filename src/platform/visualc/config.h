#define INLINE __forceinline

#define VERSION "0.60"

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 1

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Define to 1 to enable internal modem support, requires SDL_net */
#define C_MODEM 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 1

/* Enable memory function inlining in */
#define C_CORE_INLINE 0

/* Enable the FPU module, still only for beta testing */
#define C_FPU 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

#define GCC_ATTRIBUTE(x) /* attribute not supported */
