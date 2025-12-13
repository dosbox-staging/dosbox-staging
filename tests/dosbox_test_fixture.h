// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TEST_FIXTURE_H
#define DOSBOX_TEST_FIXTURE_H

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "config/config.h"
#include "cpu/cpu.h"
#include "dos/dos.h"
#include "dosbox.h"
#include "gui/common.h"
#include "hardware/dma.h"
#include "hardware/input/joystick.h"
#include "hardware/input/keyboard.h"
#include "hardware/input/mouse.h"
#include "hardware/pci_bus.h"
#include "hardware/serialport/serialport.h"
#include "ints/bios.h"
#include "ints/int10.h"
#include "misc/video.h"
#include "shell/autoexec.h"

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
		init_config_dir();
		const auto config_path = get_config_dir();
		control->ParseConfigFiles(config_path);

		// Only initialiasing the minimum number of modules for the
		// tests.
		//
		// We can't just simply call `DOSBOX_InitModules()` as that
		// would initialise the GUI and the rendering backend too.
		// That's especially problematic because 1) we don't want to be
		// opening windows during test runs, 2) we'd need to initialise
		// a lot more to make things work with crashes, 3) it would be
		// extremely slow, and 4) it wouldn't work on CI anyway.
		//
		DOSBOX_InitModuleConfigsAndMessages();

		DOSBOX_Init();

		CPU_Init();
		DMA_Init();
		KEYBOARD_Init();
		PCI_Init();

		BIOS_Init();
		INT10_Init();
		MOUSE_Init();
		JOYSTICK_Init();

		SERIAL_Init();
		DOS_Init();

		AUTOEXEC_Init();
	}

	void TearDown() override
	{
		DOS_Destroy();

		SERIAL_Destroy();

		JOYSTICK_Destroy();
		BIOS_Destroy();

		PCI_Destroy();
		DMA_Destroy();
		CPU_Destroy();
		DOSBOX_Destroy();

		control = {};
	}

private:
	const char* arg_c_str;
	const char* argv[1];

	CommandLine command_line;
	ConfigPtr config;
};

#endif
