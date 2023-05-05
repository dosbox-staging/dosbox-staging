/*
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <regex>

#include "regs.h"
#include "callback.h"
#include "string_utils.h"
#include "../ints/int10.h"

DOS_Shell::~DOS_Shell() {
	bf.reset();
}

void DOS_Shell::ShowPrompt(void) {
	uint8_t drive=DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	reset_str(dir); // DOS_GetCurrentDir doesn't always return
	                // something. (if drive is messed up)
	DOS_GetCurrentDir(0, dir);
	InjectMissingNewline();
	WriteOut("%c:\\%s>", drive, dir);
	ResetLastWrittenChar('\n'); // prevents excessive newline if cmd prints nothing
}

static void outc(uint8_t c) {
	uint16_t n=1;
	DOS_WriteFile(STDOUT,&c,&n);
}

static void move_cursor_back_one() {
	const uint8_t page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
	if (CURSOR_POS_COL(page) > 0) {
		outc(8);
	} else if (const auto row = CURSOR_POS_ROW(page); row > 0) {
		BIOS_NCOLS;
		INT10_SetCursorPos(row - 1, ncols - 1, page);
	}
}

void DOS_Shell::InputCommand(char * line) {
	Bitu size=CMD_MAXLINE-2; //lastcharacter+0
	uint8_t c;uint16_t n=1;
	size_t str_len = 0;
	size_t str_index = 0;
	uint16_t len=0;
	bool current_hist=false; // current command stored in history?

	reset_str(line);

	std::list<std::string>::iterator it_history = l_history.begin(), it_completion = l_completion.begin();

	while (size && !shutdown_requested) {
		dos.echo=false;
		while(!DOS_ReadFile(input_handle,&c,&n)) {
			uint16_t dummy;
			DOS_CloseFile(input_handle);
			DOS_OpenFile("con",2,&dummy);
			LOG(LOG_MISC,LOG_ERROR)("Reopening the input handle. This is a bug!");
		}
		if (!n) {
			size=0;			//Kill the while loop
			continue;
		}
		switch (c) {
		case 0x00: /* Extended Keys */
			{
				DOS_ReadFile(input_handle,&c,&n);
				switch (c) {

				case 0x3d: /* F3 */
					if (!l_history.size()) break;
					it_history = l_history.begin();
					if (it_history != l_history.end() && it_history->length() > str_len) {
						const char *reader = &(it_history->c_str())[str_len];
						while ((c = *reader++)) {
							line[str_index ++] = c;
							DOS_WriteFile(STDOUT,&c,&n);
						}
						str_len = str_index = it_history->length();
						size = CMD_MAXLINE - str_index - 2;
						terminate_str_at(line, str_len);
					}
					break;

				case 0x4B: /* Left */
					if (str_index) {
						move_cursor_back_one();
						str_index --;
					}
					break;

				case 0x4D: /* Right */
					if (str_index < str_len) {
						outc(line[str_index++]);
					}
					break;

				case 0x47: /* Home */
					while (str_index) {
						move_cursor_back_one();
						str_index--;
					}
					break;

				case 0x4F: /* End */
					while (str_index < str_len) {
						outc(line[str_index++]);
					}
					break;

				case 0x48: /* Up */
					if (l_history.empty() || it_history == l_history.end()) break;

					// store current command in history if we are at beginning
					if (it_history == l_history.begin() && !current_hist) {
						current_hist=true;
						l_history.push_front(line);
					}

					for (;str_index>0; str_index--) {
						// removes all characters
						move_cursor_back_one();
						outc(' ');
						move_cursor_back_one();
					}
					strcpy(line, it_history->c_str());
					len = (uint16_t)it_history->length();
					str_len = str_index = len;
					size = CMD_MAXLINE - str_index - 2;
					DOS_WriteFile(STDOUT, (uint8_t *)line, &len);
					++it_history;
					break;

				case 0x50: /* Down */
					if (l_history.empty() || it_history == l_history.begin()) break;

					// not very nice but works ..
					--it_history;
					if (it_history == l_history.begin()) {
						// no previous commands in history
						++it_history;

						// remove current command from history
						if (current_hist) {
							current_hist = false;
							l_history.pop_front();
						}
						break;
					} else {
						--it_history;
					}

					for (; str_index > 0; str_index--) {
						// removes all characters
						move_cursor_back_one();
						outc(' ');
						move_cursor_back_one();
					}
					strcpy(line, it_history->c_str());
					len = (uint16_t)it_history->length();
					str_len = str_index = len;
					size = CMD_MAXLINE - str_index - 2;
					DOS_WriteFile(STDOUT, (uint8_t *)line, &len);
					++it_history;

					break;
				case 0x53: /* Delete */
					{
						if(str_index>=str_len) break;
						auto text_len = static_cast<uint16_t>(str_len - str_index - 1);
						uint8_t* text=reinterpret_cast<uint8_t*>(&line[str_index+1]);
						DOS_WriteFile(STDOUT, text, &text_len); // write buffer to screen
						outc(' ');
						move_cursor_back_one();
						for (auto i = str_index; i < str_len-1; i++) {
							line[i]=line[i+1];
							move_cursor_back_one();
						}
						terminate_str_at(line, --str_len);
						size++;
					}
					break;
				case 15: /* Shift+Tab */
					if (l_completion.size()) {
						if (it_completion == l_completion.begin()) {
							it_completion = l_completion.end ();
						}
						--it_completion;
		
						if (it_completion->length()) {
							for (;str_index > completion_index; str_index--) {
								// removes all characters
								move_cursor_back_one();
								outc(' ');
								move_cursor_back_one();
							}

							strcpy(&line[completion_index], it_completion->c_str());
							len = (uint16_t)it_completion->length();
							str_len = str_index = completion_index + len;
							size = CMD_MAXLINE - str_index - 2;
							DOS_WriteFile(STDOUT, (uint8_t *)it_completion->c_str(), &len);
						}
					}
				default: break;
				}
			};
			break;
		case 0x08: /* Backspace */
			if (str_index) {
				move_cursor_back_one();
				size_t str_remain = str_len - str_index;
				size++;
				if (str_remain) {
					memmove(&line[str_index-1],&line[str_index],str_remain);
					terminate_str_at(line, --str_len);
					str_index --;
					/* Go back to redraw */
					for (size_t i = str_index; i < str_len; i++)
						outc(line[i]);
				} else {
					terminate_str_at(line, --str_index);
					str_len--;
				}
				outc(' ');
				move_cursor_back_one();
				// moves the cursor left
				while (str_remain--)
					move_cursor_back_one();
			}
			if (l_completion.size()) l_completion.clear();
			break;
		case 0x0a: /* New Line not handled */
			/* Don't care */
			break;
		case 0x0d: /* Return */
			outc('\r');
			outc('\n');
			size=0;			//Kill the while loop
			break;
		case'\t':
			{
				if (l_completion.size()) {
					++it_completion;
					if (it_completion == l_completion.end())
						it_completion = l_completion.begin();
				} else {
					// build new completion list
					// Lines starting with CD will only get
					// directories in the list
					bool dir_only = (strncasecmp(line,"CD ",3)==0);

					// get completion mask
					char *p_completion_start = strrchr(line, ' ');

					if (p_completion_start) {
						p_completion_start ++;
						completion_index = (uint16_t)(str_len - strlen(p_completion_start));
					} else {
						p_completion_start = line;
						completion_index = 0;
					}

					char *path;
					if ((path = strrchr(line+completion_index,'\\'))) completion_index = (uint16_t)(path-line+1);
					if ((path = strrchr(line+completion_index,'/'))) completion_index = (uint16_t)(path-line+1);

					// build the completion list
					char mask[DOS_PATHLENGTH] = {0};
					if (p_completion_start) {
						if (strlen(p_completion_start) + 3 >= DOS_PATHLENGTH) {
							//Beep;
							break;
						}
						safe_strcpy(mask, p_completion_start);
						char* dot_pos=strrchr(mask,'.');
						char* bs_pos=strrchr(mask,'\\');
						char* fs_pos=strrchr(mask,'/');
						char* cl_pos=strrchr(mask,':');
						// not perfect when line already contains wildcards, but works
						if ((dot_pos-bs_pos>0) && (dot_pos-fs_pos>0) && (dot_pos-cl_pos>0))
							strncat(mask, "*",DOS_PATHLENGTH - 1);
						else strncat(mask, "*.*",DOS_PATHLENGTH - 1);
					} else {
					        safe_strcpy(mask, "*.*");
				        }

					RealPt save_dta=dos.dta();
					dos.dta(dos.tables.tempdta);

					bool res = DOS_FindFirst(mask, 0xffff & ~DOS_ATTR_VOLUME);
					if (!res) {
						dos.dta(save_dta);
						break;	// TODO: beep
					}

					DOS_DTA dta(dos.dta());
					char name[DOS_NAMELENGTH_ASCII];uint32_t sz;uint16_t date;uint16_t time;uint8_t att;

					std::list<std::string> executable;
					while (res) {
						dta.GetResult(name,sz,date,time,att);
						// add result to completion list

						if (strcmp(name, ".") && strcmp(name, "..")) {
							if (dir_only) { //Handle the dir only case different (line starts with cd)
								if(att & DOS_ATTR_DIRECTORY) l_completion.push_back(name);
							} else {
							        if (is_executable_filename(name))
								        // Prepend executables to a separate list
								        // and place that list ahead of normal files.
									executable.push_front(name);
								else
									l_completion.push_back(name);
							}
						}
						res=DOS_FindNext();
					}
					/* Add executable list to front of completion list. */
					std::copy(executable.begin(),executable.end(),std::front_inserter(l_completion));
					it_completion = l_completion.begin();
					dos.dta(save_dta);
				}

				if (l_completion.size() && it_completion->length()) {
					for (;str_index > completion_index; str_index--) {
						// removes all characters
						move_cursor_back_one();
						outc(' ');
						move_cursor_back_one();
					}

					strcpy(&line[completion_index], it_completion->c_str());
					len = (uint16_t)it_completion->length();
					str_len = str_index = completion_index + len;
					size = CMD_MAXLINE - str_index - 2;
					DOS_WriteFile(STDOUT, (uint8_t *)it_completion->c_str(), &len);
				}
			}
			break;
		case 0x1b: /* Esc */
			while (str_index < str_len) {
				outc(line[str_index++]);
			}
			//write a backslash and return to the next line
			outc('\\');
			outc('\r');
			outc('\n');
			reset_str(line);
			if (l_completion.size()) l_completion.clear(); //reset the completion list.
			this->InputCommand(line);	//Get the NEW line.
			size = 0;       // stop the next loop
			str_len = 0;    // prevent multiple adds of the same line
			break;
		default:
			if (l_completion.size()) l_completion.clear();
			if(str_index < str_len && true) { //mem_readb(BIOS_KEYBOARD_FLAGS1)&0x80) dev_con.h ?
				outc(' ');//move cursor one to the right.
				auto text_len = static_cast<uint16_t>(str_len - str_index);
				uint8_t* text=reinterpret_cast<uint8_t*>(&line[str_index]);
				DOS_WriteFile(STDOUT, text, &text_len); // write buffer to screen
				move_cursor_back_one(); //undo the cursor the right.
				for (auto i = str_len; i > str_index; i--) {
					line[i]=line[i-1]; //move internal buffer
					move_cursor_back_one(); //move cursor back (from write buffer to screen)
				}
				// new end (as the internal buffer moved one
				// place to the right
				terminate_str_at(line, ++str_len);
				size--;
			};

			line[str_index]=c;
			str_index ++;
			if (str_index > str_len){ 
				terminate_str_at(line, str_index);
				str_len++;
				size--;
			}
			DOS_WriteFile(STDOUT,&c,&n);
			break;
		}
	}

	if (!str_len) return;
	str_len++;

	// remove current command from history if it's there
	if (current_hist) {
		// current_hist=false;
		l_history.pop_front();
	}

	// add command line to history
	l_history.push_front(line); it_history = l_history.begin();
	if (l_completion.size()) l_completion.clear();

	/* DOS %variable% substitution */
	const auto dos_section = static_cast<Section_prop *>(control->GetSection("dos"));
	assert(dos_section);
	std::string_view expand_shell_variable_pref =
					dos_section->Get_string("expand_shell_variable");
	if (expand_shell_variable_pref == "true") {
		ProcessCmdLineEnvVarStitution(line);
	} else if (expand_shell_variable_pref == "auto" && dos.version.major >= 7) {
		ProcessCmdLineEnvVarStitution(line);
	}
}

