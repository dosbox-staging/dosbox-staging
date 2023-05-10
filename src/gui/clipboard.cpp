/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include "clipboard.h"

#include "checks.h"
#include "control.h"
#include "logging.h"
#include "string_utils.h"

#include <SDL.h>

CHECK_NARROWING();


enum class ContentType {
	None,
	Text,
	Binary
};

static struct Clipboard {
	ContentType content_type = ContentType::None;

	// Content - text
	std::string text_utf8   = {}; // if content is text, otherwise empty
	std::string text_dos    = {}; // only set when pasting text from DOS
	uint16_t text_code_page = 0;

	// Content - binary
	std::vector<uint8_t> binary = {};

	// Host content mirror
	std::string host_text_utf8 = {};

	void clear_content()
	{
		content_type = ContentType::None;
		text_utf8.clear();
		text_dos.clear();
		text_code_page = 0;
		binary.clear();
		// Do not clear 'host_text_utf8' - it is crucial to keep it
		// intact for proper synchronization with host OS clipboard
	}

} clipboard;

// ***************************************************************************
// Host clipboard synchronization
// ***************************************************************************

static bool text_equals(const std::string &str_1, const std::string &str_2)
{
	// String compare, insensitive to end of the line encoding

	size_t index_1 = 0;
	size_t index_2 = 0;

	auto get_character =[](const std::string &str, size_t &index) {
		const char result = (str[index] == '\r') ? '\n' : str[index];
		if ((index + 1 < str.size()) &&
		    (str[index] == '\r' && str[index + 1] == '\n') &&
		    (str[index] == '\n' && str[index + 1] == '\r')) {
		    	// End of the line encoded as '\r\n' or '\n\r'
			++index;
		}

		++index;
		return result;
	};

	while ((index_1 < str_1.size()) && (index_2 < str_2.size())) {
		if (get_character(str_1, index_1) !=
		    get_character(str_2, index_2)) {
			return false;
		}
	}

	return (index_1 == str_1.size()) && (index_2 == str_2.size());
}

static void maybe_fetch_text_from_host()
{
	if (control->SecureMode()) {
		// Do not fetch anything from host clipboard in secure mode
		clipboard.host_text_utf8.clear();
		return;
	}

	bool host_content_changed    = false;
	std::string new_host_content = {};

	if (SDL_FALSE == SDL_HasClipboardText()) {
		// Host has no text in the clipboard
		if (!clipboard.host_text_utf8.empty()) {
			host_content_changed = true;
			clipboard.host_text_utf8.clear();
		}
	} else {
		// Host has a text in the clipboard
		new_host_content = SDL_GetClipboardText();
		if (!text_equals(new_host_content, clipboard.host_text_utf8)) {
			host_content_changed = true;
		}
	}

	if (host_content_changed) {
		clipboard.clear_content();
		clipboard.host_text_utf8 = new_host_content;
		clipboard.text_utf8      = new_host_content;
		clipboard.content_type   = ContentType::Text;
	}
}

static void maybe_push_text_to_host()
{
	if (control->SecureMode()) {
		// Do not push anything to host clipboard in secure mode
		return;
	}

	// Convert end-of-the-line encoding to host native

#if defined(WIN32)
	const std::string newline = "\r\n";
#else
	const std::string newline = "\n";
#endif

	std::string converted = {};
	converted.reserve(clipboard.text_utf8.size());
	for (size_t index = 0; index < clipboard.text_utf8.size(); ++index) {
		const auto character = clipboard.text_utf8[index];
		if (character == '\r') {
			converted += newline;
			if (index + 1 < clipboard.text_utf8.size() &&
			    clipboard.text_utf8[index + 1] == '\n') {
				++index;
			}
		} else if (character == '\n') {
			converted += newline;
			if (index + 1 < clipboard.text_utf8.size() &&
			    clipboard.text_utf8[index + 1] == '\r') {
				++index;
			}
		} else {
			converted.push_back(character);
		}
	}

	// Paste text to the clipboard

	if (0 != SDL_SetClipboardText(converted.c_str())) {
		LOG_WARNING("SDL: Clipboard error '%s'", SDL_GetError());
	} else {
		clipboard.host_text_utf8 = clipboard.text_utf8;
	}
}

// ***************************************************************************
// External interface - text clipboard support
// ***************************************************************************

bool clipboard_has_text()
{
	maybe_fetch_text_from_host();
	return (clipboard.content_type == ContentType::Text);
}

void clipboard_copy_text_dos(const std::string &content)
{
	clipboard_copy_text_dos(content, get_utf8_code_page());
}

void clipboard_copy_text_dos(const std::string &content,
                             const uint16_t code_page)
{
	clipboard.clear_content();
	if (!content.empty()) {
		clipboard.content_type   = ContentType::Text;
		clipboard.text_dos       = content;
		clipboard.text_code_page = code_page;
		dos_to_utf8(clipboard.text_dos, clipboard.text_utf8,
			    DosStringType::WithControlCodes);
	}
	maybe_push_text_to_host();
}

void clipboard_copy_text_utf8(const std::string &content)
{
	maybe_fetch_text_from_host();
	clipboard.clear_content();
	if (!content.empty()) {
		clipboard.content_type = ContentType::Text;
		clipboard.text_utf8    = content;
	}
	maybe_push_text_to_host();
}

std::string clipboard_paste_text_dos()
{
	return clipboard_paste_text_dos(get_utf8_code_page());
}

std::string clipboard_paste_text_dos(const uint16_t code_page)
{
	maybe_fetch_text_from_host();

	if (clipboard.content_type != ContentType::Text ||
	    clipboard.text_utf8.empty()) {
		return {};
	}

	if (is_utf8_code_page_duplicate(code_page, clipboard.text_code_page) &&
	    !clipboard.text_dos.empty()) {
		return clipboard.text_dos;
	}

	std::string output = {};
	utf8_to_dos(clipboard.text_utf8, output, UnicodeFallback::Simple);
	return output;
}

std::string clipboard_paste_text_utf8()
{
	maybe_fetch_text_from_host();
	return clipboard.text_utf8;
}

// ***************************************************************************
// External interface - binary clipboard support
// ***************************************************************************

bool clipboard_has_binary()
{
	maybe_fetch_text_from_host();
	return (clipboard.content_type == ContentType::Binary);
}

void clipboard_copy_binary(const std::vector<uint8_t> &content)
{
	maybe_fetch_text_from_host();
	clipboard.clear_content();
	if (!content.empty()) {
		clipboard.content_type = ContentType::Binary;
		clipboard.binary       = content;
	}
	maybe_push_text_to_host();
}

std::vector<uint8_t> clipboard_paste_binary()
{
	maybe_fetch_text_from_host();
	return clipboard.binary;
}
