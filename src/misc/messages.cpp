/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

#define LINE_IN_MAXLEN 2048

CHECK_NARROWING();

class Message {
private:
	std::string markup_msg = {};
	std::string rendered_msg = {};

	std::map<uint16_t, std::string> markup_cp_msg   = {};
	std::map<uint16_t, std::string> rendered_cp_msg = {};

	bool is_utf8_msg = false;

public:
	Message() = delete;
	Message(const char *markup, const bool is_utf8)
	{
		Set(markup, is_utf8);
	}

	const char *GetMarkup()
	{
		assert(markup_msg.length());

		if (!is_utf8_msg)
			return markup_msg.c_str();

		const uint16_t cp = UTF8_GetCodePage();
		if (markup_cp_msg[cp].empty()) {
			UTF8_RenderForDos(markup_msg, markup_cp_msg[cp], cp);
			assert(markup_cp_msg[cp].length());
		}
		return markup_cp_msg[cp].c_str();
	}

	const char *GetRendered()
	{
		assert(markup_msg.length());
		if (rendered_msg.empty())
			rendered_msg = convert_ansi_markup(markup_msg.c_str());
		assert(rendered_msg.length());

		if (!is_utf8_msg)
			return rendered_msg.c_str();

		const uint16_t cp = UTF8_GetCodePage();
		if (rendered_cp_msg[cp].empty()) {
			UTF8_RenderForDos(rendered_msg, rendered_cp_msg[cp], cp);
			assert(rendered_cp_msg[cp].length());
		}
		return rendered_cp_msg[cp].c_str();
	}

	void Set(const char *markup, const bool is_utf8)
	{
		assert(markup);
		markup_msg  = markup;
		is_utf8_msg = is_utf8;

		rendered_msg.clear();
		markup_cp_msg.clear();
		rendered_cp_msg.clear();
	}
};

static std::unordered_map<std::string, Message> messages;
static std::deque<std::string> messages_order;

// Add the message if it doesn't exist yet
void MSG_Add(const char *name, const char *markup_msg, bool is_utf8)
{
	const auto &pair = messages.try_emplace(name, markup_msg, is_utf8);
	if (pair.second) // if the insertion was successful
		messages_order.emplace_back(name);
}

void MSG_Add(const char *name, const char *markup_msg)
{
	MSG_Add(name, markup_msg, false);
}

// Replace existing or add if it doesn't exist
void MSG_Replace(const char *name, const char *markup_msg, bool is_utf8)
{
	auto it = messages.find(name);
	if (it == messages.end())
		MSG_Add(name, markup_msg, is_utf8);
	else
		it->second.Set(markup_msg, is_utf8);
}

static bool load_message_file(const std_fs::path &filename_plain,
                              const std_fs::path &filename_utf8)
{
	if (filename_plain.empty() && filename_utf8.empty())
		return false;

	auto check_file = [](const std_fs::path &filename) {
		if (filename.empty())
			return false;
		return (std_fs::status(filename).type() !=
		        std_fs::file_type::not_found);
	};

	bool is_utf8 = false;

	std::string filename_str;
	if (check_file(filename_utf8)) {
		filename_str = filename_utf8.string();
		is_utf8      = true;
	} else if (check_file(filename_plain))
		filename_str = filename_plain.string();
	else {
		if (filename_utf8.empty())
			filename_str = filename_plain.string();
		else if (filename_plain.empty())
			filename_str = filename_utf8.string();
		else
			filename_str = filename_plain.string() + " / " +
			               filename_utf8.string();

		LOG_MSG("LANG: Language file %s not found, skipping",
		        filename_str.c_str());
		return false;
	}

	FILE *mfile = fopen(filename_str.c_str(), "rt");
	if (!mfile) {
		LOG_MSG("LANG: Failed opening language file: %s, skipping",
		        filename_str.c_str());
		return false;
	}

	char linein[LINE_IN_MAXLEN];
	char name[LINE_IN_MAXLEN];
	char message[LINE_IN_MAXLEN * 10];
	/* Start out with empty strings */
	name[0] = 0;
	message[0] = 0;
	while (fgets(linein, LINE_IN_MAXLEN, mfile) != 0) {
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
			MSG_Replace(name, message, is_utf8);
		} else {
			/* Normal message to be added */
			safe_strcat(message, linein);
			safe_strcat(message, "\n");
		}
	}
	fclose(mfile);
	LOG_MSG("LANG: Loaded language file: %s", filename_str.c_str());
	return true;
}

const char *MSG_Get(char const *requested_name)
{
	const auto it = messages.find(requested_name);
	if (it != messages.end()) {
		return it->second.GetRendered();
	}
	return "Message not Found!\n";
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
		fprintf(out, ":%s\n%s\n.\n", name.data(), messages.at(name).GetMarkup());

	fclose(out);
	return true;
}

// MSG_Init loads the requested language provided on the command line or
// from the language = conf setting.

// 1. The provided language can be an exact filename and path to the lng
//    file, which is the traditionnal method to load a language file.

// 2. It also supports the more convenient syntax without needing to provide a
//    filename or path: `-lang ru`. In this case, it constructs a path into the
//    platform's config path/translations/<lang>[-utf8].lng.

void MSG_Init([[maybe_unused]] Section_prop *section)
{
	const auto lang = SETUP_GetLanguage();

	// If the language is english, then use the internal message
	if (lang.empty() || starts_with("en", lang)) {
		LOG_MSG("LANG: Using internal English language messages");
		return;
	}

	bool result = false;
	if (ends_with(lang, "-utf8.lng"))
		result = load_message_file("", GetResourcePath("translations", lang));
	else if (ends_with(lang, ".lng"))
		result = load_message_file(GetResourcePath("translations", lang), "");
	else {
		// If a short-hand name was provided then add the file extension
		result = load_message_file(GetResourcePath("translations",
		                                           lang + ".lng"),
		                           GetResourcePath("translations",
		                                           lang + "-utf8.lng"));
	}

	if (result)
		return;

	// If we got here, then the language was not found
	LOG_MSG("LANG: No language could be loaded, using English messages");
}