/* Note: Buffer pointed to by "line" must be at least CMD_MAXLINE+1 bytes long! */
void DOS_Shell::ProcessCmdLineEnvVarStitution(char* line) {
	constexpr char surrogate_percent = 8;
	const static std::regex re("\\%([^%0-9][^%]*)?%");
	std::string text = line;
	std::smatch match;
	/* Iterate over potential %var1%, %var2%, etc matches found in the text string */
	while (std::regex_search(text, match, re)) {
		// Get the first matching %'s position and length
		const auto percent_pos = static_cast<size_t>(match.position(0));
		const auto percent_len = static_cast<size_t>(match.length(0));

		std::string variable_name = match[1].str();
		if (variable_name.empty()) {
			/* Replace %% with the character "surrogate_percent", then (eventually) % */
			text.replace(percent_pos, percent_len,
			             std::string(1, surrogate_percent));
			continue;
		}
		/* Trim preceding spaces from the variable name */
		variable_name.erase(0, variable_name.find_first_not_of(' '));
		std::string variable_value;
		if (variable_name.size() && GetEnvStr(variable_name.c_str(), variable_value)) {
			const size_t equal_pos = variable_value.find_first_of('=');
			/* Replace the original %var% with its corresponding value from the environment */
			const std::string replacement = equal_pos != std::string::npos
			                ? variable_value.substr(equal_pos + 1) : "";
			text.replace(percent_pos, percent_len, replacement);
		}
		else {
			text.replace(percent_pos, percent_len, "");
		}
	}
	std::replace(text.begin(), text.end(), surrogate_percent, '%');
	assert(text.size() <= CMD_MAXLINE);
	safe_strncpy(line, text.c_str(), CMD_MAXLINE);
}

