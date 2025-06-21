// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

// When compiling for Windows, SDL converts function 'main' to 'WinMain' and
// performs some additional initialization.
//
#include <SDL.h>

int main(int argc, char *argv[])
{
	return sdl_main(argc, argv);
}
