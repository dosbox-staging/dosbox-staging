// This file exists only for MSVC builds.
// It's needed because vcpkg does not set opusfile's subdirectory in its includes list
// for projects that do not use cmake.
#include <opus/opusfile.h>
