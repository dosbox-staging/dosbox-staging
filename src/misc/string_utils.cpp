/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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
#include <vector>
#include <string>

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

std::vector<std::string> split(const std::string &seq, const char delim)
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

std::vector<std::string> split(const std::string &seq)
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

char *strip_word(char *&line)
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

bool ends_with(const std::string &str, const std::string &suffix) noexcept
{
	return (str.size() >= suffix.size() &&
	        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

void clear_language_if_default(std::string &l)
{
	lowcase(l);
	if (l.size() < 2 || starts_with("c.", l) || l == "posix") {
		l.clear();
	}
}
