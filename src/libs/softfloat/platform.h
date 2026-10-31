// DOSBox Staging portable SoftFloat configuration.
#pragma once
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#else
#define LITTLEENDIAN 1
#endif
#define INLINE static inline
