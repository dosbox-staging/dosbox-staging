// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/private/messages_adjust.h"

#include <cstdint>
#include <gtest/gtest.h>

#include <string>
#include <sys/types.h>

// The Tragedy of Romeo and Juliet, by William Shakespeare

static const std::string TestStringNewlineAfter =
        "Two households, both alike in dignity,\n"
        "In fair Verona, where we lay our scene,\n"
        "From ancient grudge break to new mutiny,\n"
        "Where civil blood makes civil hands unclean.\n";

static const std::string TestStringNewlineBefore =
        "\n"
        "From forth the fatal loins of these two foes\n"
        "A pair of star-cross'd lovers take their life;\n"
        "Whose misadventured piteous overthrows\n"
        "Do with their death bury their parents' strife.";

static const std::string TestStringNewlineBeforeAfter =
        "\n"
        "The fearful passage of their death-mark'd love,\n"
        "And the continuance of their parents' rage,\n"
        "Which, but their children's end, nought could remove,\n"
        "Is now the two hours' traffic of our stage;\n";

namespace {

TEST(Messages, Adjust_Newlines_1)
{
	const std::string& current = TestStringNewlineAfter;
	auto previous = std::string("\n\n") + current + std::string("\n");

	std::string translated = "\n\nLorem ipsum dolor sit amet\n\n";

	adjust_newlines(current, previous, translated);

	EXPECT_EQ(previous, current);
	EXPECT_EQ(translated, "Lorem ipsum dolor sit amet\n");
}

TEST(Messages, Adjust_Newlines_2)
{
	const std::string& current = TestStringNewlineBefore;
	auto previous = std::string("\n\n") + current + std::string("\n");

	std::string translated = "\n\n\nLorem ipsum dolor sit amet\n";

	adjust_newlines(current, previous, translated);

	EXPECT_EQ(previous, current);
	EXPECT_EQ(translated, "\nLorem ipsum dolor sit amet");
}

TEST(Messages, Adjust_Newlines_3)
{
	const std::string& current = TestStringNewlineBeforeAfter;
	auto previous = std::string("\n\n") + current + std::string("\n");

	std::string translated = "\n\n\nLorem ipsum dolor sit amet\n\n";

	adjust_newlines(current, previous, translated);

	EXPECT_EQ(previous, current);
	EXPECT_EQ(translated, "\nLorem ipsum dolor sit amet\n");
}

TEST(Messages, Skip_Adjust_Newlines_1)
{
	// Here we expect the newline adjustment won't be performed due to
	// English string changed beyond just leading/trailing newlines

	const std::string& current = TestStringNewlineBefore;
	auto previous = std::string("\n") + "FooBar" + std::string("\n");

	const std::string previous_original = previous;
	std::string translated = "\nLorem ipsum dolor sit amet\n";

	adjust_newlines(current, previous, translated);

	EXPECT_EQ(previous, previous_original);
	EXPECT_EQ(translated, "\nLorem ipsum dolor sit amet\n");
}

TEST(Messages, Skip_Adjust_Newlines_2)
{
	// Here we expect the newline adjustment won't be performed due to
	// translated message having different number of leading newlines
	// than the original English string

	const std::string& current = TestStringNewlineBeforeAfter;
	auto previous = std::string("\n") + current + std::string("\n");

	const std::string previous_original = previous;
	std::string translated = "Lorem ipsum dolor sit amet\n\n";

	adjust_newlines(current, previous, translated);

	EXPECT_EQ(previous, previous_original);
	EXPECT_EQ(translated, "Lorem ipsum dolor sit amet\n\n");
}

TEST(Messages, Skip_Adjust_Newlines_3)
{
	// Here we expect the newline adjustment won't be performed due to
	// translated message having different number of trailing newlines
	// than the original English string

	const std::string& current = TestStringNewlineBeforeAfter;
	auto previous = std::string("\n") + current + std::string("\n");

	const std::string previous_original = previous;
	std::string translated = "\n\nLorem ipsum dolor sit amet";

	adjust_newlines(current, previous, translated);

	EXPECT_EQ(previous, previous_original);
	EXPECT_EQ(translated, "\n\nLorem ipsum dolor sit amet");
}

} // namespace
