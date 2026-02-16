// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "misc/private/unicode_encodings.h"
#include "utils/byteorder.h"
#include "utils/checks.h"

#include <cassert>
#include <optional>

CHECK_NARROWING();

constexpr uint32_t LastCodePoint = 0x10ffff;

namespace Utf8 {

constexpr uint8_t Start2Bytes = 0b1'100'0000;
constexpr uint8_t Start3Bytes = 0b1'110'0000;
constexpr uint8_t Start4Bytes = 0b1'111'0000;
constexpr uint8_t Start5Bytes = 0b1'111'1000;
constexpr uint8_t Start6Bytes = 0b1'111'1100;
constexpr uint8_t NextByte    = 0b1'000'0000;

constexpr uint32_t EncodeThreshold2Bytes = 0x000080;
constexpr uint32_t EncodeThreshold3Bytes = 0x000800;
constexpr uint32_t EncodeThreshold4Bytes = 0x010000;

} // namespace Utf8

namespace Utf16 {

// Constants for byte order detection
constexpr char16_t ByteOrderMark     = 0xfeff;
constexpr char16_t ByteOrderReversed = 0xfffe;

// Constants for surrogate pairs encoding/decoding
constexpr char16_t LowSurrogateBase  = 0xd800;
constexpr char16_t HighSurrogateBase = 0xdc00;
constexpr char16_t PostSurrogateBase = 0xe000;

constexpr char16_t SurrogateRangeSize = HighSurrogateBase - LowSurrogateBase;

constexpr char32_t EncodeThresholdSurrogate = 0x10000;

} // namespace Utf16

static bool is_valid_codepoint(const char32_t code_point)
{
	if (code_point > LastCodePoint) {
		// Value over the Unicode code point range
		return false;
	}

	if (code_point >= static_cast<char32_t>(Utf16::LowSurrogateBase) &&
	    code_point < static_cast<char32_t>(Utf16::PostSurrogateBase)) {
		// These values are reserved for UTF-16 surrogate pairs encoding
		return false;
	}

	return true;
}

static bool is_valid_codepoint(const char16_t code_point)
{
	// A 16-bit code point can never exceed the maximum Unicode code point

	if (code_point >= Utf16::LowSurrogateBase &&
	    code_point < Utf16::PostSurrogateBase) {
		// These values are reserved for UTF-16 surrogate pairs encoding
		return false;
	}

	return true;
}

static void maybe_warn_decode(const size_t position,
                              const std::string& encoding,
                              bool& already_warned)
{
	if (already_warned) {
		return;
	}

	LOG_WARNING("UNICODE: Problem decoding %s string, position %lu",
	            encoding.c_str(),
	            static_cast<unsigned long>(position));

	already_warned = true;
}

// ***************************************************************************
// UTF-8 encoding support
// ***************************************************************************

// For UTF-8 encoding explanation see here:
// - https://www.codeproject.com/Articles/38242/Reading-UTF-8-with-C-streams
// - https://en.wikipedia.org/wiki/UTF-8#Encoding

