/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <SDL2/SDL.h>

static void maybe_print_header(const int num_keys_logged)
{
	constexpr auto log_every_n_keys = 15;
	const bool should_print = (num_keys_logged % log_every_n_keys) == 0;

	if (should_print) {
		SDL_Log(" ");
		SDL_Log("SCANCODE      val hex | SDLK                 val       hex |  KMOD   hex | remapped?");
		SDL_Log("~~~~~~~~~~~~~ ~~~ ~~~ | ~~~~~~~~~~~~~ ~~~~~~~~~~ ~~~~~~~~~ | ~~~~~ ~~~~~ | ~~~~~~~~~");
	}

}

static bool log_key(const SDL_Keysym& ks)
{
	const auto is_remapped = (ks.scancode != SDL_GetScancodeFromKey(ks.sym));
	const auto remapped_str = is_remapped ? "yes" : "no";
	
	SDL_Log("%-13s %3d %2xh | "  // SCANCODE
	        "%-13s %10d %8xh | " // SDLK
	        "%5d %4xh |"         // KMOD
	        "%3s",               // is remapped

	        SDL_GetScancodeName(ks.scancode),
	        ks.scancode,
	        ks.scancode,

	        SDL_GetKeyName(ks.sym),
	        ks.sym,
	        ks.sym,

	        ks.mod,
	        ks.mod,

	        remapped_str);

	const auto should_keep_going = ks.sym != SDLK_ESCAPE;
	return should_keep_going;
}

static bool log_key()
{
	static auto num_keys_logged = 0;
	static SDL_Event event = {};
	SDL_WaitEvent(&event);

	switch (event.type) {
	case SDL_QUIT: return false;

	case SDL_KEYDOWN:
		maybe_print_header(num_keys_logged++); 
		return log_key(event.key.keysym);
	}
	return true;
}

int main(const int, const char**)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("Failed initializing video: %s\n", SDL_GetError());
		return 1;
	}

	auto window = SDL_CreateWindow("SDL SCANCODE, SDLK, and KMOD grabber",
	                               SDL_WINDOWPOS_UNDEFINED,
	                               SDL_WINDOWPOS_UNDEFINED,
	                               400,
	                               400,
	                               SDL_WINDOW_INPUT_GRABBED |
	                                       SDL_WINDOW_ALLOW_HIGHDPI);
	if (!window) {
		SDL_Log("Failed creating the window: %s\n", SDL_GetError());
		SDL_Quit();

		return 1;
	}
	auto renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer) {
		SDL_Log("Failed creating the renderer: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		window = nullptr;
		SDL_Quit();

		return 1;
	}

	SDL_RenderPresent(renderer);

	SDL_Log("DOSBox Staging SDL key logger");
	SDL_Log("Quit by tapping ESC or close the GUI window");

	while (log_key())
		; // keep going

	SDL_DestroyRenderer(renderer);
	renderer = nullptr;

	SDL_DestroyWindow(window);
	window = nullptr;

	SDL_Quit();
	return 0;
}
