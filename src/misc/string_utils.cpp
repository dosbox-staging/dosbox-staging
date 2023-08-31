/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#include "string_utils.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

bool is_hex_digits(const std::string_view s) noexcept
{
	for (const auto ch : s) {
		if (!isxdigit(ch))
			return false;
	}
	return true;
}

bool is_digits(const std::string_view s) noexcept
{
	for (const auto ch : s) {
		if (!isdigit(ch))
			return false;
	}
	return true;
}

void strreplace(char *str, char o, char n)
{
	while (*str) {
		if (*str == o)
			*str = n;
		str++;
	}
}

char *ltrim(char *str)
{
	while (*str && isspace(*reinterpret_cast<unsigned char *>(str)))
		str++;
	return str;
}

char *rtrim(char *str)
{
	char *p;
	p = strchr(str, '\0');
	while (--p >= str && isspace(*reinterpret_cast<unsigned char *>(p))) {
	};
	p[1] = '\0';
	return str;
}

char *trim(char *str)
{
	return ltrim(rtrim(str));
}

char *upcase(char *str)
{
	for (char *idx = str; *idx; idx++)
		*idx = toupper(*reinterpret_cast<unsigned char *>(idx));
	return str;
}

char *lowcase(char *str)
{
	for (char *idx = str; *idx; idx++)
		*idx = tolower(*reinterpret_cast<unsigned char *>(idx));
	return str;
}

void upcase(std::string &str)
{
	int (*tf)(int) = std::toupper;
	std::transform(str.begin(), str.end(), str.begin(), tf);
}

void lowcase(std::string &str)
{
	int (*tf)(int) = std::tolower;
	std::transform(str.begin(), str.end(), str.begin(), tf);
}

std::string replace(const std::string &str, char old_char, char new_char) noexcept
{
	std::string new_str = str;
	for (char &c : new_str)
		if (c == old_char)
			c = new_char;
	return str;
}

void trim(std::string &str, const char trim_chars[])
{
	const auto empty_pfx = str.find_first_not_of(trim_chars);
	if (empty_pfx == std::string::npos) {
		str.clear(); // whole string is filled with trim_chars
		return;
	}
	const auto empty_sfx = str.find_last_not_of(trim_chars);
	str.erase(empty_sfx + 1);
	str.erase(0, empty_pfx);
}

std::vector<std::string> split(const std::string_view seq, const char delim)
{
	std::vector<std::string> words;
	if (seq.empty())
		return words;

	// count delimeters to reserve space in our vector of words
	const size_t n = 1u + std::count(seq.begin(), seq.end(), delim);
	words.reserve(n);

	std::string::size_type head = 0;
	while (head != std::string::npos) {
		const auto tail     = seq.find_first_of(delim, head);
		const auto word_len = tail - head;
		words.emplace_back(seq.substr(head, word_len));
		if (tail == std::string::npos) {
			break;
		}
		head += word_len + 1;
	}

	// did we reserve the exact space needed?
	assert(n == words.size());

	return words;
}

std::vector<std::string> split(const std::string_view seq)
{
	std::vector<std::string> words;
	if (seq.empty())
		return words;

	constexpr auto whitespace = " \f\n\r\t\v";

	// count words to reserve space in our vector
	size_t n  = 0;
	auto head = seq.find_first_not_of(whitespace, 0);
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(whitespace, head);
		head            = seq.find_first_not_of(whitespace, tail);
		++n;
	}
	words.reserve(n);

	// populate the vector with the words
	head = seq.find_first_not_of(whitespace, 0);
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(whitespace, head);
		words.emplace_back(seq.substr(head, tail - head));
		head = seq.find_first_not_of(whitespace, tail);
	}

	// did we reserve the exact space needed?
	assert(n == words.size());

	return words;
}

std::string join_with_commas(const std::vector<std::string>& items,
                             const std::string_view and_conjunction,
                             const std::string_view end_punctuation)
{
	const auto num_items = items.size();

	std::string result = {};

	const auto and_pair = std::string(" ") + and_conjunction.data() + " ";
	const auto and_multi = std::string(", ") + and_conjunction.data() + " ";

	std::string separator = (num_items == 2u) ? and_pair : ", ";

	size_t item_num = 1;
	for (const auto& item : items) {
		assert(!item.empty());
		result += item;
		result += (item_num == num_items) ? end_punctuation : separator;
		separator = (item_num + 2u == num_items) ? and_multi : separator;
		++item_num;
	}
	return result;
}

