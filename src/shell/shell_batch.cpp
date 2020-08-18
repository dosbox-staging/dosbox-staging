/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#include <climits>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "support.h"

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
	delete cmd;
	shell->bf=prev;
	shell->echo=echo;
}

// TODO: Refactor this sprawling function into smaller ones without GOTOs
bool BatchFile::ReadLine(char * line) {
	//Open the batchfile and seek to stored postion
	if (!DOS_OpenFile(filename.c_str(),(DOS_NOT_INHERIT|OPEN_READ),&file_handle)) {
		LOG(LOG_MISC,LOG_ERROR)("ReadLine Can't open BatchFile %s",filename.c_str());
		delete this;
		return false;
	}
	DOS_SeekFile(file_handle,&(this->location),DOS_SEEK_SET);

	uint16_t bytes_read = 1;
	uint8_t data = 0;
	char val = 0;
	char temp[CMD_MAXLINE] = "";
emptyline:
	char * cmd_write=temp;
	do {
		bytes_read = 1;
		DOS_ReadFile(file_handle, &data, &bytes_read);
		val = static_cast<char>(data);

		if (bytes_read > 0) {
			/* Inclusion criteria:
			 *  - backspace for alien odyssey
			 *  - tab for batch files
			 *  - escape for ANSI
			 * Note: the negative allowance permits high
			 * international ASCII characters that are wrapped when
			 * char is a signed type
			 */
			if (val < 0 || val > UNIT_SEPARATOR ||
			    val == BACKSPACE || val == ESC || val == TAB) {
				// Only add it if room for it (and trailing zero)
				// in the buffer, but do the check here instead
				// at the end So we continue reading till EOL/EOF
				if (cmd_write - temp + 1 < CMD_MAXLINE - 1) {
					*cmd_write++ = val;
				}
			} else if (val != LINE_FEED && val != CARRIAGE_RETURN) {
				shell->WriteOut(MSG_Get("SHELL_ILLEGAL_CONTROL_CHARACTER"),
				                val, val);
			}
		}
	} while (val != LINE_FEED && bytes_read);
	*cmd_write=0;
	if (!bytes_read && cmd_write == temp) {
		//Close file and delete bat file
		DOS_CloseFile(file_handle);
		delete this;
		return false;
	}
	if (!strlen(temp)) goto emptyline;
	if (temp[0]==':') goto emptyline;

	// Lambda that copies the src to cmd_write, provided the result fits
	// within CMD_MAXLINE while taking the existing line into consideration.
	// Finally it moves the cmd_write pointer ahead by the length copied.
	auto append_cmd_write = [&cmd_write, &line](const char *src) {
		const auto src_len = strlen(src);
		const auto req_len = cmd_write - line + static_cast<int>(src_len);
		if (req_len < CMD_MAXLINE - 1) {
			safe_strncpy(cmd_write, src, src_len);
			cmd_write += src_len;
		}
	};

	/* Now parse the line read from the bat file for % stuff */
	cmd_write=line;
	char * cmd_read=temp;
	while (*cmd_read) {
		if (*cmd_read == '%') {
			cmd_read++;
			if (cmd_read[0] == '%') {
				cmd_read++;
				if (((cmd_write - line) + 1) < (CMD_MAXLINE - 1))
					*cmd_write++ = '%';
				continue;
			}
			if (cmd_read[0] == '0') {  /* Handle %0 */
				const char *file_name = cmd->GetFileName();
				cmd_read++;
				append_cmd_write(file_name);
				continue;
			}
			char next = cmd_read[0];
			if(next > '0' && next <= '9') {
				/* Handle %1 %2 .. %9 */
				cmd_read++; //Progress reader
				next -= '0';
				if (cmd->GetCount()<(unsigned int)next) continue;
				std::string word;
				assert(next >= 0);
				if (!cmd->FindCommand(static_cast<unsigned>(next), word))
					continue;
				append_cmd_write(word.c_str());
				continue;
			} else {
				/* Not a command line number has to be an environment */
				char * first = strchr(cmd_read,'%');
				/* No env afterall. Ignore a single % */
				if (!first) {/* *cmd_write++ = '%';*/continue;}
				*first++ = 0;
				std::string env;
				if (shell->GetEnvStr(cmd_read,env)) {
					const char* equals = strchr(env.c_str(),'=');
					if (!equals) continue;
					equals++;
					append_cmd_write(equals);
				}
				cmd_read = first;
			}
		} else {
			if (((cmd_write - line) + 1) < (CMD_MAXLINE - 1))
				*cmd_write++ = *cmd_read++;
		}
	}
	*cmd_write = 0;
	//Store current location and close bat file
	this->location = 0;
	DOS_SeekFile(file_handle,&(this->location),DOS_SEEK_CUR);
	DOS_CloseFile(file_handle);
	return true;	
}

// TODO: Refactor this sprawling function into smaller ones without GOTOs
bool BatchFile::Goto(char * where) {
	//Open bat file and search for the where string
	if (!DOS_OpenFile(filename.c_str(),(DOS_NOT_INHERIT|OPEN_READ),&file_handle)) {
		LOG(LOG_MISC,LOG_ERROR)("SHELL:Goto Can't open BatchFile %s",filename.c_str());
		delete this;
		return false;
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
			if (val < 0 || val > UNIT_SEPARATOR) {
				if (cmd_write - cmd_buffer + 1 < CMD_MAXLINE - 1) {
					*cmd_write++ = val;
				}
			} else if (val != BACKSPACE && val != CARRIAGE_RETURN &&
			           val != ESC && val != LINE_FEED && val != TAB) {
				shell->WriteOut(MSG_Get("SHELL_ILLEGAL_CONTROL_CHARACTER"),
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
		delete this;
		return false;
	}
	goto again;
	return false;
}

void BatchFile::Shift(void) {
	cmd->Shift(1);
}