std::u32string utf8_to_wide(const std::string& str)
{
	bool already_warned      = false;
	auto warn_decode_problem = [&already_warned](const size_t position) {
		maybe_warn_decode(position, "UTF-8", already_warned);
	};

	std::u32string str_out = {};
	str_out.reserve(str.size());

	for (size_t i = 0; i < str.size(); ++i) {
		const size_t remaining = str.size() - i - 1;

		const auto    byte_1 = static_cast<uint8_t>(str[i]);
		const uint8_t byte_2 = (remaining >= 1) ? static_cast<uint8_t>(str[i + 1]) : 0;
		const uint8_t byte_3 = (remaining >= 2) ? static_cast<uint8_t>(str[i + 2]) : 0;
		const uint8_t byte_4 = (remaining >= 3) ? static_cast<uint8_t>(str[i + 3]) : 0;

		// Check if the given byte is a subsequent byte of n-byte sequence
		auto is_next_byte = [](const uint8_t byte) {
			return (byte >= Utf8::NextByte) &&
			       (byte < Utf8::Start2Bytes);
		};

		// Advance up to n bytes, stop at the end of sequence
		auto advance = [&](const size_t n) {
			auto counter = std::min(remaining, n);
			while (counter--) {
				const auto byte = static_cast<uint8_t>(str[i + 1]);
				if (!is_next_byte(byte)) {
					break;
				}
				++i;
			}
		};

		// Retrieve the code point
		char32_t code_point = 0;

		const auto sequence_start_position  = i;
		std::optional<size_t> fail_position = {};

		if (byte_1 >= Utf8::Start6Bytes) {
			// 6-byte code point (>= 31 bits), not supported
			fail_position = i;
			advance(5);
		} else if (byte_1 >= Utf8::Start5Bytes) {
			// 5-byte code point (>= 26 bits), not supported
			fail_position = i;
			advance(4);
		} else if (byte_1 >= Utf8::Start4Bytes) {
			// Decode the 1st byte
			code_point = static_cast<uint8_t>(byte_1 - Utf8::Start4Bytes);
			// Decode the 2nd byte
			code_point = code_point << 6;
			if (is_next_byte(byte_2)) {
				++i;
				code_point += byte_2 - Utf8::NextByte;
			} else {
				fail_position = i + 1;
			}
			// Decode the 3rd byte
			code_point = code_point << 6;
			if (!fail_position && is_next_byte(byte_3)) {
				++i;
				code_point += byte_3 - Utf8::NextByte;
			} else if (!fail_position) {
				fail_position = i + 1;
			}
			// Decode the 4th byte
			code_point = code_point << 6;
			if (!fail_position && is_next_byte(byte_4)) {
				++i;
				code_point += byte_4 - Utf8::NextByte;
			} else if (!fail_position) {
				fail_position = i + 1;
			}
		} else if (byte_1 >= Utf8::Start3Bytes) {
			// Decode the 1st byte
			code_point = static_cast<uint8_t>(byte_1 - Utf8::Start3Bytes);
			// Decode the 2nd byte
			code_point = code_point << 6;
			if (is_next_byte(byte_2)) {
				++i;
				code_point += byte_2 - Utf8::NextByte;
			} else {
				fail_position = i + 1;
			}
			// Decode the 3rd byte
			code_point = code_point << 6;
			if (!fail_position && is_next_byte(byte_3)) {
				++i;
				code_point += byte_3 - Utf8::NextByte;
			} else if (!fail_position) {
				fail_position = i + 1;
			}
		} else if (byte_1 >= Utf8::Start2Bytes) {
			// Decode the 1st byte
			code_point = static_cast<uint8_t>(byte_1 - Utf8::Start2Bytes);
			// Decode the 2nd byte
			code_point = code_point << 6;
			if (is_next_byte(byte_2)) {
				++i;
				code_point += byte_2 - Utf8::NextByte;
			} else {
				fail_position = i + 1;
			}
		} else if (byte_1 < Utf8::NextByte) {
			// 1-byte code point, ASCII compatible
			code_point = byte_1;
		} else {
			fail_position = i; // not UTF8 encoding
		}

		// Check if decoding succeeded
		if (!fail_position && !(is_valid_codepoint(code_point))) {
			fail_position = sequence_start_position;
		}
		if (fail_position) {
			code_point = UnknownCharacter;
			warn_decode_problem(*fail_position);
		}

		// Push the decoded code point
		str_out.push_back(static_cast<char32_t>(code_point));
	}

	return str_out;
}

std::string wide_to_utf8(const std::u32string& str)
{
	std::string str_out = {};
	str_out.reserve(str.size() * 2);

	auto push = [&](const int value) {
		const auto byte = static_cast<uint8_t>(value);
		str_out.push_back(static_cast<char>(byte));
	};

	constexpr uint8_t ByteMask = 0b0'011'1111;

	for (const auto code_point : str) {
		if (!is_valid_codepoint(code_point)) {
			// No decoding routine should produce invalid values
			assert(false);
			str_out.push_back(static_cast<char16_t>(UnknownCharacter));
			continue;
		}

		if (code_point < Utf8::EncodeThreshold2Bytes) {
			// Encode using 1 byte
			push(code_point);
		} else if (code_point < Utf8::EncodeThreshold3Bytes) {
			// Encode using 2 bytes
			const auto to_byte_1 = code_point >> 6;
			const auto to_byte_2 = ByteMask & code_point;

			push(to_byte_1 | Utf8::Start2Bytes);
			push(to_byte_2 | Utf8::NextByte);
		} else if (code_point < Utf8::EncodeThreshold4Bytes) {
			// Encode using 3 bytes
			const auto to_byte_1 = code_point >> 12;
			const auto to_byte_2 = ByteMask & (code_point >> 6);
			const auto to_byte_3 = ByteMask & code_point;

			push(to_byte_1 | Utf8::Start3Bytes);
			push(to_byte_2 | Utf8::NextByte);
			push(to_byte_3 | Utf8::NextByte);
		} else {
			// Encode using 4 bytes
			const auto to_byte_1 = code_point >> 18;
			const auto to_byte_2 = ByteMask & (code_point >> 12);
			const auto to_byte_3 = ByteMask & (code_point >> 6);
			const auto to_byte_4 = ByteMask & code_point;

			push(to_byte_1 | Utf8::Start4Bytes);
			push(to_byte_2 | Utf8::NextByte);
			push(to_byte_3 | Utf8::NextByte);
			push(to_byte_4 | Utf8::NextByte);
		}
	}

	str_out.shrink_to_fit();
	return str_out;
}

// ***************************************************************************
// UTF-16 encoding support
// ***************************************************************************

// For UTF-16 encoding explanation see here:
// - https://en.wikipedia.org/wiki/UTF-16#Description

