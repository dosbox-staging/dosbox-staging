// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <regex>
#include <unordered_map>

#include "ansi_code_markup.h"
#include "util/string_utils.h"
#include "support.h"

// Pre-defined markups to help creating strings

const std::string Ansi::Reset               = "[reset]";

// Low intensity text colours
const std::string Ansi::ColorBlack          = "[color=black]";
const std::string Ansi::ColorBlue           = "[color=blue]";
const std::string Ansi::ColorGreen          = "[color=green]";
const std::string Ansi::ColorCyan           = "[color=cyan]";
const std::string Ansi::ColorRed            = "[color=red]";
const std::string Ansi::ColorMagenta        = "[color=magenta]";
const std::string Ansi::ColorBrown          = "[color=brown]";
const std::string Ansi::ColorLightGray      = "[color=light-grey]";

// High intensity text colours
const std::string Ansi::ColorDarkGray       = "[color=dark-grey]";
const std::string Ansi::ColorLightBlue      = "[color=light-blue]";
const std::string Ansi::ColorLightGreen     = "[color=light-green]";
const std::string Ansi::ColorLightCyan      = "[color=light-cyan]";
const std::string Ansi::ColorLightRed       = "[color=light-red]";
const std::string Ansi::ColorLightMagenta   = "[color=light-magenta]";
const std::string Ansi::ColorYellow         = "[color=yellow]";
const std::string Ansi::ColorWhite          = "[color=white]";

// Definitions to help keeping the command output style consistent
const std::string& Ansi::HighlightHeader    = Ansi::ColorWhite;
const std::string& Ansi::HighlightSelection = Ansi::ColorLightGreen;

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
		Invalid = -1,
		Color   = -2,
		BGColor = -3,
		EraseL  = -4,
		EraseS  = -5,
		It      = 3,
		Bold    = 1,
		Ul      = 4,
		Strike  = 9,
		Blink   = 5,
		Dim     = 2,
		Hidden  = 8,
		Inverse = 7,
		Reset   = 0,
	};

	enum class Color {
		Invalid = -1,
		Black   = 30,
		Red     = 31,
		Green   = 32,
		Yellow  = 33,
		Blue    = 34,
		Magenta = 35,
		Cyan    = 36,
		White   = 37,
		Default = 39,
	};

	enum class EraseExtents {
		Invalid = -1,
		End     = 0,
		Begin   = 1,
		Entire  = 2,
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
	} t_detail = {};

	struct ColorDetail {
		Color color = Color::Invalid;
		bool is_light = false;
	} c_detail = {};

	EraseExtents e_extent = EraseExtents::Invalid;

	static inline const std::string light_prefix = "light-";

	// clang-format off
	static inline const std::unordered_map<std::string, TagDetail> tags = {
	        {"color",   {Group::Colors,  Type::Color}},
	        {"bgcolor", {Group::Colors,  Type::BGColor}},
	        {"erasel",  {Group::Erasers, Type::EraseL}},
	        {"erases",  {Group::Erasers, Type::EraseS}},
	        {"i",       {Group::Styles,  Type::It}},
	        {"b",       {Group::Styles,  Type::Bold}},
	        {"u",       {Group::Styles,  Type::Ul}},
	        {"s",       {Group::Styles,  Type::Strike}},
	        {"blink",   {Group::Styles,  Type::Blink}},
	        {"dim",     {Group::Styles,  Type::Dim}},
	        {"hidden",  {Group::Styles,  Type::Hidden}},
	        {"inverse", {Group::Styles,  Type::Inverse}},
	        {"reset",   {Group::Misc,    Type::Reset}},
	};

	static inline const std::unordered_map<std::string, ColorDetail> color_values = {
	        // Default colours
	        {"default",       {Color::Default, false}},
	        {"light-default", {Color::Default, true}},

	        // Low intensity CGA colours
	        {"black",         {Color::Black,   false}},
	        {"blue",          {Color::Blue,    false}},
	        {"green",         {Color::Green,   false}},
	        {"cyan",          {Color::Cyan,    false}},
	        {"red",           {Color::Red,     false}},
	        {"magenta",       {Color::Magenta, false}},
	        {"brown",         {Color::Yellow,  false}},
	        {"light-gray",    {Color::White,   false}},

	        // High intensity CGA colours
	        {"dark-gray",     {Color::Black,   true}},
	        {"light-blue",    {Color::Blue,    true}},
	        {"light-green",   {Color::Green,   true}},
	        {"light-cyan",    {Color::Cyan,    true}},
	        {"light-red",     {Color::Red,     true}},
	        {"light-magenta", {Color::Magenta, true}},
	        {"yellow",        {Color::Yellow,  true}},
	        {"white",         {Color::White,   true}},
	};
	// clang-format on

	static inline const std::unordered_map<std::string, EraseExtents> eraser_extents = {
	        {"end", EraseExtents::End},
	        {"begin", EraseExtents::Begin},
	        {"entire", EraseExtents::Entire},
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

bool Tag::parse_color_val(const std::string& val)
{
	if (!contains(color_values, val)) {
		return false;
	}
	c_detail = color_values.at(val);
	return true;
}

bool Tag::parse_erase_val(const std::string& val)
{
	if (!contains(eraser_extents, val)) {
		return false;
	}
	e_extent = eraser_extents.at(val);
	return true;
}

int Tag::ansi_num() const
{
	switch (t_detail.group) {
	case Group::Colors: return enum_val(c_detail.color); break;
	case Group::Erasers: return enum_val(e_extent); break;
	default: return enum_val(t_detail.type); break;
	}
	return 0;
}

/*
 * Regular expression to match tags
 *
 * The following is an example with group numbers:
 *               _____2_____
 *               |		   |
 * This color is [color=light-red] red
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
		             (tag.color_is_light() ? ";1" : ""));
		break;

	case Tag::Group::Erasers:
		safe_sprintf(ansi_code, "\033[%d%s", ansi_num,
		             type == Tag::Type::EraseL ? "K" : "J");
		break;

	case Tag::Group::Styles:
		// "closing" tags have ascii codes +20
		if (tag.closed()) {
			ansi_num += 20 + (type == Tag::Type::Bold ? 1 : 0);
		}
		safe_sprintf(ansi_code, "\033[%dm", ansi_num);
		break;

	default: safe_sprintf(ansi_code, "\033[%dm", ansi_num); break;
	}
	return ansi_code;
}

std::string convert_ansi_markup(const std::string &str)
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

std::string strip_ansi_markup(const std::string& str)
{
	std::string result;
	const char* begin      = str.c_str();
	const char* last_match = str.c_str();
	std::cmatch m;

	while (std::regex_search(begin, m, markup)) {
		// Copy text before current match to output string
		result += m.prefix().str();

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
