/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "dosbox.h"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>

#include "std_filesystem.h"
#include <string>
#include <unordered_map>

#include "ansi_code_markup.h"
#include "checks.h"
#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"
#include "unicode.h"

#define LINE_IN_MAXLEN 2048

CHECK_NARROWING();

static const char *msg_not_found = "Message not Found!\n";

class Message {
private:
	std::string markup_msg = {};
	std::string rendered_msg = {};

	std::map<uint16_t, std::string> rendered_msg_by_codepage = {};

	static std::string to_dos(const std::string& in_str, const uint16_t code_page)
	{
		return utf8_to_dos(in_str,
		                   DosStringConvertMode::WithControlCodes,
		                   UnicodeFallback::Box,
		                   code_page);
	}

	const char* CachedRenderString(const std::string& msg,
	                               std::map<uint16_t, std::string>& output_msg_by_codepage)
	{
		assert(msg.length());
		const uint16_t cp = get_utf8_code_page();
		if (output_msg_by_codepage[cp].empty()) {
			output_msg_by_codepage[cp] = to_dos(msg, cp);
			assert(output_msg_by_codepage[cp].length());
		}

		return output_msg_by_codepage[cp].c_str();
	}

public:
	Message() = delete;
	Message(const char *markup)
	{
		Set(markup);
	}

	const char *GetRaw()
	{
		assert(markup_msg.length());
		return markup_msg.c_str();
	}

	const char *GetRendered()
	{
		assert(markup_msg.length());
		if (rendered_msg.empty())
			rendered_msg = convert_ansi_markup(markup_msg.c_str());

		assert(rendered_msg.length());
		const uint16_t cp = get_utf8_code_page();
		if (rendered_msg_by_codepage[cp].empty()) {
			rendered_msg_by_codepage[cp] = to_dos(rendered_msg, cp);
			assert(rendered_msg_by_codepage[cp].length());
		}

		return rendered_msg_by_codepage[cp].c_str();
	}

	void Set(const char *markup)
	{
		assert(markup);
		markup_msg = markup;

		rendered_msg.clear();
		rendered_msg_by_codepage.clear();
	}
};

static std::unordered_map<std::string, Message> messages;
static std::deque<std::string> messages_order;

// Add the message if it doesn't exist yet
void MSG_Add(const char* name, const char* markup_msg)
{
	const auto pair = messages.try_emplace(name, markup_msg);
	if (pair.second) { // if the insertion was successful
		messages_order.emplace_back(name);
	} else if ((control->GetLanguage() == "en" || control->GetLanguage().empty()) &&
	           strcmp(pair.first->second.GetRaw(), markup_msg) != 0) {
		// Detect duplicates in the English language
		LOG_WARNING("LANG: Duplicate text added for message '%s'. Second instance is ignored.",
		            name);
	} else {
		// Duplicate ID definitions most likely occured by adding the help in the code
		// after it was added by a help-file.
	}
}

// Replace existing or add if it doesn't exist
static void msg_replace(const char *name, const char *markup_msg)
{
	auto it = messages.find(name);
	if (it == messages.end())
		MSG_Add(name, markup_msg);
	else
		it->second.Set(markup_msg);
}

static bool load_message_file(const std_fs::path &filename)
{
	if (filename.empty())
		return false;

	if (!path_exists(filename) || !is_readable(filename)) {
		LOG_MSG("LANG: Language file %s not found, skipping",
		        filename.string().c_str());
		return false;
	}

	FILE *mfile = fopen(filename.string().c_str(), "rt");
	if (!mfile) {
		LOG_MSG("LANG: Failed opening language file: %s, skipping",
		        filename.string().c_str());
		return false;
	}

	char linein[LINE_IN_MAXLEN];
	char name[LINE_IN_MAXLEN];
	char message[LINE_IN_MAXLEN * 10];
	/* Start out with empty strings */
	name[0] = 0;
	message[0] = 0;
	while (fgets(linein, LINE_IN_MAXLEN, mfile) != nullptr) {
		/* Parse the read line */
		/* First remove characters 10 and 13 from the line */
		char * parser=linein;
		char * writer=linein;
		while (*parser) {
			if (*parser!=10 && *parser!=13) {
				*writer++=*parser;
			}
			parser++;
		}
		*writer=0;
		/* New message name */
		if (linein[0]==':') {
			message[0] = 0;
			safe_strcpy(name, linein + 1);
			/* End of message marker */
		} else if (linein[0]=='.') {
			/* Replace/Add the message to the internal languagefile */
			/* Remove last newline (marker is \n.\n) */
			size_t ll = safe_strlen(message);
			// This second if should not be needed, but better be safe.
			if (ll && message[ll - 1] == '\n')
				message[ll - 1] = 0;
			msg_replace(name, message);
		} else {
			/* Normal message to be added */
			safe_strcat(message, linein);
			safe_strcat(message, "\n");
		}
	}
	fclose(mfile);
	LOG_MSG("LANG: Loaded language file: %s", filename.string().c_str());
	return true;
}

const char* MSG_Get(const char* requested_name)
{
	const auto it = messages.find(requested_name);
	if (it != messages.end()) {
		return it->second.GetRendered();
	}
	LOG_WARNING("LANG: Message '%s' not found", requested_name);
	return msg_not_found;
}

const char* MSG_GetRaw(const char* requested_name)
{
	const auto it = messages.find(requested_name);
	if (it != messages.end()) {
		return it->second.GetRaw();
	}
	LOG_WARNING("LANG: Message '%s' not found", requested_name);
	return msg_not_found;
}

bool MSG_Exists(const char *requested_name)
{
	return contains(messages, requested_name);
}

// Write the names and messages (in the order they were added) to the given location
bool MSG_Write(const char * location) {
	FILE *out = fopen(location, "w+t");
	if (!out)
		return false;

	for (const auto &name : messages_order)
		fprintf(out, ":%s\n%s\n.\n", name.c_str(), messages.at(name).GetRaw());

	fclose(out);
	return true;
}

// MSG_Init loads the requested language provided on the command line or
// from the language = conf setting.

// 1. The provided language can be an exact filename and path to the lng
//    file, which is the traditionnal method to load a language file.

// 2. It also supports the more convenient syntax without needing to provide a
//    filename or path: `-lang ru`. In this case, it constructs a path into the
//    platform's config path/translations/<lang>[.utf8].lng.

void MSG_Init([[maybe_unused]] Section_prop *section)
{
	// TODO: After migration to C++20 try to switch to constexpr
	static const std_fs::path subdir   = "translations";
	static const std::string extension = ".lng";

	const auto lang = control->GetLanguage();

	// If the language is english, then use the internal message
	if (lang.empty() || lang.starts_with("en")) {
		LOG_MSG("LANG: Using internal English language messages");
		return;
	}

	bool result = false;
	if (lang.ends_with(".lng"))
		result = load_message_file(get_resource_path(subdir, lang));
	else
		// If a short-hand name was provided then add the file extension
		result = load_message_file(get_resource_path(subdir, lang + extension));

	if (result)
		return;

	// If we got here, then the language was not found
	LOG_WARNING("LANG: The '%s' language resource file could not be loaded, using internal English messages",
	            lang.c_str());
}
