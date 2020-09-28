// This file exists only for MSVC builds.
// It's needed because vcpkg does not set SDL subdirectory in includes list
// for projects that do not use cmake.
#include <SDL2/SDL.h>

static_assert(SDL_VERSION_ATLEAST(2, 0, 2), "SDL >= 2.0.2 required");
