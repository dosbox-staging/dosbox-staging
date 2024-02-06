// This file exists only for MSVC builds.
// It's needed because vcpkg does not set SDL subdirectory in includes list
// for projects that do not use cmake.
#include <SDL2/SDL.h>

#include "compiler.h"

#ifndef SDL_CONSTEXPR_VERSION
#define SDL_CONSTEXPR_VERSION \
	XSTR(SDL_MAJOR_VERSION) "." XSTR(SDL_MINOR_VERSION) "." XSTR(SDL_PATCHLEVEL)
#endif

static_assert(SDL_VERSION_ATLEAST(2, 0, 5),
              "SDL >= 2.0.5 required (using " SDL_CONSTEXPR_VERSION ")");
