// SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TEST_FIXTURE_H
#define DOSBOX_TEST_FIXTURE_H

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "audio/mixer.h"
#include "config/config.h"
#include "cpu/cpu.h"
#include "dos/dos.h"
#include "dosbox.h"
#include "hardware/serialport/serialport.h"
#include "ints/bios.h"
#include "shell/autoexec.h"

class DOSBoxTestFixture : public ::testing::Test {
public:
	DOSBoxTestFixture()
	        : command_line("dosbox_tests",
	                       "--noprimaryconf "
	                       "--nolocalconf "
	                       "--conf tests/files/dosbox-staging-tests.conf")
	{
		// Don't load the local & primary configs if they exist as they're
		// outside of our control and could skew the tests.
		control = std::make_unique<Config>(&command_line);
	}

	void SetUp() override
	{
		// Create DOSBox Staging's config directory, which is a
		// pre-requisite that's asserted during the Init process.
		//
		init_config_dir();

		// Only initialiasing the minimum number of modules required for
		// the tests.
		//
		// This results in a 4-fold reduction in test execution times
		// compared to using `DOSBOX_InitModules()` (e.g. DOS_FilesTest
		// runs in 3 seconds instead of 13).
		//
		// Config sections must be registered before parsing the config
		// files below, otherwise settings targeting not-yet-registered
		// sections (e.g. "nosound" in the "mixer" section) are silently
		// dropped.
		//
		DOSBOX_InitModuleConfigsAndMessages();

		const auto config_path = get_config_dir();
		control->ParseConfigFiles(config_path);

		DOSBOX_Init();
		CPU_Init();
		MIXER_Init();
		BIOS_Init();
		SERIAL_Init();
		DOS_Init();
		AUTOEXEC_Init();
	}

	void TearDown() override
	{
		DOS_Destroy();
		SERIAL_Destroy();
		BIOS_Destroy();
		CPU_Destroy();
		MIXER_Destroy();
		DOSBOX_Destroy();

		control = {};
	}

private:
	CommandLine command_line;
};

#endif