bool ciequals(const char a, const char b)
{
	return tolower(a) == tolower(b);
}

bool natural_compare(const std::string& a_str, const std::string& b_str)
{
	auto parse_num = [](auto& it, const auto it_end) {
		int num = 0;
		while (it != it_end && isdigit(*it)) {
			num = num * 10 + (*it - '0');
			++it;
		}
		return num;
	};
	auto a = a_str.begin();
	auto b = b_str.begin();

	const auto a_end = a_str.end();
	const auto b_end = b_str.end();

	while (a != a_end && b != b_end) {
		const auto found_nums = isdigit(*a) && isdigit(*b);
		const auto a_val = found_nums ? parse_num(a, a_end) : tolower(*a++);
		const auto b_val = found_nums ? parse_num(b, b_end) : tolower(*b++);
		if (a_val != b_val) {
			return a_val < b_val;
		}
	}
	// the overlapping strings match, so finally check if A is shorter
	return a == a_end && b != b_end;
}

char* strip_word(char*& line)
{
	char *scan = line;
	scan       = ltrim(scan);
	if (*scan == '"') {
		char *end_quote = strchr(scan + 1, '"');
		if (end_quote) {
			*end_quote = 0;
			line       = ltrim(++end_quote);
			return (scan + 1);
		}
	}
	char *begin = scan;
	for (char c = *scan; (c = *scan); scan++) {
		if (isspace(*reinterpret_cast<unsigned char *>(&c))) {
			*scan++ = 0;
			break;
		}
	}
	line = scan;
	return begin;
}

void strip_punctuation(std::string &str)
{
	str.erase(std::remove_if(str.begin(),
	                         str.end(),
	                         [](unsigned char c) { return std::ispunct(c); }),
	          str.end());
}

// TODO in C++20: replace with str.starts_with(prefix)
bool starts_with(const std::string_view str, const std::string_view prefix) noexcept
{
	if (prefix.length() > str.length()) {
		return false;
	}
	return std::equal(prefix.begin(), prefix.end(), str.begin());
}

// TODO in C++20: replace with str.ends_with(suffix)
bool ends_with(const std::string_view str, const std::string_view suffix) noexcept
{
	if (suffix.length() > str.length()) {
		return false;
	}
	return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

std::string strip_prefix(const std::string& str, const std::string& prefix) noexcept
{
	if (starts_with(str, prefix)) {
		return str.substr(prefix.size());
	}
	return str;
}

std::string strip_suffix(const std::string& str, const std::string& suffix) noexcept
{
	if (ends_with(str, suffix)) {
		return str.substr(0, str.size() - suffix.size());
	}
	return str;
}

void clear_language_if_default(std::string &l)
{
	lowcase(l);
	if (l.size() < 2 || starts_with(l, "c.") || l == "posix") {
		l.clear();
	}
}

std::optional<float> parse_value(const std::string_view s,
                                 const float min_value, const float max_value)
{
	// parse_value can check if a string holds a number (or not), so we expect
	// exceptions and return an empty result to indicate conversion status.
	try {
		if (!s.empty()) {
			return std::clamp(std::stof(s.data()), min_value, max_value);
		}
		// Note: stof can throw invalid_argument and out_of_range
	} catch (const std::invalid_argument &) {
		// do nothing, we expect these
	} catch (const std::out_of_range &) {
		// do nothing, we expect these
	}
	return {}; // empty
}

std::optional<float> parse_percentage(const std::string_view s)
{
	constexpr auto min_percentage = 0.0f;
	constexpr auto max_percentage = 100.0f;
	return parse_value(s, min_percentage, max_percentage);
}

std::optional<float> parse_prefixed_value(const char prefix, const std::string &s,
                                          const float min_value, const float max_value)
{
	if (s.size() <= 1 || !ciequals(s[0], prefix))
		return {};

	return parse_value(s.substr(1), min_value, max_value);
}

std::optional<float> parse_prefixed_percentage(const char prefix, const std::string &s)
{
	constexpr auto min_percentage = 0.0f;
	constexpr auto max_percentage = 100.0f;
	return parse_prefixed_value(prefix, s, min_percentage, max_percentage);
}

std::optional<int> to_int(const std::string& value, const int base)
{
	try {
		// Do not store number of characters processed
		constexpr std::size_t* pos = nullptr;

		return std::stoi(value, pos, base);
	} catch (...) {
		return {};
	}
}
