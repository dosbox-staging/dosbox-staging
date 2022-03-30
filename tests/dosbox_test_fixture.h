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
		CROSS_DetermineConfigPaths();
		const auto config_path = CROSS_GetPlatformConfigDir();
		SETUP_ParseConfigFiles(config_path);

		Section *_sec;
		// This will register all the init functions, but won't run them
		DOSBOX_Init();

		for (auto section_name : sections) {
			_sec = control->GetSection(section_name);
			// NOTE: Some of the sections will return null pointers,
			// if you add a section below, make sure to test for
			// nullptr before executing early init.
			_sec->ExecuteEarlyInit();
		}

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
	config_ptr_t config;
	// Only init these sections for our tests
	std::vector<std::string> sections{"dosbox", "cpu",      "mixer",
	                                  "midi",   "sblaster", "speaker",
	                                  "serial", "dos",      "autoexec"};
};

#endif
