// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TEST_FIXTURE_H
#define DOSBOX_TEST_FIXTURE_H

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "config/config.h"
#include "misc/video.h"

class DOSBoxTestFixture : public ::testing::Test {
public:
	DOSBoxTestFixture()
	        : arg_c_str("-conf tests/files/dosbox-staging-tests.conf\0"),
	          argv{arg_c_str},
	          command_line(1, argv)
	{
		control = std::make_unique<Config>(&command_line);
	}

	void SetUp() override
	{
		// Create DOSBox Staging's config directory, which is a
		// pre-requisite that's asserted during the Init process.
		//
		InitConfigDir();
		const auto config_path = GetConfigDir();
		control->ParseConfigFiles(config_path);

		// This will register all the init functions, but won't run them
		DOSBOX_InitAllModuleConfigsAndMessages();

		for (const auto& section_name : sections) {
			auto section = control->GetSection(section_name);
			// TODO
//			section->ExecuteInit();
		}
	}

	void TearDown() override
	{
		std::vector<std::string>::reverse_iterator r = sections.rbegin();

		for (const auto& section_name : sections) {
			// TODO
//			control->GetSection(section_name)->ExecuteDestroy();
		}
	}

private:
	const char* arg_c_str;
	const char* argv[1];

	CommandLine command_line;
	ConfigPtr config;

	// Only init these sections for our tests
	std::vector<std::string> sections{"dosbox", "cpu", "serial", "dos", "autoexec"};
};

#endif
