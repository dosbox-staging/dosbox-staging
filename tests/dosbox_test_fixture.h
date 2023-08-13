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
	          command_line_instance(1, (char**)argv),
	          config_instance(&command_line_instance)
	{
		control = &config_instance;
	}

	void SetUp() override
	{
		// Create DOSBox Staging's config directory, which is a
		// pre-requisite that's asserted during the Init process.
		//
		InitConfigDir();
		const auto config_path = GetConfigDir();
		ParseConfigFiles(config_path);

		// This will register all the init functions, but won't run them
		DOSBOX_Init();

		for (const auto section_name : section_names) {
			auto sec = config_instance.GetSection(section_name);
			sec->ConfigureModules(ModuleLifecycle::Create);
			sec->ExecuteInit();
		}
	}

	void TearDown() override
	{
		auto section_name = section_names.rbegin();
		while (section_name != section_names.rend()) {
			auto section = config_instance.GetSection(*section_name);
			assert(section);
			section->ConfigureModules(ModuleLifecycle::Destroy);
			section->ExecuteDestroy();

			++section_name;
		}
		GFX_RequestExit(true);
	}

private:
	const char* arg_c_str = {};
	const char* argv[1]   = {};
	CommandLine command_line_instance;
	Config config_instance = {};
	// Only init these section_names for our tests
	std::vector<const char*> section_names{"dosbox",
	                                       "cpu",
	                                       "mixer",
	                                       "midi",
	                                       "sblaster",
	                                       "speaker",
	                                       "serial",
	                                       "dos",
	                                       "autoexec"};
};

#endif
