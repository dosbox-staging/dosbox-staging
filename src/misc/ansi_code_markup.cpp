/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022  The DOSBox Staging Team
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

#include <regex>
#include <unordered_map>

#include "string_utils.h"
#include "support.h"
#include "ansi_code_markup.h"

/*!
 * \brief Represents a markup tag.
 *
 * The constructor takes the string of a tag name and its optional value, and
 * attempts to construct a valid tag that can be easily queried for its
 * properties.
 *
 * The tag will be valid if, and only if, valid() returns true. Otherwise the
 * tag could not be recognized.
 */
class Tag {
public:
	enum class Group {
		Invalid,
		Colors,
		Erasers,
		Styles,
		Misc,
	};

	enum class Type {
		Invalid,
		Color,
		BGColor,
		EraseL,
		EraseS,
		It,
		Bold,
		Ul,
		Strike,
		Blink,
		Dim,
		Hidden,
		Inverse,
		Reset,
	};

	enum class Color {
		Invalid,
		Black,
		Red,
		Green,
		Yellow,
		Blue,
		Magenta,
		Cyan,
		White,
		Default,
	};

	enum class EraseExtents {
		Invalid,
		Begin,
		End,
		Entire,
	};

	Tag() = delete;
	Tag(std::string &tag, std::string &val, const bool close);

	/*!
	 * \brief Determine if tag object is valid.
	 *
	 * This should be called after creation of the tag object. If false, all
	 * other methods do not provide valid values.
	 *
	 * \return true
	 * \return false
	 */
	bool valid() const { return is_valid; }
	bool closed() const { return is_closed; }
	Group group() const { return t_detail.group; }
	Type type() const { return t_detail.type; }
	bool color_is_light() const { return c_detail.is_light; }
	int ansi_num() const;

private:
	bool parse_color_val(const std::string &val);
	bool parse_erase_val(const std::string &val);

	bool is_closed = false;
	bool is_valid = false;

	struct TagDetail {
		Group group = Group::Invalid;
		Type type = Type::Invalid;
		int ansi_num = -1;
	} t_detail = {};

	struct ColorDetail {
		Color color = Color::Invalid;
		int base_ansi_num = -1;
		bool is_light = false;
	} c_detail = {};

	struct EraseDetail {
		EraseExtents extents = EraseExtents::Invalid;
		int ansi_num = -1;
	} e_detail = {};

	static inline const std::string light_prefix = "light-";

	static inline const std::unordered_map<std::string, TagDetail> tags = {
	        {"color", {Group::Colors, Type::Color, -1}},
	        {"bgcolor", {Group::Colors, Type::BGColor, -1}},
	        {"erasel", {Group::Erasers, Type::EraseL, -1}},
	        {"erases", {Group::Erasers, Type::EraseS, -1}},
	        {"i", {Group::Styles, Type::It, 3}},
	        {"b", {Group::Styles, Type::Bold, 1}},
	        {"u", {Group::Styles, Type::Ul, 4}},
	        {"s", {Group::Styles, Type::Strike, 9}},
	        {"blink", {Group::Styles, Type::Blink, 5}},
	        {"dim", {Group::Styles, Type::Dim, 2}},
	        {"hidden", {Group::Styles, Type::Hidden, 8}},
	        {"inverse", {Group::Styles, Type::Inverse, 7}},
	        {"reset", {Group::Misc, Type::Reset, 0}},
	};

	static inline const std::unordered_map<std::string, ColorDetail> color_values = {
	        {"black", {Color::Black, 30, false}},
	        {"red", {Color::Red, 31, false}},
	        {"green", {Color::Green, 32, false}},
	        {"yellow", {Color::Yellow, 33, false}},
	        {"blue", {Color::Blue, 34, false}},
	        {"magenta", {Color::Magenta, 35, false}},
	        {"cyan", {Color::Cyan, 36, false}},
	        {"white", {Color::White, 37, false}},
	        {"default", {Color::Default, 39, false}},
	        {"light-black", {Color::Black, 30, true}},
	        {"light-red", {Color::Red, 31, true}},
	        {"light-green", {Color::Green, 32, true}},
	        {"light-yellow", {Color::Yellow, 33, true}},
	        {"light-blue", {Color::Blue, 34, true}},
	        {"light-magenta", {Color::Magenta, 35, true}},
	        {"light-cyan", {Color::Cyan, 36, true}},
	        {"light-white", {Color::White, 37, true}},
	        {"light-default", {Color::Default, 39, true}},
	};

	static inline const std::unordered_map<std::string, EraseDetail> eraser_extents = {
	        {"end", {EraseExtents::End, 0}},
	        {"begin", {EraseExtents::Begin, 1}},
	        {"entire", {EraseExtents::End, 2}},
	};
};

Tag::Tag(std::string &tag, std::string &val, const bool close)
{
	lowcase(tag);
	lowcase(val);
	if (!contains(tags, tag)) {
		return;
	}
	is_closed = close;
	t_detail = tags.at(tag);
	if ((t_detail.group == Group::Colors || t_detail.group == Group::Erasers) &&
	    is_closed) {
		return;
	}
	if (t_detail.group == Group::Colors && !parse_color_val(val)) {
		return;
	}
	if (t_detail.group == Group::Erasers && !parse_erase_val(val)) {
		return;
	}
	is_valid = true;
	return;
}

