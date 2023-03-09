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

#include "shell.h"

#include <climits>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "string_utils.h"

// Permitted ASCII control characters in batch files
constexpr uint8_t BACKSPACE = 8;
constexpr uint8_t CARRIAGE_RETURN = '\r';
constexpr uint8_t ESC = 27;
constexpr uint8_t LINE_FEED = '\n';
constexpr uint8_t TAB = '\t';
constexpr uint8_t UNIT_SEPARATOR = 31;

BatchFile::BatchFile(DOS_Shell *host,
                     char const *const resolved_name,
                     char const *const entered_name,
                     char const *const cmd_line)
        : file_handle(0),
          location(0),
          echo(host->echo),
          shell(host),
          prev(host->bf),
          cmd(new CommandLine(entered_name, cmd_line)),
          filename("")
{
	char totalname[DOS_PATHLENGTH+4];

	// Get fullname including drive specification
	if (!DOS_Canonicalize(resolved_name, totalname))
		E_Exit("SHELL: Can't determine path to batch file %s", resolved_name);
	
	filename = totalname;
	// Test if file is openable
	if (!DOS_OpenFile(totalname,(DOS_NOT_INHERIT|OPEN_READ),&file_handle)) {
		//TODO Come up with something better
		E_Exit("SHELL:Can't open BatchFile %s",totalname);
	}
	DOS_CloseFile(file_handle);
}

BatchFile::~BatchFile() {
	cmd.reset();
	assert(shell);
	shell->bf = prev;
	shell->echo = echo;
}

bool BatchFile::ReadLine(char* lineout)
{
	std::string line = {};
	while (line.empty()) {
		line = GetLine();

		if (line.empty()) {
			return false;
		}

		const auto colon_index = line.find_first_of(':');
		if (colon_index != std::string::npos &&
		    colon_index == line.find_first_not_of("=\t ")) {
			// Line is a comment or label, so ignore it
			line.clear();
		}

		trim(line);
	}

	line = ExpandedBatchLine(line);
	strncpy(lineout, line.c_str(), CMD_MAXLINE);
	lineout[CMD_MAXLINE - 1] = '\0';
	return true;
}

std::string BatchFile::GetLine()
{
	if (!DOS_OpenFile(filename.c_str(), (DOS_NOT_INHERIT | OPEN_READ), &file_handle)) {
		LOG(LOG_MISC, LOG_ERROR)
		("ReadLine Can't open BatchFile %s", filename.c_str());
		return "";
	}
	DOS_SeekFile(file_handle, &(this->location), DOS_SEEK_SET);

	uint8_t data        = 0;
	uint16_t bytes_read = 1;
	std::string line    = {};

	while (data != LINE_FEED) {
		DOS_ReadFile(file_handle, &data, &bytes_read);

		// EOF
		if (bytes_read == 0) {
			break;
		}

		/* Inclusion criteria:
		 *  - backspace for alien odyssey
		 *  - tab for batch files
		 *  - escape for ANSI
		 */
		if (data <= UNIT_SEPARATOR && data != TAB && data != BACKSPACE &&
		    data != ESC && data != LINE_FEED && data != CARRIAGE_RETURN) {
			DEBUG_LOG_MSG("Encountered non-standard character: Dec %03u and Hex %#04x",
			              data,
			              data);
		} else {
			line += static_cast<char>(data);
		}
	}

	this->location = 0;
	DOS_SeekFile(file_handle, &(this->location), DOS_SEEK_CUR);
	DOS_CloseFile(file_handle);

	return line;
}

std::string BatchFile::ExpandedBatchLine(std::string_view line) const
{
	std::string expanded = {};

	auto percent_index = line.find_first_of('%');
	while (percent_index != std::string::npos) {
		expanded += line.substr(0, percent_index);
		line = line.substr(percent_index + 1);

		if (line.empty()) {
			break;
		} else if (line[0] == '%') {
			expanded += '%';
		} else if (line[0] == '0') {
			expanded += cmd->GetFileName();
		} else if (line[0] >= '1' && line[0] <= '9') {
			std::string arg;
			if (cmd->FindCommand(line[0] - '0', arg)) {
				expanded += arg;
			}
		} else {
			auto closing_percent = line.find_first_of('%');
			if (closing_percent == std::string::npos) {
				break;
			}
			std::string result;
			std::string envvar(line.substr(0, closing_percent));
			shell->GetEnvStr(envvar.c_str(), result);
			result.erase(0, envvar.size() + sizeof('='));
			expanded += result;
			line = line.substr(closing_percent);
		}
		line          = line.substr(1);
		percent_index = line.find_first_of('%');
	}

	expanded += line;
	return expanded;
}

// TODO: Refactor this sprawling function into smaller ones without GOTOs
bool BatchFile::Goto(char * where) {
	//Open bat file and search for the where string
	if (!DOS_OpenFile(filename.c_str(),(DOS_NOT_INHERIT|OPEN_READ),&file_handle)) {
		LOG(LOG_MISC,LOG_ERROR)("SHELL:Goto Can't open BatchFile %s",filename.c_str());
		return false; // Parent deletes this BatchFile on negative return
	}

	char cmd_buffer[CMD_MAXLINE] = "";
	char *cmd_write = nullptr;

	/* Scan till we have a match or return false */
	uint16_t bytes_read = 1;
	uint8_t data = 0;
	char val = 0;
again:
	cmd_write=cmd_buffer;
	do {
		bytes_read = 1;
		DOS_ReadFile(file_handle, &data, &bytes_read);
		val = static_cast<char>(data);
		if (bytes_read > 0) {
			// Note: the negative allowance permits high
			// international ASCII characters that are wrapped when
			// char is a signed type

			if (
#if (CHAR_MIN < 0) // char is signed
			    val < 0 ||
#endif
			    val > UNIT_SEPARATOR) {
				if (cmd_write - cmd_buffer + 1 < CMD_MAXLINE - 1) {
					*cmd_write++ = val;
				}
			} else if (val != BACKSPACE && val != CARRIAGE_RETURN &&
			           val != ESC && val != LINE_FEED && val != TAB) {
				DEBUG_LOG_MSG("Encountered non-standard character: Dec %03u and Hex %#04x",
				              val, val);
			}
		}
	} while (val != LINE_FEED && bytes_read);
	*cmd_write++ = 0;
	char *nospace = trim(cmd_buffer);
	if (nospace[0] == ':') {
		nospace++; //Skip :
		//Strip spaces and = from it.
		while(*nospace && (isspace(*reinterpret_cast<unsigned char*>(nospace)) || (*nospace == '=')))
			nospace++;

		//label is until space/=/eol
		char* const beginlabel = nospace;
		while(*nospace && !isspace(*reinterpret_cast<unsigned char*>(nospace)) && (*nospace != '=')) 
			nospace++;

		*nospace = 0;
		if (strcasecmp(beginlabel,where)==0) {
		//Found it! Store location and continue
			this->location = 0;
			DOS_SeekFile(file_handle,&(this->location),DOS_SEEK_CUR);
			DOS_CloseFile(file_handle);
			return true;
		}
	   
	}
	if (!bytes_read) {
		DOS_CloseFile(file_handle);
		return false; // Parent deletes this BatchFile on negative return
	}
	goto again;
	return false;
}

void BatchFile::Shift()
{
	cmd->Shift(1);
}
