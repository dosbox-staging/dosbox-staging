/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_TEST_FIXTURE_H
#define DOSBOX_TEST_FIXTURE_H

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#define SDL_MAIN_HANDLED

#include "control.h"
#include "video.h"

class DOSBoxTestFixture : public ::testing::Test {
public:
	DOSBoxTestFixture()
	        : arg_c_str("-conf tests/files/dosbox-staging-tests.conf\0"),
	          argv{arg_c_str},
	          com_line(1, argv)
	{
		control = std::make_unique<Config>(&com_line);
	}

	void SetUp() override
	{
		// Create DOSBox Staging's config directory, which is a
		// pre-requisite that's asserted during the Init process.
		//
		InitConfigDir();
		const auto config_path = GetConfigDir();
		control->ParseConfigFiles(config_path);

		Section *_sec;
		// This will register all the init functions, but won't run them
		DOSBOX_InitAllModuleConfigsAndMessages();

		for (auto section_name : sections) {
			_sec = control->GetSection(section_name);
			_sec->ExecuteInit();
		}
	}

	void TearDown() override
	{
		std::vector<std::string>::reverse_iterator r = sections.rbegin();
		for (; r != sections.rend(); ++r)
			control->GetSection(*r)->ExecuteDestroy();
		GFX_RequestExit(true);
	}

private:
	char const *arg_c_str;
	const char *argv[1];
	CommandLine com_line;
	ConfigPtr config;
	// Only init these sections for our tests
	std::vector<std::string> sections{"dosbox", "cpu",      "mixer",
	                                  "midi",   "sblaster", "speaker",
	                                  "serial", "dos",      "autoexec"};
};

#endif
