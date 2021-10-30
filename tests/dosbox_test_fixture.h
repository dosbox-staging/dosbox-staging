#ifndef DOSBOX_TEST_FIXTURE_H
#define DOSBOX_TEST_FIXTURE_H

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "control.h"

class DOSBoxTestFixture : public ::testing::Test {
public:
	DOSBoxTestFixture()
	        : arg_c_str("-conf tests/files/dosbox-staging-tests.conf\0"),
	          argv{arg_c_str},
	          com_line(1, argv),
	          config(Config(&com_line))
	{
		control = &config;
	}

	void SetUp() override
	{
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
	}

private:
	char const *arg_c_str;
	const char *argv[1];
	CommandLine com_line;
	Config config;
	Section *_sec;
	// Only init these sections for our tests
	std::vector<std::string> sections{"dosbox", "cpu",      "mixer",
	                                  "midi",   "sblaster", "speaker",
	                                  "serial", "dos",      "autoexec"};
};

#endif