std::u32string utf16_to_wide(const std::u16string& str)
{
	bool already_warned      = false;
	auto warn_decode_problem = [&already_warned](const size_t position) {
		maybe_warn_decode(position, "UTF-16", already_warned);
	};

	std::u32string str_out = {};
	str_out.reserve(str.size());

	// If true, the input string endianness is opposite to the native one 
	bool swap_bytes = false;

	// Only set if decoding surrogate pairs
	std::optional<char16_t> previous = {};

	bool is_first_token = true;
	for (size_t i = 0; i < str.size(); ++i) {
		if (is_first_token) {
			is_first_token = false;

			// Check for BOM (byte order mark)
			if (str[i] == Utf16::ByteOrderMark) {
				continue;
			}
			if (str[i] == Utf16::ByteOrderReversed) {
				swap_bytes = true;
				continue;
			}
		}

		const auto token = swap_bytes ? bswap_u16(str[i]) : str[i];

		if (token < Utf16::LowSurrogateBase ||
		    token >= Utf16::PostSurrogateBase) {
			// Not a surrogate pair
			if (previous) {
				// Expected a second surrogate
				warn_decode_problem(i);
				str_out.push_back(
				        static_cast<char32_t>(UnknownCharacter));
				previous = {};
			}
			str_out.push_back(static_cast<char32_t>(token));
			continue;
		}

		// Handle the first surrogate
		if (token < Utf16::HighSurrogateBase) {
			if (previous) {
				// Expected a second surrogate
				warn_decode_problem(i);
				str_out.push_back(
				        static_cast<char32_t>(UnknownCharacter));
			}
			previous = token;
			continue;
		}

		// Handle the second surrogate
		if (!previous) {
			// We don't have the first surrogate
			warn_decode_problem(i);
			str_out.push_back(static_cast<char32_t>(UnknownCharacter));
			continue;
		}

		const auto result = (*previous - Utf16::LowSurrogateBase) *
		                    Utf16::SurrogateRangeSize +
		                    (token - Utf16::HighSurrogateBase) +
		                    Utf16::EncodeThresholdSurrogate;

		previous = {};
		str_out.push_back(static_cast<char32_t>(result));
	}

	if (previous) {
		// Expected a second surrogate, not end of data
		warn_decode_problem(str.size() - 1);
		str_out.push_back(static_cast<char32_t>(UnknownCharacter));
	}

	str_out.shrink_to_fit();
	return str_out;
}

std::u16string wide_to_utf16(const std::u32string& str)
{
	std::u16string str_out = {};
	str_out.reserve(str.size());

	for (const auto code_point : str) {
		if (!is_valid_codepoint(code_point)) {
			// No decoding routine should produce invalid values
			assert(false);
			str_out.push_back(static_cast<char16_t>(UnknownCharacter));
			continue;
		}

		if (code_point < Utf16::EncodeThresholdSurrogate) {
			// Just copy the value
			str_out.push_back(static_cast<char16_t>(code_point));
			continue;
		}

		// Encode using two surrogates
		const auto tmp = code_point - Utf16::EncodeThresholdSurrogate;

		const auto high = Utf16::LowSurrogateBase +
		                  (tmp / Utf16::SurrogateRangeSize);
		const auto low  = Utf16::HighSurrogateBase +
		                  (tmp % Utf16::SurrogateRangeSize);

		str_out.push_back(static_cast<char16_t>(high));
		str_out.push_back(static_cast<char16_t>(low));
	}

	str_out.shrink_to_fit();
	return str_out;
}

// ***************************************************************************
// UCS-2 encoding support
// ***************************************************************************

std::u32string ucs2_to_wide(const std::u16string& str)
{
	bool already_warned      = false;
	auto warn_decode_problem = [&already_warned](const size_t position) {
		maybe_warn_decode(position, "UCS-2", already_warned);
	};

	std::u32string str_out = {};
	str_out.reserve(str.size());

	for (size_t i = 0; i < str.size(); ++i) {
		if (!is_valid_codepoint(str[i])) {
			warn_decode_problem(i);
			str_out.push_back(static_cast<char32_t>(UnknownCharacter));
			continue;
		}

		str_out.push_back(static_cast<char32_t>(str[i]));
	}

	return str_out;
}

std::u16string wide_to_ucs2(const std::u32string& str)
{
	std::u16string str_out = {};
	str_out.reserve(str.size());

	for (const auto code_point : str) {
		if (!is_valid_codepoint(code_point)) {
			// No decoding routine should produce invalid values
			assert(false);
			str_out.push_back(static_cast<char16_t>(UnknownCharacter));
			continue;
		}

		if (code_point > UINT16_MAX) {
			// UCS-2 encoding does not allow this code point
			str_out.push_back(static_cast<char16_t>(UnknownCharacter));
			continue;
		}

		str_out.push_back(static_cast<char16_t>(code_point));
	}

	return str_out;
}
