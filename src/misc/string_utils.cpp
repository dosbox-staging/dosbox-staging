/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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
	return std::all_of(s.begin(), s.end(), &isxdigit);
}

bool is_digits(const std::string_view s) noexcept
{
	return std::all_of(s.begin(), s.end(), &isdigit);
}

void strreplace(char *str, char o, char n)
{
	while (*str) {
		if (*str == o)
			*str = n;
		str++;
	}
}

void ltrim(std::string &str) {
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int c) {
		          return !isspace(c);
	          }));
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
	std::replace(new_str.begin(), new_str.end(), old_char, new_char);
	return new_str;
}

void trim(std::string &str, const std::string_view trim_chars)
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

std::vector<std::string> split_with_empties(const std::string_view seq, const char delim)
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

std::vector<std::string> split(const std::string_view seq, const std::string_view delims)
{
	std::vector<std::string> words;
	if (seq.empty()) {
		return words;
	}

	// count words to reserve space in our vector
	size_t n  = 0;
	auto head = seq.find_first_not_of(delims, 0);
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(delims, head);
		head            = seq.find_first_not_of(delims, tail);
		++n;
	}
	words.reserve(n);

	// populate the vector with the words
	head = seq.find_first_not_of(delims, 0);
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(delims, head);
		words.emplace_back(seq.substr(head, tail - head));
		head = seq.find_first_not_of(delims, tail);
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

	// C++26 should add the missing operator+(std::string, std::string_view)
	const auto and_pair = std::string(" ").append(and_conjunction) + " ";
	const auto and_multi = std::string(", ").append(and_conjunction) + " ";

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

std::string strip_word(std::string& line)
{
	ltrim(line);
	if (line.empty()) {
		return "";
	}
	if (line[0] == '"') {
		size_t end_quote = line.find('"', 1);
		if (end_quote != std::string::npos) {
			const std::string word = line.substr(1, end_quote - 1);
			line.erase(0, end_quote + 1);
			ltrim(line);
			return word;
		}
	}
	auto end_word = std::find_if(line.begin(), line.end(), [](int c) {return isspace(c);});
	const std::string word(line.begin(), end_word);
	if (end_word != line.end()) {
		++end_word;
	}
	line.erase(line.begin(), end_word);
	return word;
}

void strip_punctuation(std::string &str)
{
	str.erase(std::remove_if(str.begin(),
	                         str.end(),
	                         [](unsigned char c) { return std::ispunct(c); }),
	          str.end());
}

std::string strip_prefix(const std::string_view str, const std::string_view prefix) noexcept
{
	if (str.starts_with(prefix)) {
		return std::string(str.substr(prefix.size()));
	}
	return std::string(str);
}

std::string strip_suffix(const std::string_view str, const std::string_view suffix) noexcept
{
	if (str.ends_with(suffix)) {
		return std::string(str.substr(0, str.size() - suffix.size()));
	}
	return std::string(str);
}

std::optional<float> parse_float(const std::string& s)
{
	// parse_float can check if a string holds a number (or not), so we
	// expect exceptions and return an empty result to indicate conversion
	// status.
	try {
		if (!s.empty()) {
			size_t num_chars_processed = 0;
			const auto number = std::stof(s, &num_chars_processed);
			if (s.size() == num_chars_processed) {
				return number;
			}
		}
		// Note: stof can throw invalid_argument and out_of_range
	} catch (const std::invalid_argument&) {
		// do nothing, we expect these
	} catch (const std::out_of_range&) {
		// do nothing, we expect these
	}
	return {};
}

std::optional<int> parse_int(const std::string& s, const int base)
{
	try {
		if (!s.empty()) {
			size_t num_chars_processed = 0;
			const auto number = std::stoi(s, &num_chars_processed, base);
			if (s.size() == num_chars_processed) {
				return number;
			}
		}
		// Note: stof can throw invalid_argument and out_of_range
	} catch (const std::invalid_argument&) {
		// do nothing, we expect these
	} catch (const std::out_of_range&) {
		// do nothing, we expect these
	}
	return {};
}

std::optional<float> parse_percentage(const std::string_view s,
                                      const bool is_percent_sign_optional)
{
	if (!is_percent_sign_optional) {
		if (!s.ends_with('%')) {
			return {};
		}
	}
	return {parse_float(strip_suffix(s, "%"))};
}

std::optional<float> parse_percentage_with_percent_sign(const std::string_view s)
{
	const auto is_percent_sign_optional = false;
	return parse_percentage(s, is_percent_sign_optional);
}

std::optional<float> parse_percentage_with_optional_percent_sign(const std::string_view s)
{
	const auto is_percent_sign_optional = true;
	return parse_percentage(s, is_percent_sign_optional);
}

std::string replace_all(const std::string& str, const std::string& from,
                        const std::string& to)
{
	auto new_str = str;

	size_t pos = 0;
	while ((pos = new_str.find(from, pos)) != std::string::npos) {
		new_str.replace(pos, from.length(), to);

		// Handles case where `to` is a substring of `from`
		pos += to.length();
	}
	return new_str;
}

