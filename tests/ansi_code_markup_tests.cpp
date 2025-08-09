// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/ansi_code_markup.h"

#include <gtest/gtest.h>

namespace {
TEST(ConvertAnsiMarkup, ValidForegroundColorMiddle)
{
    const char str[] = "this [color=red]colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "this \033[31mcolour is red");
}

TEST(ConvertAnsiMarkup, ValidForegroundColorStart)
{
    const char str[] = "[color=red]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "\033[31mthis colour is red");
}

TEST(ConvertAnsiMarkup, InvalidForegroundColorEnd)
{
    const char str[] = "the color will be reset[/color]";
    EXPECT_EQ(convert_ansi_markup(str), "the color will be reset[/color]");
}

TEST(ConvertAnsiMarkup, ValidForegroundLightBlue)
{
    const char str[] = "this [color=light-blue]colour is light blue";
    EXPECT_EQ(convert_ansi_markup(str), "this \033[34;1mcolour is light blue");
}

TEST(ConvertAnsiMarkup, InvalidForegroundColor)
{
    const char str[] = "[color=invalid]this is an invalid foreground color";
    EXPECT_EQ(convert_ansi_markup(str), "[color=invalid]this is an invalid foreground color");
}

TEST(ConvertAnsiMarkup, ValidBackgroundColorMiddle)
{
    const char str[] = "this [bgcolor=red]colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "this \033[41mcolour is red");
}

TEST(ConvertAnsiMarkup, ValidBackgroundColorStart)
{
    const char str[] = "[bgcolor=red]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "\033[41mthis colour is red");
}

TEST(ConvertAnsiMarkup, InvalidBackgroundColorEnd)
{
    const char str[] = "the color will be reset[/bgcolor]";
    EXPECT_EQ(convert_ansi_markup(str), "the color will be reset[/bgcolor]");
}

TEST(ConvertAnsiMarkup, ValidBackgroundLightBlue)
{
    const char str[] = "this [bgcolor=light-blue]colour is light blue";
    EXPECT_EQ(convert_ansi_markup(str), "this \033[44;1mcolour is light blue");
}

TEST(ConvertAnsiMarkup, InvalidBackgroundColor)
{
    const char str[] = "[bgcolor=invalid]this is an invalid foreground color";
    EXPECT_EQ(convert_ansi_markup(str), "[bgcolor=invalid]this is an invalid foreground color");
}

TEST(ConvertAnsiMarkup, ValidForegroundColorMultiple)
{
    const char str[] = "this [color=red]colour is red. [color=blue]And this is blue.[reset]";
    EXPECT_EQ(convert_ansi_markup(str), "this \033[31mcolour is red. \033[34mAnd this is blue.\033[0m");
}

TEST(ConvertAnsiMarkup, InvalidForegroundColorNoValue)
{
    const char str[] = "this [color]colour is red.";
    EXPECT_EQ(convert_ansi_markup(str), "this [color]colour is red.");
}

TEST(ConvertAnsiMarkup, InvalidBackgroundColorNoValue)
{
    const char str[] = "this [bgcolor]colour is red.";
    EXPECT_EQ(convert_ansi_markup(str), "this [bgcolor]colour is red.");
}

TEST(ConvertAnsiMarkup, InvalidTagName)
{
    const char str[] = "this [sometag] tag is invalid.";
    EXPECT_EQ(convert_ansi_markup(str), "this [sometag] tag is invalid.");
}

TEST(ConvertAnsiMarkup, InvalidTagUnclosed)
{
    const char str[] = "this [color=red colour is red.";
    EXPECT_EQ(convert_ansi_markup(str), "this [color=red colour is red.");
}

TEST(ConvertAnsiMarkup, InvalidTagUnopened)
{
    const char str[] = "this color=red] colour is red.";
    EXPECT_EQ(convert_ansi_markup(str), "this color=red] colour is red.");
}

TEST(ConvertAnsiMarkup, Bold)
{
    const char str[] = "This is [b]bold[/b] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[1mbold\033[22m text.");
}

TEST(ConvertAnsiMarkup, Italic)
{
    const char str[] = "This is [i]italic[/i] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[3mitalic\033[23m text.");
}

TEST(ConvertAnsiMarkup, Underline)
{
    const char str[] = "This is [u]underline[/u] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[4munderline\033[24m text.");
}

TEST(ConvertAnsiMarkup, Strikethrough)
{
    const char str[] = "This is [s]strikethrough[/s] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[9mstrikethrough\033[29m text.");
}

TEST(ConvertAnsiMarkup, Dim)
{
    const char str[] = "This is [dim]dim[/dim] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[2mdim\033[22m text.");
}

