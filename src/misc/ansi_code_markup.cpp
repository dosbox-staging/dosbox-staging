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
	enum class TagGroup {
		InvalidGroup,
		Colors,
		Erasers,
		Styles,
		Misc,
	};

	enum class TagName {
		InvalidName,
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

	enum class ColorName {
		InvalidColor,
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
		InvalidExtents,
		Begin,
		End,
		Entire,
	};

	struct TagInfo {
		TagGroup group = TagGroup::InvalidGroup;
		TagName name = TagName::InvalidName;
		int ansi_num = -1;
	};

	struct ColorInfo {
		ColorName name = ColorName::InvalidColor;
		int base_ansi_num = -1;
		bool is_light = false;
	};

	struct EraseInfo {
		EraseExtents extents = EraseExtents::InvalidExtents;
		int ansi_num = -1;
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
	const TagInfo &info() const { return t_info; }
	/*!
	 * \brief Return extra info about color tags
	 *
	 * The returned information will only be valid if valid() returns true
	 * and info().group == TagGroup::Colors
	 *
	 * \return const ColorInfo&
	 */
	const ColorInfo &color_info() const { return c_info; }
	/*!
	 * \brief Return extra info about erase tags
	 *
	 * The returned information will only be valid if valid() returns true
	 * and info().group == TagGroup::Erasers
	 *
	 * \return const EraseInfo&
	 */
	const EraseInfo &erase_info() const { return e_info; }

private:
	bool parse_color_val(const std::string &val);
	bool parse_erase_val(const std::string &val);

	bool is_closed = false;
	bool is_valid = false;

	TagInfo t_info;
	ColorInfo c_info;
	EraseInfo e_info;

	static inline const std::string light_prefix = "light-";

	static inline const std::unordered_map<std::string, TagInfo> tags = {
	        {"color", {TagGroup::Colors, TagName::Color, -1}},
	        {"bgcolor", {TagGroup::Colors, TagName::BGColor, -1}},
	        {"erasel", {TagGroup::Erasers, TagName::EraseL, -1}},
	        {"erases", {TagGroup::Erasers, TagName::EraseS, -1}},
	        {"i", {TagGroup::Styles, TagName::It, 3}},
	        {"b", {TagGroup::Styles, TagName::Bold, 1}},
	        {"u", {TagGroup::Styles, TagName::Ul, 4}},
	        {"s", {TagGroup::Styles, TagName::Strike, 9}},
	        {"blink", {TagGroup::Styles, TagName::Blink, 5}},
	        {"dim", {TagGroup::Styles, TagName::Dim, 2}},
	        {"hidden", {TagGroup::Styles, TagName::Hidden, 8}},
	        {"inverse", {TagGroup::Styles, TagName::Inverse, 7}},
	        {"reset", {TagGroup::Misc, TagName::Reset, 0}},
	};

	static inline const std::unordered_map<std::string, ColorInfo> color_values = {
	        {"black", {ColorName::Black, 30, false}},
	        {"red", {ColorName::Red, 31, false}},
	        {"green", {ColorName::Green, 32, false}},
	        {"yellow", {ColorName::Yellow, 33, false}},
	        {"blue", {ColorName::Blue, 34, false}},
	        {"magenta", {ColorName::Magenta, 35, false}},
	        {"cyan", {ColorName::Cyan, 36, false}},
	        {"white", {ColorName::White, 37, false}},
	        {"default", {ColorName::Default, 39, false}},
	        {"light-black", {ColorName::Black, 30, true}},
	        {"light-red", {ColorName::Red, 31, true}},
	        {"light-green", {ColorName::Green, 32, true}},
	        {"light-yellow", {ColorName::Yellow, 33, true}},
	        {"light-blue", {ColorName::Blue, 34, true}},
	        {"light-magenta", {ColorName::Magenta, 35, true}},
	        {"light-cyan", {ColorName::Cyan, 36, true}},
	        {"light-white", {ColorName::White, 37, true}},
	        {"light-default", {ColorName::Default, 39, true}},
	};

	static inline const std::unordered_map<std::string, EraseInfo> eraser_extents = {
	        {"end", {EraseExtents::End, 0}},
	        {"begin", {EraseExtents::Begin, 1}},
	        {"entire", {EraseExtents::End, 2}},
	};
};

Tag::Tag(std::string &tag, std::string &val, const bool close)
        : t_info(),
          c_info(),
          e_info()
{
	lowcase(tag);
	lowcase(val);
	if (!contains(tags, tag)) {
		return;
	}
	is_closed = close;
	t_info = tags.at(tag);
	if ((t_info.group == TagGroup::Colors || t_info.group == TagGroup::Erasers) &&
	    is_closed) {
		return;
	}
	if (t_info.group == TagGroup::Colors && !parse_color_val(val)) {
		return;
	}
	if (t_info.group == TagGroup::Erasers && !parse_erase_val(val)) {
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
	c_info = color_values.at(val);
	return true;
}
bool Tag::parse_erase_val(const std::string &val)
{
	if (!contains(eraser_extents, val)) {
		return false;
	}
	e_info = eraser_extents.at(val);
	return true;
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
	int ansi_num = -1;
	reset_str(ansi_code);
	auto &tag_info = tag.info();
	switch (tag_info.group) {
	case Tag::TagGroup::Colors:
		// Background colors have codes that are +10
		// the equivalent foreground color.
		ansi_num = tag.color_info().base_ansi_num +
		           (tag_info.name == Tag::TagName::BGColor ? 10 : 0);
		safe_sprintf(ansi_code, "\033[%d%sm", ansi_num,
		             (tag.color_info().is_light ? "" : ";1"));
		break;

	case Tag::TagGroup::Erasers:
		ansi_num = tag.erase_info().ansi_num;
		safe_sprintf(ansi_code, "\033[%d%sm", ansi_num,
		             tag_info.name == Tag::TagName::EraseL ? "K" : "J");
		break;

	case Tag::TagGroup::Styles:
		ansi_num = tag_info.ansi_num;
		// "closing" tags have ascii codes +20
		if (tag.closed()) {
			ansi_num += 20 + (tag_info.name == Tag::TagName::Bold
			                          ? 1
			                          : 0); // [/b] is the same as
			                                // [/dim]
		}
		safe_sprintf(ansi_code, "\033[%dm", ansi_num);
		break;

	default:
		ansi_num = tag_info.ansi_num;
		safe_sprintf(ansi_code, "\033[%dm", ansi_num);
		break;
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
