// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/string_utils.h"

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

/**
 * @brief Removes leading whitespace from a std::string in place.
 *
 * This function modifies the input string by erasing all characters
 * from the beginning until a non-whitespace character is found.
 *
 * @param str The string object to be modified.
 */
void ltrim(std::string& str)
{
	str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int c) {
		          return !isspace(c);
	          }));
}

/**
 * @brief Advances a pointer past leading whitespace.
 *
 * This function finds the first non-whitespace character in the provided
 * C-string and returns a pointer to it.
 *
 * @note This function does not modify the memory of the string; it simply
 * returns a pointer offset from the original start.
 *
 * @param str The input C-string.
 * @return char* A pointer to the first non-whitespace character.
 */
char* ltrim(char* str)
{
	while (*str && isspace(*reinterpret_cast<unsigned char *>(str)))
		str++;
	return str;
}

/**
 * @brief Removes trailing whitespace from a C-string.
 *
 * This function iterates backwards from the end of the string and
 * replaces trailing whitespace characters with null terminators ('\0').
 *
 * @note This function modifies the input buffer.
 *
 * @param str The input C-string to modify.
 * @return char* A pointer to the start of the original string.
 */
char* rtrim(char* str)
{
	char *p;
	p = strchr(str, '\0');
	while (--p >= str && isspace(*reinterpret_cast<unsigned char *>(p))) {
	};
	p[1] = '\0';
	return str;
}

/**
 * @brief Trims both leading and trailing whitespace from a C-string.
 *
 * This function first truncates trailing whitespace (modifying the buffer),
 * and then calculates a pointer to the first non-whitespace character
 * (skipping leading whitespace).
 *
 * @param str The input C-string to trim.
 * @return char* A pointer to the trimmed substring within the original buffer.
 */
char* trim(char* str)
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
	auto to_upper = [](const int character) {
		return std::toupper(character);
	};
	std::transform(str.begin(), str.end(), str.begin(), to_upper);
}

void lowcase(std::string &str)
{
	auto to_lower = [](const int character) {
		return std::tolower(character);
	};
	std::transform(str.begin(), str.end(), str.begin(), to_lower);
}

std::string upcase(const std::string_view sv)
{
	std::string res(sv);
	upcase(res);
	return res;
}

std::string lowcase(const std::string_view sv)
{
	std::string res(sv);
	lowcase(res);
	return res;
}

std::string replace(const std::string &str, char old_char, char new_char) noexcept
{
	std::string new_str = str;
	std::replace(new_str.begin(), new_str.end(), old_char, new_char);
	return new_str;
}

/**
 * @brief Trims specific characters from both ends of a std::string.
 *
 * This function removes all leading and trailing occurrences of the characters
 * specified in @p trim_chars from the input string @p str. The operation is
 * performed in-place.
 *
 * @param str The string to be modified.
 * @param trim_chars A set of characters to remove (defaults to nothing if
 * empty). Passed as a string_view for efficiency.
 *
 * @note If the string consists entirely of characters found in @p trim_chars,
 * the string will be cleared (become empty).
 */
void trim(std::string& str, const std::string_view trim_chars)
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
	std::erase_if(str, [](unsigned char c) { return std::ispunct(c); });
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

// Search for the needle in the haystack, case insensitive.
bool find_in_case_insensitive(const std::string &needle, const std::string &haystack)
{
	const auto it = std::search(haystack.begin(), haystack.end(),
	                            needle.begin(), needle.end(),
	                            [](char ch1, char ch2) {
		                            return std::toupper(ch1) ==
		                                   std::toupper(ch2);
	                            });
	return (it != haystack.end());
}

std::string host_eol()
{
#if defined(WIN32)
	return "\r\n";
#else
	return "\n";
#endif
}

std::string replace_eol(const std::string& str, const std::string& new_eol)
{
	std::string result = {};
	result.reserve(str.size());

	for (size_t index = 0; index < str.size(); ++index) {
		const auto character = str[index];
		if (character == '\r') {
			result += new_eol;
			if (index + 1 < str.size() && str[index + 1] == '\n') {
				++index;
			}
		} else if (character == '\n') {
			result += new_eol;
			if (index + 1 < str.size() && str[index + 1] == '\r') {
				++index;
			}
		} else {
			result.push_back(character);
		}
	}

	return result;
}

bool is_text_equal(const std::string& str_1, const std::string& str_2)
{
	size_t index_1 = 0;
	size_t index_2 = 0;

	auto get_character = [](const std::string& str, size_t& index) {
		const char result = (str[index] == '\r') ? '\n' : str[index];
		if ((index + 1 < str.size()) &&
		    ((str[index] == '\r' && str[index + 1] == '\n') ||
		     (str[index] == '\n' && str[index + 1] == '\r'))) {
			// End of the line encoded as '\r\n' or '\n\r'
			++index;
		}

		++index;
		return result;
	};

	while ((index_1 < str_1.size()) && (index_2 < str_2.size())) {
		if (get_character(str_1, index_1) != get_character(str_2, index_2)) {
			return false;
		}
	}

	return (index_1 == str_1.size()) && (index_2 == str_2.size());
}

/**
 * @brief Write a string into a fixed-width buffer and pad a specified character
 *
 * This function takes the contents of @p str,  ensuring that the total length
 * is exactly @p length bytes. If @p str is shorter than @p length, the
 * remaining space is filled with the character specified by @p pad_char.
 * If @p str is longer than @p length, it is truncated to fit.
 *
 * @param str The input string
 * @param length The desired length of the output string
 * @param pad_char The character to use for padding if @p str is shorter than @p
 * length
 *
 */
std::string right_pad(const std::string& str, int length, char pad_char)
{
	auto temp = str;
	temp.resize(length, pad_char);
	return temp;
}
