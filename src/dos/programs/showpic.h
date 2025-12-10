// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_SHOWPIC_H
#define DOSBOX_PROGRAM_SHOWPIC_H

#include <optional>

#include "dos/programs.h"
#include "ints/int10.h"
#include "misc/help_util.h"

#include <SDL.h>

class SHOWPIC final : public Program {
public:
	SHOWPIC()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "SHOWPIC"};
	}

	void Run(void) override;

private:
	static void AddMessages();

	std::optional<VideoModeBlock> GetClosestVideoMode(const int width,
	                                                  const int height,
	                                                  const int num_colors) const;

	void SetPalette(const SDL_Palette& palette) const;

	void DisplayImage(const SDL_Surface& surface, const int screen_width,
	                  const int screen_height) const;

	void ClearScreen(const int screen_width, const int screen_height) const;

	void WaitForTicks(const int num_ticks) const;
	void WaitForKeypress() const;
};

#endif // DOSBOX_PROGRAM_SHOPIC_H
