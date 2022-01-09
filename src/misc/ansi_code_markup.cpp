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

template <typename K, typename V>
static bool map_contains(std::unordered_map<K, V> &map, K key)
{
	return map.find(key) != map.end();
}

class ColorParser {
public:
	/*!
	 * \brief Returns a pointer to a color ANSI code
	 *
	 * The returned string is owned by this object. It remains valid until
	 * the next call to this function.
	 *
	 * \param color the color value
	 * \param background whether to set the backround (true) or foreground
	 *                   (false) color
	 * \return const char*
	 */
	const char *get_ansi_code(std::string &color, bool background)
	{
		reset_str(ansi_code);
		std::string c = color;
		bool light = starts_with(light_prefix, color);
		if (light) {
			c = color.substr(sizeof light_prefix - 1);
		}
		if (!map_contains<std::string, int>(base_colors, c)) {
			return nullptr;
		}
		// Background colors have codes that are +10
		// the equivalent foreground color.
		int ansi_num = base_colors.at(c) + (background ? 10 : 0);
		safe_sprintf(ansi_code, "\033[%d%sm", ansi_num, (light ? "" : ";1"));
		return ansi_code;
	}

private:
	char ansi_code[10] = {0};
	char light_prefix[7] = "light-";
	std::unordered_map<std::string, int> base_colors = {
	        {"black", 30},  {"red", 31},   {"green", 32},
	        {"yellow", 33}, {"blue", 34},  {"magenta", 35},
	        {"cyan", 36},   {"white", 37}, {"default", 39},
	};
};

class TagParser {
public:
	TagParser() : color() {}
	/*!
	 * \brief Returns a pointer to an ANSI code
	 *
	 * The returned string is owned by this object. It remains valid until
	 * the next call to this function.
	 *
	 * \param tag tag name (eg: b, color)
	 * \param close whether this is a closing tag
	 * \param color_val the color value if a color tag
	 * \return const char*
	 */
	const char *get_ansi_code(std::string &tag, bool close, std::string &color_val)
	{
		if (tag == "color" || tag == "bgcolor") {
			// We don't support closing color tags
			if (close) {
				return nullptr;
			}
			return color.get_ansi_code(color_val, tag == "bgcolor");
		}
		reset_str(ansi_code);
		if (!map_contains<std::string, int>(tags, tag)) {
			return nullptr;
		}
		int ansi_num = tags.at(tag);
		if (close) {
			// "closing" tags have ascii codes +20
			ansi_num += 20 + (tag == "b" ? 1 : 0); // [/b] is the
			                                       // same as [/dim]
		}
		safe_sprintf(ansi_code, "\033[%dm", ansi_num);
		return ansi_code;
	}

private:
	ColorParser color;
	char ansi_code[10] = {0};
	std::unordered_map<std::string, int> tags = {
	        {"reset", 0}, {"b", 1},       {"dim", 2},
	        {"i", 3},     {"u", 4},       {"s", 9},
	        {"blink", 5}, {"inverse", 7}, {"hidden", 8},
	};
};

static TagParser tag_parser;

static std::regex markup(R"((\\)?)" // Escape tag?
						 "(\\["      // Opening bracket, open main group
                         "[ \\t]*?" // Optional spacing after opening bracket
                         "(\\/)?"   // Check for losing tag
                         "("        // Start group of tags
                         "((?:bg)?color)" // Select color or bgcolor. bg not
                                          // captured in separate group
                         "(?:[ \\t]*?=[ \\t]*?([a-z\\-]+))?" // Color value. '='
                                                             // not captured in
                                                             // separate group.
                                                             // Spacing around
                                                             // '=' is allowed
                         "|i|b|u|s|blink|dim|hidden|inverse|reset" // All other
                                                                   // tags to match
                         ")"        // End group of tags
                         "[ \\t]*?" // Optional spacing before closing bracket
                         "\\])" // Closing bracket. Note: Clang and MSVC requires
                               // this to be escaped. Closing main group
                         ,
                         std::regex::icase);

std::string convert_ansi_markup(std::string &str)
{
	return convert_ansi_markup(str.c_str());
}

/*!
 * \brief Convert ANSI markup tags in string to ANSI terminal codes
 *
 * Tags are in the form of [tagname]some text[/tagname]. Not all tags
 * have closing counterparts, such as [reset], [color], and [bgcolor].
 *
 * Color tags take a required parameter in the form [color=value] or
 * [bgcolor=value]. Tag matching is case insensitive, and spacing is
 * allowed between '[', ']' and '='.
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
		const char* r = nullptr;
		bool escape = m[1].matched;
		if (!escape) {
			bool close = m[3].matched;
			std::string tag = m[5].matched ? m[5].str() : m[4].str();
			std::string color = m[5].matched ? m[6].str() : "";
			lowcase(tag);
			lowcase(color);
			r = tag_parser.get_ansi_code(tag, close, color);
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