std::string full_arguments = "";
bool DOS_Shell::Execute(char * name,char * args) {
/* return true  => don't check for hardware changes in do_command 
 * return false =>       check for hardware changes in do_command */
	char fullname[DOS_PATHLENGTH+4]; //stores results from Which
	char line[CMD_MAXLINE];
	const bool have_args = args && args[0] != '\0';
	if (have_args) {
		if (*args != ' ') { // put a space in front
			line[0] = ' ';
			terminate_str_at(line, 1);
			strncat(line, args, CMD_MAXLINE - 2);
			terminate_str_at(line, CMD_MAXLINE - 1);
		} else {
			safe_strcpy(line, args);
		}
	} else {
		reset_str(line);
	}

	/* check for a drive change */
	if (((strcmp(name + 1, ":") == 0) || (strcmp(name + 1, ":\\") == 0)) && isalpha(*name))
	{
		const auto drive_idx = drive_index(name[0]);
		if (!DOS_SetDrive(drive_idx)) {
			WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),
			         toupper(name[0]));
		}
		return true;
	}

	/* Check for a full name */
	const char *p_fullname = Which(name);
	if (!p_fullname)
		return false;
	safe_strcpy(fullname, p_fullname);

	// Always disallow files without extension from being executed.
	// Only internal commands can be run this way and they never get in
	// this handler.
	const char *extension = strrchr(fullname, '.');
	if (!extension) {
		// Check if the result will fit in the parameters.
		if (safe_strlen(fullname) > (DOS_PATHLENGTH - 1)) {
			WriteOut(MSG_Get("PROGRAM_PATH_TOO_LONG"), fullname,
			         DOS_PATHLENGTH);
			return false;
		}

		// Try to add .COM, .EXE and .BAT extensions to the filename
		char temp_name[DOS_PATHLENGTH + 4];
		for (const char *ext : {".COM", ".EXE", ".BAT"}) {
			safe_sprintf(temp_name, "%s%s", fullname, ext);
			const char *temp_fullname = Which(temp_name);
			if (temp_fullname) {
				extension = ext;
				safe_strcpy(fullname, temp_fullname);
				break;
			}
		}

		if (!extension)
			return false;
	}

	if (strcasecmp(extension, ".bat") == 0) 
	{	/* Run the .bat file */
		/* delete old batch file if call is not active*/
		bool temp_echo=echo; /*keep the current echostate (as delete bf might change it )*/
		if (bf && !call)
			bf.reset();
		bf = std::make_shared<BatchFile>(this, fullname, name, line);
		echo = temp_echo; // restore it.
	} 
	else 
	{	/* only .bat .exe .com extensions maybe be executed by the shell */
		if(strcasecmp(extension, ".com") !=0) 
		{
			if(strcasecmp(extension, ".exe") !=0) return false;
		}
		/* Run the .exe or .com file from the shell */
		/* Allocate some stack space for tables in physical memory */
		reg_sp-=0x200;
		//Add Parameter block
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);
		block.Clear();
		//Add a filename
		RealPt file_name=RealMakeSeg(ss,reg_sp+0x20);
		MEM_BlockWrite(RealToPhysical(file_name), fullname,
		               (Bitu)(safe_strlen(fullname) + 1));

		/* HACK: Store full commandline for mount and imgmount */
		full_arguments.assign(line);

		/* Fill the command line */
		CommandTail cmdtail = {};

		// copy at-most 126 chracters plus the terminating zero
		safe_strcpy(cmdtail.buffer, line);

		cmdtail.count = check_cast<uint8_t>(safe_strlen(cmdtail.buffer));
		terminate_str_at(line, cmdtail.count);

		assert(cmdtail.count < sizeof(cmdtail.buffer));
		cmdtail.buffer[cmdtail.count] = 0xd;

		/* Copy command line in stack block too */
		MEM_BlockWrite(SegPhys(ss)+reg_sp+0x100,&cmdtail,128);

		
		/* Split input line up into parameters, using a few special rules, most notable the one for /AAA => A\0AA
		 * Qbix: It is extremly messy, but this was the only way I could get things like /:aa and :/aa to work correctly */
		
		//Prepare string first
		char parseline[258] = { 0 };
		for(char *pl = line,*q = parseline; *pl ;pl++,q++) {
			if (*pl == '=' || *pl == ';' || *pl ==',' || *pl == '\t' || *pl == ' ') 
				reset_str(q);
			else
				*q = *pl; //Replace command seperators with 0.
		} //No end of string \0 needed as parseline is larger than line

		for(char* p = parseline; (p-parseline) < 250 ;p++) { //Stay relaxed within boundaries as we have plenty of room
			if (*p == '/') { //Transform /Hello into H\0ello
				reset_str(p);
				p++;
				while ( *p == 0 && (p-parseline) < 250) p++; //Skip empty fields
				if ((p-parseline) < 250) { //Found something. Lets get the first letter and break it up
					p++;
					memmove(static_cast<void*>(p + 1),static_cast<void*>(p),(250-(p-parseline)));
					if ((p - parseline) < 250)
						reset_str(p);
				}
			}
		}
		// Just to be safe
		terminate_str_at(parseline, 255);
		terminate_str_at(parseline, 256);
		terminate_str_at(parseline, 257);

		/* Parse FCB (first two parameters) and put them into the current DOS_PSP */
		uint8_t add;
		uint16_t skip = 0;
		//find first argument, we end up at parseline[256] if there is only one argument (similar for the second), which exists and is 0.
		while(skip < 256 && parseline[skip] == 0) skip++;
		FCB_Parsename(dos.psp(),0x5C,0x01,parseline + skip,&add);
		skip += add;
		
		//Move to next argument if it exists
		while(parseline[skip] != 0) skip++;  //This is safe as there is always a 0 in parseline at the end.
		while(skip < 256 && parseline[skip] == 0) skip++; //Which is higher than 256
		FCB_Parsename(dos.psp(),0x6C,0x01,parseline + skip,&add); 

		block.exec.fcb1=RealMake(dos.psp(),0x5C);
		block.exec.fcb2=RealMake(dos.psp(),0x6C);
		/* Set the command line in the block and save it */
		block.exec.cmdtail=RealMakeSeg(ss,reg_sp+0x100);
		block.SaveData();
