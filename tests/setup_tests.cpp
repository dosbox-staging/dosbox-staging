/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include "setup.h"

#include <gtest/gtest.h>

#include <string>

namespace {

const parse_environ_result_t expected_empty{};

TEST(ParseEnv, NoRelevantEnvVariables)
{
	const char *test_environ[] = {"FOO=foo", "BAR=bar", "BAZ=baz", nullptr};

	EXPECT_EQ(expected_empty, parse_environ(test_environ));
}

TEST(ParseEnv, SingleValueInEnv)
{
	const char *test_environ[] = {
		"SOME_NAME=value",
		"DOSBOX_SECTIONNAME_PROPNAME=value",
		"OTHER_NAME=other_value",
		nullptr
	};
	parse_environ_result_t expected{{"SECTIONNAME", "PROPNAME=value"}};

	EXPECT_EQ(expected, parse_environ(test_environ));
}

TEST(ParseEnv, PropertyOrValueCanHaveUnderscores)
{
	const char *test_environ[] = {
		"DOSBOX_sec_prop_name=value",
		"DOSBOX_sec_prop=value_name",
		nullptr
	};
	parse_environ_result_t expected{{"sec", "prop_name=value"},
	                                {"sec", "prop=value_name"}};

	EXPECT_EQ(expected, parse_environ(test_environ));
}

TEST(ParseEnv, SelectVarsBasedOnPrefix)
{
	const char *test_environ[] = {
		"DOSBOX_sec_prop1=value1",
		"dosbox_sec_prop2=value2",
		"DOSBox_sec_prop3=value3",
		"dOsBoX_sec_prop4=value4",
		"non_dosbox_sec_prop=val",
		nullptr
	};

	EXPECT_EQ(4, parse_environ(test_environ).size());
}

TEST(ParseEnv, FilterOutEmptySection)
{
	const char *test_environ[] = {
		"DOSBOX=value",
		"DOSBOX_=value",
		"DOSBOX__prop=value",
		nullptr
	};

	EXPECT_EQ(expected_empty, parse_environ(test_environ));
}

TEST(ParseEnv, FilterOutEmptyPropname)
{
	const char *test_environ[] = {
		"DOSBOX_sec=value",
		"DOSBOX_sec_=value",
		nullptr
	};

	EXPECT_EQ(expected_empty, parse_environ(test_environ));
}

} // namespace
