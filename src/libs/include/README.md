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

## Fluidsynth

Headers are sourced from the automated GitHub archive from the 2.2.3 release. 
The contents of the `include` directory have been copied into this directory.

The original `fluidsynth.cmake` file has been renamed to `fluidsynth.h` and 
the conditional logic for `BUILD_SHARED_LIBS` was removed and replaced with a 
bare `#define FLUIDSYNTH_API`. Additionally, `#include "fluidsynth/version.h"` 
has been commented out.

## Slirp

Headers are sourced from the dist archive for the 4.8.0 release. 

The `libslirp.h` file was copied into the the `slirp` directory, and  
`#include "libslirp-version.h"` was commented out.
