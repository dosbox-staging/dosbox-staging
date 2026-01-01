// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_RWOPS_H
#define DOSBOX_DOS_RWOPS_H

#include <cstdint>

#include <SDL3/SDL_iostream.h>

// SDL IOStream implementation for DOS files. With this, we can use
// libraries like SDL2_image to read DOS files from the host filesystem or
// mounted disk images.
//
SDL_IOStream* create_sdl_rwops_for_dos_file(const uint16_t dos_file_handle);

#endif // DOSBOX_DOS_RWOPS_H
