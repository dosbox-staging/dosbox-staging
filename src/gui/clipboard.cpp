// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "clipboard.h"

#include "util/checks.h"
#include "config.h"
#include "logging.h"
#include "util/string_utils.h"
#include "unicode.h"

#include <SDL.h>

CHECK_NARROWING();

static struct Clipboard {
	// Content - text
	uint16_t text_code_page = 0;
	// Since DOS -> UTF-8 -> DOS string conversion won't necessarily produce
	// the same output as the original DOS-encoded text (some code pages
	// have multiple blank/unused characters, etc.), in case the text comes
	// from the DOS side, we keep both DOS and UTF-8 encoded strings - this
	// way the DOSBox can get precisely the same content as it has sent
	// to the clipboard as long as the code page did not change in the
	// meantime.
	std::string text_utf8 = {}; // always filled in if we contain a text
	std::string text_dos  = {}; // only set when pasting text from DOS

	// Host content mirror
	std::string host_text_utf8 = {};

	void clear_content()
	{
		text_utf8.clear();
		text_dos.clear();
		text_code_page = 0;
		// Do not clear the 'host_text_utf8' - it is crucial to keep it
		// intact for proper synchronization with host OS clipboard
	}

} clipboard;

// ***************************************************************************
// Host clipboard synchronization
// ***************************************************************************

static void maybe_fetch_text_from_host()
{
	if (control->SecureMode()) {
		// We won't need the host content for anything more
		clipboard.host_text_utf8.clear();
		// Do not fetch anything from the host clipboard in secure mode
		return;
	}

	bool has_host_content_changed = false;
	std::string new_host_text = {};

	if (SDL_FALSE == SDL_HasClipboardText()) {
		// Host has no text in the clipboard
		if (!clipboard.host_text_utf8.empty()) {
			has_host_content_changed = true;
		}
	} else {
		// Host has a text in the clipboard
		new_host_text = SDL_GetClipboardText();
		if (!is_text_equal(new_host_text, clipboard.host_text_utf8)) {
			has_host_content_changed = true;
		}
	}

	if (has_host_content_changed) {
		clipboard.clear_content();
		clipboard.host_text_utf8 = new_host_text;
		clipboard.text_utf8      = std::move(new_host_text);
	}
}

static void maybe_push_text_to_host()
{
	if (control->SecureMode()) {
		// We won't need the host content for anything more
		clipboard.host_text_utf8.clear();
		// Do not copy anything to host clipboard in secure mode
		return;
	}

	// Convert end-of-the-line markers to host native
	const auto converted = replace_eol(clipboard.text_utf8, host_eol());

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

bool CLIPBOARD_HasText()
{
	maybe_fetch_text_from_host();
	return !clipboard.text_utf8.empty();
}

void CLIPBOARD_CopyText(const std::string& content)
{
	CLIPBOARD_CopyText(content, get_utf8_code_page());
}

void CLIPBOARD_CopyText(const std::string& content, const uint16_t code_page)
{
	clipboard.clear_content();
	if (!content.empty()) {
		clipboard.text_dos       = content;
		clipboard.text_code_page = code_page;
		clipboard.text_utf8      = dos_to_utf8(clipboard.text_dos,
                                                  DosStringConvertMode::WithControlCodes);
	}
	maybe_push_text_to_host();
}

std::string CLIPBOARD_PasteText()
{
	return CLIPBOARD_PasteText(get_utf8_code_page());
}

std::string CLIPBOARD_PasteText(const uint16_t code_page)
{
	maybe_fetch_text_from_host();

	if (clipboard.text_utf8.empty()) {
		return {};
	}

	if (is_code_page_equal(code_page, clipboard.text_code_page) &&
	    !clipboard.text_dos.empty()) {
		return clipboard.text_dos;
	}

	return utf8_to_dos(clipboard.text_utf8,
	                   DosStringConvertMode::WithControlCodes,
	                   UnicodeFallback::Simple,
	                   code_page);
}
