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
#include "gui/common.h"
#include "gui/mapper.h"
#include "hardware/input/keyboard.h"
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
		GFX_AddConfigSection();
		DOSBOX_InitModuleConfigsAndMessages();

		const auto config_path = get_config_dir();
		control->ParseConfigFiles(config_path);

		DOSBOX_Init();
		CPU_Init();
		KEYBOARD_Init();
		MIXER_Init();
		BIOS_Init();
		SERIAL_Init();
		DOS_Init();
		AUTOEXEC_Init();

		// Creates the mapper's key events, so keys can be pressed via
		// the mapper (e.g., with `MAPPER_AutoType()`)
		MAPPER_BindKeys(get_sdl_section());

		SetUpFirstShellProcess();
	}

	void TearDown() override
	{
		DOS_Destroy();
		SERIAL_Destroy();
		BIOS_Destroy();
		MIXER_Destroy();
		KEYBOARD_Destroy();
		CPU_Destroy();
		DOSBOX_Destroy();

		MAPPER_Destroy();

		control = {};
	}

private:
	// Makes the first shell the current process and opens its standard
	// handles on the console, like the shell does at startup. Without
	// this, there's no standard input and output to read and write.
	//
	static void SetUpFirstShellProcess()
	{
		DOS_PSP psp(DOS_FIRST_SHELL);
		psp.MakeNew(0);
		dos.psp(DOS_FIRST_SHELL);

		uint16_t handle = 0;
		ASSERT_TRUE(DOS_OpenFile("CON", OPEN_READWRITE, &handle));
		ASSERT_EQ(handle, STDIN);

		ASSERT_TRUE(DOS_OpenFile("CON", OPEN_READWRITE, &handle));
		ASSERT_EQ(handle, STDOUT);

		ASSERT_TRUE(DOS_ForceDuplicateEntry(STDOUT, STDERR));
	}

	CommandLine command_line;
};

#endif