bool Tag::parse_color_val(const std::string &val)
{
	if (!contains(color_values, val)) {
		return false;
	}
	c_detail = color_values.at(val);
	return true;
}
bool Tag::parse_erase_val(const std::string &val)
{
	if (!contains(eraser_extents, val)) {
		return false;
	}
	e_detail = eraser_extents.at(val);
	return true;
}

int Tag::ansi_num() const
{
	switch (t_detail.group) {
	case Group::Colors: return c_detail.base_ansi_num; break;
	case Group::Erasers: return e_detail.ansi_num; break;
	default: return t_detail.ansi_num; break;
	}
}

/*
 * Regular expression to match tags
 *
 * The following is an example with group numbers:
 *               _____2_____
 *               |		   |
 * This color is [color=red] red
 *                |_4_| |6|
 *                |_5_|
 *
 * The folowing is a closing tag example:
 * _____2____
 * |		|
 * [/inverse]
 *  ||     |
 *  3|__4__|
 */
static std::regex markup(R"((\\)?)" // Escape tag? (1)
                         "(\\["     // Opening bracket, open main group (2)
                         "[ \\t]*?" // Optional spacing after opening bracket
                         "(\\/)?"   // Check for closing tag (3)
                         "("        // Start group of tags (4)
                         // Select tags which require a value (5)
                         "(color|bgcolor|erasel|erases)"
                         // Color or erase value. '=' not captured in separate
                         // group. Spacing around '=' is allowed (6)
                         "(?:[ \\t]*?=[ \\t]*?([a-z\\-]+))?"
                         // All other tags to match
                         "|i|b|u|s|blink|dim|hidden|inverse|reset"
                         ")"        // End group of tags (4)
                         "[ \\t]*?" // Optional spacing before closing bracket
                         "\\])" // Closing bracket. Note: Clang and MSVC requires
                                // this to be escaped. Closing main group (2)
                         ,
                         std::regex::icase);

static char ansi_code[10] = {};

/*!
 * \brief Returns a pointer to an ANSI code
 *
 * The returned string remains valid until the next
 * call to this function.
 *
 * nullptr will be returned if the provided tag
 * is not valid.
 *
 * \param tag a valid tag
 * \return const char*
 */
static const char *get_ansi_code(const Tag &tag)
{
	if (!tag.valid()) {
		return nullptr;
	}
	reset_str(ansi_code);
	Tag::Group group = tag.group();
	Tag::Type type = tag.type();
	int ansi_num = tag.ansi_num();
	switch (group) {
	case Tag::Group::Colors:
		// Background colors have codes that are +10
		// the equivalent foreground color.
		ansi_num = ansi_num + (type == Tag::Type::BGColor ? 10 : 0);
		safe_sprintf(ansi_code, "\033[%d%sm", ansi_num,
		             (tag.color_is_light() ? "" : ";1"));
		break;

	case Tag::Group::Erasers:
		safe_sprintf(ansi_code, "\033[%d%sm", ansi_num,
		             type == Tag::Type::EraseL ? "K" : "J");
		break;

	case Tag::Group::Styles:
		// "closing" tags have ascii codes +20
		if (tag.closed()) {
			ansi_num += 20 + (type == Tag::Type::Bold ? 1 : 0); // [/b] is the same as
			                                                    // [/dim]
		}
		safe_sprintf(ansi_code, "\033[%dm", ansi_num);
		break;

	default: safe_sprintf(ansi_code, "\033[%dm", ansi_num); break;
	}
	return ansi_code;
}

std::string convert_ansi_markup(std::string &str)
{
	return convert_ansi_markup(str.c_str());
}

/*!
 * \brief Convert ANSI markup tags in string to ANSI terminal codes
 *
 * Tags are in the form of [tagname]some text[/tagname]. Not all tags
 * have closing counterparts, such as [reset], [color], [bgcolor], [erasel] and
 * [erases].
 *
 * Color and erase tags take a required parameter in the form [color=value],
 * [bgcolor=value], [erasel=value], [erases=value].
 *
 * Tag matching is case insensitive, and spacing is allowed between
 * '[', ']' and '='.
 *
 * If a tag cannot be parsed for any reason, the resulting string will
 * contain the original unparsed tag.
 *
 * Existing ANSI terminal codes are preserved in the output string.
 *
 * \param str string containing ANSI markup tags
 * \return std::string
 */
std::string convert_ansi_markup(const char *str)
{
	std::string result;
	const char *begin = str;
	const char *last_match = str;
	std::cmatch m;
	while (std::regex_search(begin, m, markup)) {
		const char *r = nullptr;
		bool escape = m[1].matched;
		if (!escape) {
			bool close = m[3].matched;
			std::string tag = m[5].matched ? m[5].str() : m[4].str();
			std::string val = m[5].matched ? m[6].str() : "";
			Tag t(tag, val, close);
			r = get_ansi_code(t);
		}
		// Copy text before current match to output string
		result += m.prefix().str();
		result += r ? r : m[2].str();

		// Continue the next iteration from the end of the current match
		begin += m.position() + m.length();
		last_match = m[0].second;
	}
	// Add the rest of the string after all matches have been found
	result += last_match;
	// And just in case our result is empty for some reason, set output
	// string to input string
	if (result.empty()) {
		result = str;
	}
	return result;
}
