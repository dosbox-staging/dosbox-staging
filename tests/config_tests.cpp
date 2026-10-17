// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config/config.h"
#include "config/setup.h"

#include <memory>
#include <string>
#include <utility>

#include <gtest/gtest.h>

namespace {

struct StringCase {
	const char* input    = "";
	const char* expected = "";
};

class ConfigParsing : public ::testing::Test {
protected:
	void SetUp() override
	{
		previous_control = std::move(control);
		control          = std::make_unique<Config>(&command_line);
		section          = control->AddSection("parsing");
	}

	void TearDown() override
	{
		control = std::move(previous_control);
	}

	static constexpr auto WhenIdle = Property::Changeable::WhenIdle;

	CommandLine command_line{0, nullptr};
	ConfigPtr previous_control = {};
	SectionProp* section       = nullptr;
};

TEST_F(ConfigParsing, PreservesExistingStringQuoteAndWhitespaceHandling)
{
	const auto property = section->AddString("text", WhenIdle, "default");
	EXPECT_FALSE(property->PreservesQuotes());

	// clang-format off
	const StringCase cases[] = {
	        {"", ""},
	        {" \t\r\n\f ", ""},
	        {"value", "value"},
	        {" \tvalue\t ", "value"},
	        {"two  words", "two  words"},

	        {"\"value\"", "value"},
	        {"'value'", "value"},
	        {" \t\"  value \t\" \t", "value"},
	        {" \t'  value \t' \t", "value"},

	        {"\"\"", ""},
	        {"''", ""},
	        {"\" \t \"", ""},
	        {"' \t '", ""},

	        {"\"", "\""},
	        {"'", "'"},
	        {"\"value'", "\"value'"},
	        {"'value\"", "'value\""},
	        {"\"unclosed", "\"unclosed"},

	        {"\"'value'\"", "'value'"},
	        {"\"\"value\"\"", "\"value\""},
	        {"left=right", "left=right"},
	        {"\"FIRST=one\" \"SECOND=two\"", "FIRST=one\" \"SECOND=two"},
	};
	// clang-format on

	for (const auto& test : cases) {
		SCOPED_TRACE(test.input);

		ASSERT_TRUE(section->HandleInputLine(std::string(" \tTeXt \t= ") +
		                                     test.input));
		EXPECT_EQ(section->GetString("text"), test.expected);
	}
}

TEST_F(ConfigParsing, PreservesQuotesAndEnclosedWhitespaceOnlyWhenRequested)
{
	const auto property = section->AddString("arguments", WhenIdle, "");
	property->SetPreserveQuotes(true);
	section->AddString("text", WhenIdle, "");

	// clang-format off
	const StringCase cases[] = {
	        {"", ""},
	        {" \t ", ""},
	        {" \tvalue \t", "value"},

	        {" \t\"  value \t\" \t", "\"  value \t\""},
	        {" \t'  value \t' \t", "'  value \t'"},
	        {"\"\"", "\"\""},
	        {"''", "''"},

	        {"\"", "\""},
	        {"\"value'", "\"value'"},
	        {"\"unclosed", "\"unclosed"},

	        {R"( "FIRST=C:\My ROMs" "SECOND=another value" )",
	         R"("FIRST=C:\My ROMs" "SECOND=another value")"},
	};
	// clang-format on

	for (const auto& test : cases) {
		SCOPED_TRACE(test.input);

		ASSERT_TRUE(section->HandleInputLine(std::string("arguments = ") +
		                                     test.input));
		EXPECT_EQ(section->GetString("arguments"), test.expected);
	}

	ASSERT_TRUE(section->HandleInputLine("text = \"  value  \""));
	EXPECT_EQ(section->GetString("text"), "value");

	property->SetPreserveQuotes(false);
	ASSERT_TRUE(section->HandleInputLine("arguments = \"  value  \""));
	EXPECT_EQ(section->GetString("arguments"), "value");
}

TEST_F(ConfigParsing, PreservesQuotedScalarValues)
{
	section->AddInt("integer", WhenIdle, 0);
	section->AddBool("boolean", WhenIdle, false);
	section->AddHex("address", WhenIdle, 0);

	for (const auto* input : {"42", "\"42\"", "'42'", " \t\" 42 \t\" \t"}) {
		SCOPED_TRACE(input);

		ASSERT_TRUE(section->HandleInputLine(std::string("integer = ") +
		                                     input));
		EXPECT_EQ(section->GetInt("integer"), 42);
	}

	ASSERT_TRUE(section->HandleInputLine("boolean = \" true \t\""));
	EXPECT_TRUE(section->GetBool("boolean"));

	ASSERT_TRUE(section->HandleInputLine("boolean = ' false '"));
	EXPECT_FALSE(section->GetBool("boolean"));

	ASSERT_TRUE(section->HandleInputLine("address = ' 0x330 '"));
	EXPECT_EQ(section->GetHex("address"), Hex(0x330));
}

TEST_F(ConfigParsing, PreservesQuotedMultiValueSettings)
{
	const auto property = section->AddMultiVal("values", WhenIdle, " ");
	const auto values   = property->GetSection();
	values->AddString("mode", WhenIdle, "");
	values->AddInt("amount", WhenIdle, 0);

	ASSERT_TRUE(section->HandleInputLine("values = \"  mode 42  \""));
	EXPECT_EQ(section->GetPropertyValue("values"), "mode 42");
	EXPECT_EQ(values->GetString("mode"), "mode");
	EXPECT_EQ(values->GetInt("amount"), 42);
}

TEST_F(ConfigParsing, RejectsLinesWithoutAnAssignment)
{
	section->AddString("text", WhenIdle, "default");

	EXPECT_FALSE(section->HandleInputLine("text"));
	EXPECT_EQ(section->GetString("text"), "default");
}

} // namespace
