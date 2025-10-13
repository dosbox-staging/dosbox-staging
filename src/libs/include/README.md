# Runtime loaded library headers

This directory contains the headers for the libraries loaded at runtime using 
`dlopen` or `LoadLibrary`. The directory has been added to the include paths, 
and headers can be included as they normally would.

The headers for the following libraries are included:

- Fluidsynth
- Slirp

Ideally, headers should come from the minimum required version of the library, 
and only updated when new features of the API are used.

Notes about each library are provided in the next sections.

## Slirp

Headers are sourced from the dist archive for the 4.8.0 release. 

The `libslirp.h` file was copied into the the `slirp` directory, and  
`#include "libslirp-version.h"` was commented out.
