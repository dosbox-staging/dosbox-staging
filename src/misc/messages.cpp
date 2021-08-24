/*
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <string>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "setup.h"
#include "support.h"

#define LINE_IN_MAXLEN 2048

static std::unordered_map<std::string, std::string> messages;
static std::deque<std::string> messages_order;

// Add but don't replace existing
void MSG_Add(const char *name, const char *msg)
{
	// Only add the message if it doesn't exist yet
	if (messages.find(name) == messages.end()) {
		messages[name] = msg;
		messages_order.emplace_back(name);
	}
}

// Replace existing or add if it doesn't exist
void MSG_Replace(const char *name, const char *msg)
{
	auto it = messages.find(name);
	if (it == messages.end())
		MSG_Add(name, msg);
	else // replace the prior message
		it->second = msg;
}

static bool LoadMessageFile(std::string filename)
{
	if (filename.empty())
		return false;

	// Expand the filename and check if it exists -- returns empty if not found
	filename = to_native_path(filename);

	// Was the file not found?
	if (filename.empty()) {
		LOG_MSG("LANG: Language file %s not found, skipping",
		        filename.c_str());
		return false;
	}

	FILE *mfile = fopen(filename.c_str(), "rt");
	if (!mfile) {
		LOG_MSG("LANG: Failed opening language file: %s, skipping",
		        filename.c_str());
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
			MSG_Replace(name, message);
		} else {
			/* Normal message to be added */
			safe_strcat(message, linein);
			safe_strcat(message, "\n");
		}
	}
	fclose(mfile);
	LOG_MSG("LANG: Loaded language file: %s", filename.c_str());
	return true;
}

const char *MSG_Get(char const *requested_name)
{
	const auto it = messages.find(requested_name);
	if (it != messages.end())
		return it->second.c_str();
	return "Message not Found!\n";
}

// Write the names and messages (in the order they were added) to the given location
bool MSG_Write(const char * location) {
	FILE *out = fopen(location, "w+t");
	if (!out)
		return false;

	for (const auto &name : messages_order)
		fprintf(out, ":%s\n%s\n.\n", name.c_str(),
		        messages.at(name).c_str());

	fclose(out);
	return true;
}

// MSG_Init loads the requested language provided on the command line or
// from the language = conf setting.

// 1. The provided language can be an exact filename and path to the lng
//    file, which is the traditionnal method to load a language file.

// 2. It also supports the more convenient syntax without needing to provide a
//    filename or path: `-lang ru`. In this case, it constructs a path into the
//    platform's config path/translations/<lang>.lng. 

void MSG_Init(Section_prop *section)
{
	std::string lang = {};
	std::deque<std::string> langs = {};

	// Did the user provide a language on the command line?
	if (control->cmdline->FindString("-lang", lang, true))
		langs.emplace_back(std::move(lang));

	// Is a language provided in the conf file?
	const auto pathprop = section->Get_path("language");
	if (pathprop) {
		lang = pathprop->realpath;
		if (lang.size()) {
			langs.emplace_back(std::move(lang));
		}
	}

	// No languages provided, so nothing more to do!
	if (langs.empty())
		return;

	// Try load the user's language file(s)
	for (const auto &l : langs) {
		// If a short-hand name was provided then add the file extension
		lang = l + (ends_with(l, ".lng") ? "" : ".lng");

		// Can we load the filename from the current path?
		if (path_exists(lang) && LoadMessageFile(lang)) {
			return;
		}
		// If not, let's try getting it from the config/translations
		// area. To do this we need to first get just the
		// filename-portion from the possible /full/path/lang.lng
		const auto path_elements = split(lang, CROSS_FILESPLIT);
		if (path_elements.empty())
			continue;

		// the last element is the filename without the path
		lang = path_elements.back();
		LOG_MSG("LANG: Searching config path for: %s", lang.c_str());

		// Construct a full path by prepending the config/translations path
		lang = CROSS_GetPlatformConfigDir() + "translations" + CROSS_FILESPLIT + lang;

		// Try loading it
		if (LoadMessageFile(lang)) {
			return;
		}
	}
}
