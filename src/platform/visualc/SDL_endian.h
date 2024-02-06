// This file exists only for MSVC builds.
// It's needed because vcpkg does not set SDL subdirectory in includes list
// for projects that do not use cmake.
#include <SDL2/SDL_endian.h>
