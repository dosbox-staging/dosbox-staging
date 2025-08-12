// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config/setup.h"

#include <gtest/gtest.h>

#include "misc/support.h"
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

TEST(Value, None)
{
	Value test_value{};
	Value check_value("", Value::V_NONE);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_NONE);
}

TEST(Value, Hex)
{
	Value test_value{Hex(0x42)};
	Value check_value("42", Value::V_HEX);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_HEX);
	EXPECT_TRUE(test_value == check_value);
	EXPECT_FALSE(test_value < check_value);
	EXPECT_FALSE(check_value < test_value);
	EXPECT_EQ(test_value.ToString(), check_value.ToString());
	EXPECT_EQ(test_value.ToString(), "42");
	EXPECT_EQ(test_value, Hex(0x42));
	EXPECT_EQ(Hex(0x42), test_value);
}

TEST(Value, Bool)
{
	Value test_value{true};
	Value check_value("on", Value::V_BOOL);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_BOOL);
	EXPECT_TRUE(test_value == check_value);
	EXPECT_FALSE(test_value < check_value);
	EXPECT_FALSE(check_value < test_value);
	EXPECT_EQ(test_value.ToString(), check_value.ToString());
	EXPECT_EQ(test_value.ToString(), "on");
	EXPECT_TRUE(test_value);
}

TEST(Value, Int)
{
	Value test_value{42};
	Value check_value("42", Value::V_INT);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_INT);
	EXPECT_TRUE(test_value == check_value);
	EXPECT_FALSE(test_value < check_value);
	EXPECT_FALSE(check_value < test_value);
	EXPECT_EQ(test_value.ToString(), check_value.ToString());
	EXPECT_EQ(test_value.ToString(), "42");
	EXPECT_EQ(static_cast<int>(test_value), 42);
}

TEST(Value, CString)
{
	Value test_value{"abc"};
	Value check_value("abc", Value::V_STRING);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_STRING);
	EXPECT_TRUE(test_value == check_value);
	EXPECT_FALSE(test_value < check_value);
	EXPECT_FALSE(check_value < test_value);
	EXPECT_EQ(test_value.ToString(), check_value.ToString());
	EXPECT_EQ(test_value.ToString(), "abc");
	EXPECT_EQ(test_value, "abc");
}

TEST(Value, StdString)
{
	Value test_value{std::string("cde")};
	Value check_value("cde", Value::V_STRING);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_STRING);
	EXPECT_TRUE(test_value == check_value);
	EXPECT_FALSE(test_value < check_value);
	EXPECT_FALSE(check_value < test_value);
	EXPECT_EQ(test_value.ToString(), check_value.ToString());
	EXPECT_EQ(test_value.ToString(), "cde");
	EXPECT_EQ(test_value, "cde");
}

TEST(Value, Double)
{
	Value test_value{42.0};
	Value check_value("42", Value::V_DOUBLE);
	EXPECT_EQ(test_value.type, check_value.type);
	EXPECT_EQ(test_value.type, Value::V_DOUBLE);
	EXPECT_TRUE(test_value == check_value);
	EXPECT_FALSE(test_value < check_value);
	EXPECT_FALSE(check_value < test_value);
	EXPECT_EQ(test_value.ToString(), check_value.ToString());
	EXPECT_EQ(test_value.ToString(), "42.00");
	EXPECT_DOUBLE_EQ(test_value, 42.0);
}

} // namespace
