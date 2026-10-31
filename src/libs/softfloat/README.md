# Berkeley SoftFloat 3e subset

Upstream: <https://www.jhauser.us/arithmetic/SoftFloat.html>
Author: John R. Hauser. Release 3e (2018-01-20).
License: BSD-3-Clause; see COPYING.txt and notices in each imported file.

Imported from the local SoftFloat-3e checkout. The selected translation units
provide extended80 arithmetic, comparisons, conversions, and binary128 guard
arithmetic for the portable x87 emulator. The `8086` specialization supplies
x87-compatible indefinite values and NaN propagation. The original rounding
algorithm in `s_roundPackToExtF80.c` is renamed and
wrapped to provide x87 unmasked exponent-wrap responses, controlled by
`softfloat_extF80_exceptionMask`. Other imported arithmetic sources are unchanged;
`platform.h` and `CMakeLists.txt` are integration files.
Portable integer primitives are used, including on compilers without __int128.
The C API is exposed inside a C++ namespace by src/fpu/float80.h to avoid
collisions with ARM NEON's float32_t/float64_t typedefs.
