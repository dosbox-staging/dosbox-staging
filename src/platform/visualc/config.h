#define INLINE __forceinline

#define VERSION "0.58"

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 1

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

/* Enable some big compile-time increasing inlines */
#define C_EXTRAINLINE 0

/* Enable the FPU module, still only for beta testing */
#define C_FPU 0

/* Maximum memory address range in megabytes */
#define C_MEM_MAX_SIZE 12

#define GCC_ATTRIBUTE(x) /* attribute not supported */
