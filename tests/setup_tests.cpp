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

#include "support.h"
#include <string>

namespace {

const parse_environ_result_t expected_empty{};

TEST(ParseEnv, NoRelevantEnvVariables)
{
	const char* test_environ[] = {"FOO=foo", "BAR=bar", "BAZ=baz", nullptr};

	EXPECT_EQ(expected_empty, parse_environ(test_environ));
}

TEST(ParseEnv, SingleValueInEnv)
{
	const char* test_environ[] = {"SOME_NAME=value",
	                              "DOSBOX_SECTIONNAME_PROPNAME=value",
	                              "OTHER_NAME=other_value",
	                              nullptr};
	parse_environ_result_t expected{{"SECTIONNAME", "PROPNAME=value"}};

	EXPECT_EQ(expected, parse_environ(test_environ));
}

TEST(ParseEnv, PropertyOrValueCanHaveUnderscores)
{
	const char* test_environ[] = {"DOSBOX_sec_prop_name=value",
	                              "DOSBOX_sec_prop=value_name",
	                              nullptr};
	parse_environ_result_t expected{{"sec", "prop_name=value"},
	                                {"sec", "prop=value_name"}};

	EXPECT_EQ(expected, parse_environ(test_environ));
}

TEST(ParseEnv, SelectVarsBasedOnPrefix)
{
	const char* test_environ[] = {"DOSBOX_sec_prop1=value1",
	                              "dosbox_sec_prop2=value2",
	                              "DOSBox_sec_prop3=value3",
	                              "dOsBoX_sec_prop4=value4",
	                              "non_dosbox_sec_prop=val",
	                              nullptr};

	EXPECT_EQ(4, parse_environ(test_environ).size());
}

TEST(ParseEnv, FilterOutEmptySection)
{
	const char* test_environ[] = {"DOSBOX=value",
	                              "DOSBOX_=value",
	                              "DOSBOX__prop=value",
	                              nullptr};

	EXPECT_EQ(expected_empty, parse_environ(test_environ));
}

TEST(ParseEnv, FilterOutEmptyPropname)
{
	const char* test_environ[] = {"DOSBOX_sec=value", "DOSBOX_sec_=value", nullptr};

	EXPECT_EQ(expected_empty, parse_environ(test_environ));
}

TEST(Value, WalkThrough)
{
	struct TestItem {
		using value_types = std::variant<std::monostate, Hex, bool, int,
		                                 const char*, std::string, double>;

		TestItem(const value_types& value_,
		         const std::string& tostring_result_, Value::Etype type_)
		        : test_value(create_value(value_)),
		          check_value(create_value(tostring_result_, type_)),
		          value(value_),
		          tostring_result(tostring_result_),
		          etype(type_)
		{}

		static Value create_value(const value_types& raw_value)
		{
			return std::visit(
			        Overload{
			                [](std::monostate) { return Value{}; },
			                [](auto arg) { return Value{arg}; },
			        },
			        raw_value);
		};
		static Value create_value(const std::string& str, Value::Etype type)
		{
			return type != Value::V_NONE ? Value(str, type) : Value{};
		}

		const Value test_value;
		const Value check_value;
		const value_types value;
		const std::string tostring_result;
		const Value::Etype etype;
	};

	static const std::vector<TestItem> values{
	        {{}, "", Value::V_NONE},
	        {Hex(0x1b), "1b", Value::V_HEX},
	        {true, "true", Value::V_BOOL},
	        {42, "42", Value::V_INT},
	        {"abc", "abc", Value::V_STRING},
	        {std::string("abc"), "abc", Value::V_STRING},
	        {0.0, "0.00", Value::V_DOUBLE}};

	for (const auto& item : values) {
		const auto& test_value  = item.test_value;
		const auto& check_value = item.check_value;
		const auto etype        = item.etype;
		EXPECT_EQ(test_value.type, check_value.type);
		EXPECT_EQ(test_value.type, etype);
		if (test_value.type != Value::V_NONE) {
			EXPECT_TRUE(test_value == check_value);
			EXPECT_FALSE(test_value < check_value);
			EXPECT_FALSE(check_value < test_value);
			EXPECT_EQ(test_value.ToString(), check_value.ToString());
			EXPECT_EQ(test_value.ToString(), item.tostring_result);
			// Check invocations of the operator type()
			// definitions for all types.
			EXPECT_TRUE(std::visit(
			        Overload{
			                [test_value](std::monostate) {
				                return false;
			                },
			                [test_value](Hex arg) {
				                return arg == test_value;
			                },
			                [test_value](const char* arg) {
				                return std::string(test_value) == arg;
			                },
			                [test_value](auto arg) {
				                return arg ==
				                       static_cast<decltype(arg)>(
				                               test_value);
			                },
			        },
			        item.value));
		}
	}
}

} // namespace