TEST(ConvertAnsiMarkup, blink)
{
    const char str[] = "This is [blink]blink[/blink] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[5mblink\033[25m text.");
}

TEST(ConvertAnsiMarkup, Inverse)
{
    const char str[] = "This is [inverse]inverse[/inverse] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[7minverse\033[27m text.");
}

TEST(ConvertAnsiMarkup, Hidden)
{
    const char str[] = "This is [hidden]hidden[/hidden] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[8mhidden\033[28m text.");
}

TEST(ConvertAnsiMarkup, Uppercase)
{
    const char str[] = "This is [HIDDEN]hidden[/HIDDEN] text.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[8mhidden\033[28m text.");
}

TEST(ConvertAnsiMarkup, ColorUppercase)
{
    const char str[] = "[COLOR=RED]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "\033[31mthis colour is red");
}

TEST(ConvertAnsiMarkup, ColorUppercaseTag)
{
    const char str[] = "[COLOR=red]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "\033[31mthis colour is red");
}

TEST(ConvertAnsiMarkup, ColorUppercaseValue)
{
    const char str[] = "[color=RED]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "\033[31mthis colour is red");
}

TEST(ConvertAnsiMarkup, EraseScreenBeginning)
{
    const char str[] = "erase [erases=begin] to beginning of screen.";
    EXPECT_EQ(convert_ansi_markup(str), "erase \033[1J to beginning of screen.");
}

TEST(ConvertAnsiMarkup, EraseScreenEnd)
{
    const char str[] = "erase [erases=end] to end of screen.";
    EXPECT_EQ(convert_ansi_markup(str), "erase \033[0J to end of screen.");
}

TEST(ConvertAnsiMarkup, EraseScreenEntire)
{
    const char str[] = "[erases=entire] Erase entire screen.";
    EXPECT_EQ(convert_ansi_markup(str), "\033[2J Erase entire screen.");
}

TEST(ConvertAnsiMarkup, EraseLineBeginning)
{
    const char str[] = "erase [erasel=begin] to beginning of line.";
    EXPECT_EQ(convert_ansi_markup(str), "erase \033[1K to beginning of line.");
}

TEST(ConvertAnsiMarkup, EraseLineEnd)
{
    const char str[] = "erase [erasel=end] to end of line.";
    EXPECT_EQ(convert_ansi_markup(str), "erase \033[0K to end of line.");
}

TEST(ConvertAnsiMarkup, EraseLineEntire)
{
    const char str[] = "[erasel=entire] Erase entire line.";
    EXPECT_EQ(convert_ansi_markup(str), "\033[2K Erase entire line.");
}

TEST(ConvertAnsiMarkup, Whitespace)
{
    const char str[] = "[  color = red  ]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "\033[31mthis colour is red");
}

TEST(ConvertAnsiMarkup, EscapeTag)
{
    const char str[] = "\\[color=red]this colour is red";
    EXPECT_EQ(convert_ansi_markup(str), "[color=red]this colour is red");
}

TEST(ConvertAnsiMarkup, InvalidNesting)
{
    const char str[] = "This will be[bgcolor=light-blue [bgcolor=light-blue] ] light blue.";
    EXPECT_EQ(convert_ansi_markup(str), "This will be[bgcolor=light-blue \033[44;1m ] light blue.");
}

TEST(ConvertAnsiMarkup, EscapedBothBrackets)
{
    const char str[] = "This will be \\[bgcolor=light-blue\\] light blue.";
    EXPECT_EQ(convert_ansi_markup(str), "This will be \\[bgcolor=light-blue\\] light blue.");
}

TEST(ConvertAnsiMarkup, EscapedDoubleQuotes)
{
    const char str[] = "This will be [bgcolor=\"light-blue] light blue.";
    EXPECT_EQ(convert_ansi_markup(str), "This will be [bgcolor=\"light-blue] light blue.");
}

TEST(ConvertAnsiMarkup, EscapedMixedQuotes)
{
    const char str[] = "This will be [\"bgcolor=\'light-blue] light blue.";
    EXPECT_EQ(convert_ansi_markup(str), "This will be [\"bgcolor=\'light-blue] light blue.");
}

TEST(ConvertAnsiMarkup, NoMarkupPlain)
{
    const char str[] = "This is plain text with no markup.";
    EXPECT_EQ(convert_ansi_markup(str), "This is plain text with no markup.");
}

TEST(ConvertAnsiMarkup, NoMarkupExistingANSI)
{
    const char str[] = "This is \033[31mred text with no markup.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[31mred text with no markup.");
}

TEST(ConvertAnsiMarkup, MixedMarkupExistingANSI)
{
    const char str[] = "This is \033[31mred text with no markup. [color=blue]And this blue text with markup.";
    EXPECT_EQ(convert_ansi_markup(str), "This is \033[31mred text with no markup. \033[34mAnd this blue text with markup.");
}

TEST(ConvertAnsiMarkup, StartupMessage)
{
    const char new_msg[] = "[bgcolor=blue]\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	        "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	        "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
	        "\xBA [color=light-green]Welcome to DOSBox Staging %-40s[color=white] \xBA\n"
	        "\xBA                                                                    \xBA\n"
	        "\xBA For a short introduction for new users type: [color=yellow]INTRO[color=white]                 \xBA\n"
	        "\xBA For supported shell commands type: [color=yellow]HELP[color=white]                            \xBA\n"
	        "\xBA                                                                    \xBA\n"
	        "\xBA To adjust the emulated CPU speed, use [color=light-red]%s+F11[color=white] and \033[31m%s+F12[color=white].%s%s       \xBA\n"
	        "\xBA To activate the keymapper [color=light-red]%s+F1[color=white].%s                                 \xBA\n"
	        "\xBA                                                                    \xBA\n";

    std::string orig_msg = "\033[44m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	        "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
	        "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
	        "\xBA \033[32;1mWelcome to DOSBox Staging %-40s\033[37;1m \xBA\n"
	        "\xBA                                                                    \xBA\n"
	        "\xBA For a short introduction for new users type: \033[33;1mINTRO\033[37;1m                 \xBA\n"
	        "\xBA For supported shell commands type: \033[33;1mHELP\033[37;1m                            \xBA\n"
	        "\xBA                                                                    \xBA\n"
	        "\xBA To adjust the emulated CPU speed, use \033[31;1m%s+F11\033[37;1m and \033[31m%s+F12\033[37;1m.%s%s       \xBA\n"
	        "\xBA To activate the keymapper \033[31;1m%s+F1\033[37;1m.%s                                 \xBA\n"
	        "\xBA                                                                    \xBA\n";
    EXPECT_EQ(convert_ansi_markup(new_msg), orig_msg);
}
}

TEST(StripAnsiMarkup, ColorUppercaseTag)
{
    const auto str = "[COLOR=red]this colour is red";
    EXPECT_EQ(strip_ansi_markup(str), "this colour is red");
}

TEST(StripAnsiMarkup, MixedMarkupExistingAnsi)
{
    const auto str = "This is \033[31mred text with no markup. [color=blue]And this blue text with markup.";
    EXPECT_EQ(strip_ansi_markup(str), "This is \033[31mred text with no markup. And this blue text with markup.");
}