#if 0
		/* Save CS:IP to some point where i can return them from */
		uint32_t oldeip=reg_eip;
		uint16_t oldcs=SegValue(cs);
		RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
		SegSet16(cs,RealSeg(newcsip));
		reg_ip=RealOffset(newcsip);
#endif
		/* Start up a dos execute interrupt */
		reg_ax=0x4b00;
		//Filename pointer
		SegSet16(ds,SegValue(ss));
		reg_dx=RealOffset(file_name);
		//Paramblock
		SegSet16(es,SegValue(ss));
		reg_bx=reg_sp;
		SETFLAGBIT(IF,false);
		CALLBACK_RunRealInt(0x21);
		/* Restore CS:IP and the stack */
		reg_sp+=0x200;
#if 0
		reg_eip=oldeip;
		SegSet16(cs,oldcs);
#endif
	}
	return true; //Executable started
}

static char which_ret[DOS_PATHLENGTH+4];

const char *DOS_Shell::Which(const char *name) const
{
	const size_t name_len = strlen(name);
	if (name_len >= DOS_PATHLENGTH)
		return 0;

	/* Parse through the Path to find the correct entry */
	/* Check if name is already ok but just misses an extension */

	if (DOS_FileExists(name))
		return name;

	/* try to find .com .exe .bat */
	for (const char *ext_fmt : {"%s.COM", "%s.EXE", "%s.BAT"}) {
		safe_sprintf(which_ret, ext_fmt, name);
		if (DOS_FileExists(which_ret))
			return which_ret;
	}

	/* No Path in filename look through path environment string */
	char path[DOS_PATHLENGTH];std::string temp;
	if (!GetEnvStr("PATH",temp)) return 0;
	const char * pathenv=temp.c_str();

	assert(pathenv);
	pathenv = strchr(pathenv,'=');
	if (!pathenv) return 0;
	pathenv++;
	Bitu i_path = 0;
	while (*pathenv) {
		/* remove ; and ;; at the beginning. (and from the second entry etc) */
		while(*pathenv == ';')
			pathenv++;

		/* get next entry */
		i_path = 0; /* reset writer */
		while(*pathenv && (*pathenv !=';') && (i_path < DOS_PATHLENGTH) )
			path[i_path++] = *pathenv++;

		if(i_path == DOS_PATHLENGTH) {
			/* If max size. move till next ; and terminate path */
			while(*pathenv && (*pathenv != ';')) 
				pathenv++;
			terminate_str_at(path, DOS_PATHLENGTH - 1);
		} else {
			terminate_str_at(path, i_path);
		}

		/* check entry */
		if (size_t len = safe_strlen(path)) {
			if(len >= (DOS_PATHLENGTH - 2)) continue;

			if(path[len - 1] != '\\') {
				safe_strcat(path, "\\");
				len++;
			}

			//If name too long =>next
			if((name_len + len + 1) >= DOS_PATHLENGTH) continue;

			safe_strcat(path, name);
			safe_strcpy(which_ret, path);
			if (DOS_FileExists(which_ret))
				return which_ret;

			for (const char *ext_fmt : {"%s.COM", "%s.EXE", "%s.BAT"}) {
				safe_sprintf(which_ret, ext_fmt, path);
				if (DOS_FileExists(which_ret))
					return which_ret;
			}
		}
	}
	return 0;
}
