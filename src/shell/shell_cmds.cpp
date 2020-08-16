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

#include "shell.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <vector>

#include "callback.h"
#include "regs.h"
#include "bios.h"
#include "drives.h"
#include "support.h"
#include "control.h"
#include "paging.h"
#include "../ints/int10.h"

// clang-format off
static SHELL_Cmd cmd_list[] = {
	{ "ATTRIB",   1, &DOS_Shell::CMD_ATTRIB,   "SHELL_CMD_ATTRIB_HELP" },
	{ "CALL",     1, &DOS_Shell::CMD_CALL,     "SHELL_CMD_CALL_HELP" },
	{ "CD",       0, &DOS_Shell::CMD_CHDIR,    "SHELL_CMD_CHDIR_HELP" },
	{ "CHDIR",    1, &DOS_Shell::CMD_CHDIR,    "SHELL_CMD_CHDIR_HELP" },
	{ "CHOICE",   1, &DOS_Shell::CMD_CHOICE,   "SHELL_CMD_CHOICE_HELP" },
	{ "CLS",      0, &DOS_Shell::CMD_CLS,      "SHELL_CMD_CLS_HELP" },
	{ "COPY",     0, &DOS_Shell::CMD_COPY,     "SHELL_CMD_COPY_HELP" },
	{ "DATE",     0, &DOS_Shell::CMD_DATE,     "SHELL_CMD_DATE_HELP" },
	{ "DEL",      0, &DOS_Shell::CMD_DELETE,   "SHELL_CMD_DELETE_HELP" },
	{ "DELETE",   1, &DOS_Shell::CMD_DELETE,   "SHELL_CMD_DELETE_HELP" },
	{ "DIR",      0, &DOS_Shell::CMD_DIR,      "SHELL_CMD_DIR_HELP" },
	{ "ECHO",     1, &DOS_Shell::CMD_ECHO,     "SHELL_CMD_ECHO_HELP" },
	{ "ERASE",    1, &DOS_Shell::CMD_DELETE,   "SHELL_CMD_DELETE_HELP" },
	{ "EXIT",     0, &DOS_Shell::CMD_EXIT,     "SHELL_CMD_EXIT_HELP" },
	{ "GOTO",     1, &DOS_Shell::CMD_GOTO,     "SHELL_CMD_GOTO_HELP" },
	{ "HELP",     1, &DOS_Shell::CMD_HELP,     "SHELL_CMD_HELP_HELP" },
	{ "IF",       1, &DOS_Shell::CMD_IF,       "SHELL_CMD_IF_HELP" },
	{ "LH",       1, &DOS_Shell::CMD_LOADHIGH, "SHELL_CMD_LOADHIGH_HELP" },
	{ "LOADHIGH", 1, &DOS_Shell::CMD_LOADHIGH, "SHELL_CMD_LOADHIGH_HELP" },
	{ "LS",       0, &DOS_Shell::CMD_LS,       "SHELL_CMD_LS_HELP" },
	{ "MD",       0, &DOS_Shell::CMD_MKDIR,    "SHELL_CMD_MKDIR_HELP" },
	{ "MKDIR",    1, &DOS_Shell::CMD_MKDIR,    "SHELL_CMD_MKDIR_HELP" },
	{ "PATH",     1, &DOS_Shell::CMD_PATH,     "SHELL_CMD_PATH_HELP" },
	{ "PAUSE",    1, &DOS_Shell::CMD_PAUSE,    "SHELL_CMD_PAUSE_HELP" },
	{ "RD",       0, &DOS_Shell::CMD_RMDIR,    "SHELL_CMD_RMDIR_HELP" },
	{ "REM",      1, &DOS_Shell::CMD_REM,      "SHELL_CMD_REM_HELP" },
	{ "REN",      0, &DOS_Shell::CMD_RENAME,   "SHELL_CMD_RENAME_HELP" },
	{ "RENAME",   1, &DOS_Shell::CMD_RENAME,   "SHELL_CMD_RENAME_HELP" },
	{ "RMDIR",    1, &DOS_Shell::CMD_RMDIR,    "SHELL_CMD_RMDIR_HELP" },
	{ "SET",      1, &DOS_Shell::CMD_SET,      "SHELL_CMD_SET_HELP" },
	{ "SHIFT",    1, &DOS_Shell::CMD_SHIFT,    "SHELL_CMD_SHIFT_HELP" },
	{ "SUBST",    1, &DOS_Shell::CMD_SUBST,    "SHELL_CMD_SUBST_HELP" },
	{ "TIME",     0, &DOS_Shell::CMD_TIME,     "SHELL_CMD_TIME_HELP" },
	{ "TYPE",     0, &DOS_Shell::CMD_TYPE,     "SHELL_CMD_TYPE_HELP" },
	{ "VER",      0, &DOS_Shell::CMD_VER,      "SHELL_CMD_VER_HELP" },
	{ 0, 0, 0, 0 }
};
// clang-format on

/* support functions */
static char empty_char = 0;
static char* empty_string = &empty_char;
static void StripSpaces(char*&args) {
	while (args && *args && isspace(*reinterpret_cast<unsigned char*>(args)))
		args++;
}

static void StripSpaces(char*&args,char also) {
	while (args && *args && (isspace(*reinterpret_cast<unsigned char*>(args)) || (*args == also)))
		args++;
}

static char *ExpandDot(const char *args, char *buffer, size_t bufsize)
{
	if (*args == '.') {
		if (*(args+1) == 0){
			safe_strncpy(buffer, "*.*", bufsize);
			return buffer;
		}
		if ( (*(args+1) != '.') && (*(args+1) != '\\') ) {
			buffer[0] = '*';
			buffer[1] = 0;
			if (bufsize > 2) strncat(buffer,args,bufsize - 1 /*used buffer portion*/ - 1 /*trailing zero*/  );
			return buffer;
		} else
			safe_strncpy (buffer, args, bufsize);
	}
	else safe_strncpy(buffer,args, bufsize);
	return buffer;
}

bool DOS_Shell::CheckConfig(char *cmd_in, char *line) {
	Section* test = control->GetSectionFromProperty(cmd_in);
	if (!test)
		return false;

	if (line && !line[0]) {
		std::string val = test->GetPropValue(cmd_in);
		if (val != NO_SUCH_PROPERTY)
			WriteOut("%s\n", val.c_str());
		return true;
	}
	char newcom[1024];
	snprintf(newcom, sizeof(newcom), "z:\\config -set %s %s%s",
	         test->GetName(),
	         cmd_in,
	         line ? line : "");
	DoCommand(newcom);
	return true;
}

void DOS_Shell::DoCommand(char * line) {
/* First split the line into command and arguments */
	line=trim(line);
	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write=cmd_buffer;
	while (*line) {
		if (*line == 32) break;
		if (*line == '/') break;
		if (*line == '\t') break;
		if (*line == '=') break;
//		if (*line == ':') break; //This breaks drive switching as that is handled at a later stage.
		if ((*line == '.') ||(*line == '\\')) {  //allow stuff like cd.. and dir.exe cd\kees
			*cmd_write=0;
			Bit32u cmd_index=0;
			while (cmd_list[cmd_index].name) {
				if (strcasecmp(cmd_list[cmd_index].name,cmd_buffer) == 0) {
					(this->*(cmd_list[cmd_index].handler))(line);
			 		return;
				}
				cmd_index++;
			}
		}
		*cmd_write++=*line++;
	}
	*cmd_write=0;
	if (strlen(cmd_buffer) == 0) return;
/* Check the internal list */
	Bit32u cmd_index=0;
	while (cmd_list[cmd_index].name) {
		if (strcasecmp(cmd_list[cmd_index].name,cmd_buffer) == 0) {
			(this->*(cmd_list[cmd_index].handler))(line);
			return;
		}
		cmd_index++;
	}
/* This isn't an internal command execute it */
	if (Execute(cmd_buffer,line)) return;
	if (CheckConfig(cmd_buffer,line)) return;
	WriteOut(MSG_Get("SHELL_EXECUTE_ILLEGAL_COMMAND"),cmd_buffer);
}

#define HELP(command) \
	if (ScanCMDBool(args,"?")) { \
		WriteOut(MSG_Get("SHELL_CMD_" command "_HELP")); \
		const char* long_m = MSG_Get("SHELL_CMD_" command "_HELP_LONG"); \
		WriteOut("\n"); \
		if (strcmp("Message not Found!\n",long_m)) WriteOut(long_m); \
		else WriteOut(command "\n"); \
		return; \
	}

void DOS_Shell::CMD_CLS(char * args) {
	HELP("CLS");
	reg_ax=0x0003;
	CALLBACK_RunRealInt(0x10);
}

void DOS_Shell::CMD_DELETE(char * args) {
	HELP("DELETE");
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	/* If delete accept switches mind the space infront of them. See the dir /p code */

	char full[DOS_PATHLENGTH];
	char buffer[CROSS_LEN];
	args = ExpandDot(args,buffer, CROSS_LEN);
	StripSpaces(args);
	if (!DOS_Canonicalize(args,full)) { WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));return; }
//TODO Maybe support confirmation for *.* like dos does.
	bool res=DOS_FindFirst(args,0xffff & ~DOS_ATTR_VOLUME);
	if (!res) {
		WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),args);
		dos.dta(save_dta);
		return;
	}
	//end can't be 0, but if it is we'll get a nice crash, who cares :)
	char * end=strrchr(full,'\\')+1;*end=0;
	char name[DOS_NAMELENGTH_ASCII];Bit32u size;Bit16u time,date;Bit8u attr;
	DOS_DTA dta(dos.dta());
	while (res) {
		dta.GetResult(name,size,date,time,attr);
		if (!(attr & (DOS_ATTR_DIRECTORY|DOS_ATTR_READ_ONLY))) {
			strcpy(end,name);
			if (!DOS_UnlinkFile(full)) WriteOut(MSG_Get("SHELL_CMD_DEL_ERROR"),full);
		}
		res=DOS_FindNext();
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_HELP(char * args){
	HELP("HELP");
	bool optall=ScanCMDBool(args,"ALL");
	/* Print the help */
	if (!optall) WriteOut(MSG_Get("SHELL_CMD_HELP"));
	Bit32u cmd_index=0,write_count=0;
	while (cmd_list[cmd_index].name) {
		if (optall || !cmd_list[cmd_index].flags) {
			WriteOut("<\033[34;1m%-8s\033[0m> %s",cmd_list[cmd_index].name,MSG_Get(cmd_list[cmd_index].help));
			if (!(++write_count % 24))
				CMD_PAUSE(empty_string);
		}
		cmd_index++;
	}
}

void DOS_Shell::CMD_RENAME(char * args){
	HELP("RENAME");
	StripSpaces(args);
	if (!*args) {SyntaxError();return;}
	if ((strchr(args,'*')!=NULL) || (strchr(args,'?')!=NULL) ) { WriteOut(MSG_Get("SHELL_CMD_NO_WILD"));return;}
	char * arg1=StripWord(args);
	StripSpaces(args);
	if (!*args) {SyntaxError();return;}
	char* slash = strrchr(arg1,'\\');
	if (slash) {
		/* If directory specified (crystal caves installer)
		 * rename from c:\X : rename c:\abc.exe abc.shr.
		 * File must appear in C:\
		 * Ren X:\A\B C => ren X:\A\B X:\A\C */

		char dir_source[DOS_PATHLENGTH + 4] = {0}; //not sure if drive portion is included in pathlength
		//Copy first and then modify, makes GCC happy
		safe_strncpy(dir_source,arg1,DOS_PATHLENGTH + 4);
		char* dummy = strrchr(dir_source,'\\');
		if (!dummy) { //Possible due to length
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			return;
		}
		dummy++;
		*dummy = 0;

		//Maybe check args for directory, as I think that isn't allowed

		//dir_source and target are introduced for when we support multiple files being renamed.
		char target[DOS_PATHLENGTH+CROSS_LEN + 5] = {0};
		safe_strcpy(target, dir_source);
		strncat(target,args,CROSS_LEN);

		DOS_Rename(arg1,target);

	} else {
		DOS_Rename(arg1,args);
	}
}

void DOS_Shell::CMD_ECHO(char * args){
	if (!*args) {
		if (echo) { WriteOut(MSG_Get("SHELL_CMD_ECHO_ON"));}
		else { WriteOut(MSG_Get("SHELL_CMD_ECHO_OFF"));}
		return;
	}
	char buffer[512];
	char* pbuffer = buffer;
	safe_strncpy(buffer,args,512);
	StripSpaces(pbuffer);
	if (strcasecmp(pbuffer,"OFF") == 0) {
		echo=false;
		return;
	}
	if (strcasecmp(pbuffer,"ON") == 0) {
		echo=true;
		return;
	}
	if (strcasecmp(pbuffer,"/?") == 0) { HELP("ECHO"); }

	args++;//skip first character. either a slash or dot or space
	size_t len = strlen(args); //TODO check input of else ook nodig is.
	if (len && args[len - 1] == '\r') {
		LOG(LOG_MISC,LOG_WARN)("Hu ? carriage return already present. Is this possible?");
		WriteOut("%s\n",args);
	} else WriteOut("%s\r\n",args);
}

void DOS_Shell::CMD_EXIT(char *args)
{
	HELP("EXIT");
	exit_flag = true;
}

void DOS_Shell::CMD_CHDIR(char * args) {
	HELP("CHDIR");
	StripSpaces(args);
	Bit8u drive = DOS_GetDefaultDrive()+'A';
	char dir[DOS_PATHLENGTH];
	if (!*args) {
		DOS_GetCurrentDir(0,dir);
		WriteOut("%c:\\%s\n",drive,dir);
	} else if (strlen(args) == 2 && args[1] == ':') {
		Bit8u targetdrive = (args[0] | 0x20) - 'a' + 1;
		unsigned char targetdisplay = *reinterpret_cast<unsigned char*>(&args[0]);
		if (!DOS_GetCurrentDir(targetdrive,dir)) {
			if (drive == 'Z') {
				WriteOut(MSG_Get("SHELL_EXECUTE_DRIVE_NOT_FOUND"),toupper(targetdisplay));
			} else {
				WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			}
			return;
		}
		WriteOut("%c:\\%s\n",toupper(targetdisplay),dir);
		if (drive == 'Z')
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT"),toupper(targetdisplay));
	} else 	if (!DOS_ChangeDir(args)) {
		/* Changedir failed. Check if the filename is longer then 8 and/or contains spaces */

		std::string temps(args),slashpart;
		std::string::size_type separator = temps.find_first_of("\\/");
		if (!separator) {
			slashpart = temps.substr(0,1);
			temps.erase(0,1);
		}
		separator = temps.find_first_of("\\/");
		if (separator != std::string::npos) temps.erase(separator);
		separator = temps.rfind('.');
		if (separator != std::string::npos) temps.erase(separator);
		separator = temps.find(' ');
		if (separator != std::string::npos) {/* Contains spaces */
			temps.erase(separator);
			if (temps.size() >6) temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else if (temps.size()>8) {
			temps.erase(6);
			temps += "~1";
			WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_2"),temps.insert(0,slashpart).c_str());
		} else {
			if (drive == 'Z') {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_HINT_3"));
			} else {
				WriteOut(MSG_Get("SHELL_CMD_CHDIR_ERROR"),args);
			}
		}
	}
}

void DOS_Shell::CMD_MKDIR(char * args) {
	HELP("MKDIR");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_MakeDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_MKDIR_ERROR"),args);
	}
}

void DOS_Shell::CMD_RMDIR(char * args) {
	HELP("RMDIR");
	StripSpaces(args);
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}
	if (!DOS_RemoveDir(args)) {
		WriteOut(MSG_Get("SHELL_CMD_RMDIR_ERROR"),args);
	}
}

static void FormatNumber(Bit32u num,char * buf) {
	Bit32u numm,numk,numb,numg;
	numb=num % 1000;
	num/=1000;
	numk=num % 1000;
	num/=1000;
	numm=num % 1000;
	num/=1000;
	numg=num;
	if (numg) {
		sprintf(buf,"%d,%03d,%03d,%03d",numg,numm,numk,numb);
		return;
	};
	if (numm) {
		sprintf(buf,"%d,%03d,%03d",numm,numk,numb);
		return;
	};
	if (numk) {
		sprintf(buf,"%d,%03d",numk,numb);
		return;
	};
	sprintf(buf,"%d",numb);
}

struct DtaResult {
	char name[DOS_NAMELENGTH_ASCII];
	Bit32u size;
	Bit16u date;
	Bit16u time;
	Bit8u attr;

	static bool compareName(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.name, rhs.name) < 0; }
	static bool compareExt(const DtaResult &lhs, const DtaResult &rhs) { return strcmp(lhs.getExtension(), rhs.getExtension()) < 0; }
	static bool compareSize(const DtaResult &lhs, const DtaResult &rhs) { return lhs.size < rhs.size; }
	static bool compareDate(const DtaResult &lhs, const DtaResult &rhs) { return lhs.date < rhs.date || (lhs.date == rhs.date && lhs.time < rhs.time); }

	const char * getExtension() const {
		const char * ext = empty_string;
		if (name[0] != '.') {
			ext = strrchr(name, '.');
			if (!ext) ext = empty_string;
		}
		return ext;
	}

};

static std::string to_search_pattern(const char *arg)
{
	std::string pattern = arg;
	trim(pattern);

	const char last_char = (pattern.length() > 0 ? pattern.back() : '\0');
	switch (last_char) {
	case '\0': // No arguments, search for all.
		pattern = "*.*";
		break;
	case '\\': // Handle \, C:\, etc.
	case ':':  // Handle C:, etc.
		pattern += "*.*";
		break;
	default: break;
	}

	// Handle patterns starting with a dot.
	char buffer[CROSS_LEN];
	pattern = ExpandDot(pattern.c_str(), buffer, sizeof(buffer));

	// When there's no wildcard and target is a directory then search files
	// inside the directory.
	const char *p = pattern.c_str();
	if (!strrchr(p, '*') && !strrchr(p, '?')) {
		uint16_t attr = 0;
		if (DOS_GetFileAttr(p, &attr) && (attr & DOS_ATTR_DIRECTORY))
			pattern += "\\*.*";
	}

	// If no extension, list all files.
	// This makes patterns like foo* work.
	if (!strrchr(pattern.c_str(), '.'))
		pattern += ".*";

	return pattern;
}

// Map a vector of dir contents to a vector of word widths.
static std::vector<int> to_name_lengths(const std::vector<DtaResult> &dir_contents,
                                        int padding)
{
	std::vector<int> ret;
	ret.reserve(dir_contents.size());
	for (const auto &entry : dir_contents) {
		const int len = static_cast<int>(strlen(entry.name));
		ret.push_back(len + padding);
	}
	return ret;
}

static std::vector<int> calc_column_widths(const std::vector<int> &word_widths,
                                           int min_col_width)
{
	assert(min_col_width > 0);

	// Actual terminal width (number of text columns) using current text
	// mode; in practice it's either 40, 80, or 132.
	const int term_width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);

	// Use term_width-1 because we never want to print line up to the actual
	// limit; this would cause unnecessary line wrapping
	const size_t max_columns = (term_width - 1) / min_col_width;
	std::vector<int> col_widths(max_columns);

	// This function returns true when column number is too high to fit
	// words into a terminal width.  If it returns false, then the first
	// coln integers in col_widths vector describe the column widths.
	auto too_many_columns = [&](size_t coln) -> bool {
		std::fill(col_widths.begin(), col_widths.end(), 0);
		if (coln <= 1)
			return false;
		int max_line_width = 0; // tally of the longest line
		int c = 0;              // current columnt
		for (const int width : word_widths) {
			const int old_col_width = col_widths[c];
			const int new_col_width = std::max(old_col_width, width);
			col_widths[c] = new_col_width;
			max_line_width += (new_col_width - old_col_width);
			if (max_line_width >= term_width)
				return true;
			c = (c + 1) % coln;
		}
		return false;
	};

	size_t col_count = max_columns;
	while (too_many_columns(col_count)) {
		col_count--;
		col_widths.pop_back();
	}
	return col_widths;
}

void DOS_Shell::CMD_DIR(char * args) {
	HELP("DIR");
	char numformat[16];

	std::string line;
	if (GetEnvStr("DIRCMD",line)){
		std::string::size_type idx = line.find('=');
		std::string value=line.substr(idx +1 , std::string::npos);
		line = std::string(args) + " " + value;
		args=const_cast<char*>(line.c_str());
	}

	bool optW=ScanCMDBool(args,"W");
	ScanCMDBool(args,"S");
	bool optP=ScanCMDBool(args,"P");
	if (ScanCMDBool(args,"WP") || ScanCMDBool(args,"PW")) {
		optW=optP=true;
	}
	bool optB=ScanCMDBool(args,"B");
	bool optAD=ScanCMDBool(args,"AD");
	bool optAminusD=ScanCMDBool(args,"A-D");
	// Sorting flags
	bool reverseSort = false;
	bool optON=ScanCMDBool(args,"ON");
	if (ScanCMDBool(args,"O-N")) {
		optON = true;
		reverseSort = true;
	}
	bool optOD=ScanCMDBool(args,"OD");
	if (ScanCMDBool(args,"O-D")) {
		optOD = true;
		reverseSort = true;
	}
	bool optOE=ScanCMDBool(args,"OE");
	if (ScanCMDBool(args,"O-E")) {
		optOE = true;
		reverseSort = true;
	}
	bool optOS=ScanCMDBool(args,"OS");
	if (ScanCMDBool(args,"O-S")) {
		optOS = true;
		reverseSort = true;
	}
	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		return;
	}

	const std::string pattern = to_search_pattern(args);

	/* Make a full path in the args */
	char path[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(pattern.c_str(), path)) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
		return;
	}

	// DIR cmd in DOS and cmd.exe format 'Directory of <path>'
	// accordingly:
	// - only directory part of pattern passed as an argument
	// - do not append '\' to the directory name
	// - for root directories/drives: append '\' to the name
	char *last_dir_sep = strrchr(path, '\\');
	if (last_dir_sep == path + 2)
		*(last_dir_sep + 1) = '\0';
	else
		*last_dir_sep = '\0';

	const char drive_letter = path[0];
	const auto drive_idx = drive_index(drive_letter);
	const bool print_label = (drive_letter >= 'A') && Drives[drive_idx];
	unsigned p_count = 0; // line counter for 'pause' command

	if (!optB) {
		if (print_label) {
			const char *label = Drives[drive_idx]->GetLabel();
			WriteOut(MSG_Get("SHELL_CMD_DIR_VOLUME"), drive_letter, label);
			p_count += 1;
		}
		WriteOut(MSG_Get("SHELL_CMD_DIR_INTRO"), path);
		WriteOut_NoParsing("\n");
		p_count += 2;
	}

	// Helper function to handle 'Press any key to continue' message
	// regardless of user-selected formatting of DIR command output.
	//
	// Call it whenever a newline gets printed to potentially display
	// this one-line message.
	//
	// For some strange reason number of columns stored in BIOS segment
	// is exact, while number of rows is 0-based (so 80x25 mode is
	// represented as 80x24).  It's convenient for us, as it means we can
	// get away with (p_count % term_rows) instead of
	// (p_count % (term_rows - 1)).
	//
	const int term_rows = real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS);
	auto show_press_any_key = [&]() {
		p_count += 1;
		if (optP && (p_count % term_rows) == 0)
			CMD_PAUSE(empty_string);
	};

	const bool is_root = strnlen(path, sizeof(path)) == 3;

	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	bool ret = DOS_FindFirst(pattern.c_str(), 0xffff & ~DOS_ATTR_VOLUME);
	if (!ret) {
		if (!optB)
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),
			         pattern.c_str());
		dos.dta(save_dta);
		return;
	}

	std::vector<DtaResult> results;
	// TODO OS should be asked for a number of files and appropriate
	// vector capacity should be set

	do {    /* File name and extension */
		DtaResult result;
		dta.GetResult(result.name,result.size,result.date,result.time,result.attr);

		/* Skip non-directories if option AD is present, or skip dirs in case of A-D */
		if (optAD && !(result.attr&DOS_ATTR_DIRECTORY) ) continue;
		else if (optAminusD && (result.attr&DOS_ATTR_DIRECTORY) ) continue;

		results.push_back(result);

	} while ( (ret=DOS_FindNext()) );

	if (optON) {
		// Sort by name
		std::sort(results.begin(), results.end(), DtaResult::compareName);
	} else if (optOE) {
		// Sort by extension
		std::sort(results.begin(), results.end(), DtaResult::compareExt);
	} else if (optOD) {
		// Sort by date
		std::sort(results.begin(), results.end(), DtaResult::compareDate);
	} else if (optOS) {
		// Sort by size
		std::sort(results.begin(), results.end(), DtaResult::compareSize);
	}
	if (reverseSort) {
		std::reverse(results.begin(), results.end());
	}

	uint32_t byte_count = 0;
	uint32_t file_count = 0;
	uint32_t dir_count = 0;
	unsigned w_count = 0;

	for (auto &entry : results) {

		char *name = entry.name;
		const uint32_t size = entry.size;
		const uint16_t date = entry.date;
		const uint16_t time = entry.time;
		const bool is_dir = entry.attr & DOS_ATTR_DIRECTORY;

		// Skip listing . and .. from toplevel directory, to simulate
		// DIR output correctly.
		// Bare format never lists .. nor . as directories.
		if (is_root || optB) {
			if (strcmp(".", name) == 0 || strcmp("..", name) == 0)
				continue;
		}

		if (is_dir) {
			dir_count += 1;
		} else {
			file_count += 1;
			byte_count += size;
		}

		// 'Bare' format: just the name, one per line, nothing else
		//
		if (optB) {
			WriteOut("%s\n", name);
			show_press_any_key();
			continue;
		}

		// 'Wide list' format: using several columns
		//
		if (optW) {
			if (is_dir) {
				const int length = static_cast<int>(strlen(name));
				WriteOut("[%s]%*s", name, (14 - length), "");
			} else {
				WriteOut("%-16s", name);
			}
			w_count += 1;
			if ((w_count % 5) == 0)
				show_press_any_key();
			continue;
		}

		// default format: one detailed entry per line
		//
		const auto year   = static_cast<uint16_t>((date >> 9) + 1980);
		const auto month  = static_cast<uint8_t>((date >> 5) & 0x000f);
		const auto day    = static_cast<uint8_t>(date & 0x001f);
		const auto hour   = static_cast<uint8_t>((time >> 5) >> 6);
		const auto minute = static_cast<uint8_t>((time >> 5) & 0x003f);

		char *ext = empty_string;
		if ((name[0] != '.')) {
			ext = strrchr(name, '.');
			if (ext) {
				*ext = '\0';
				ext++;
			} else {
				// prevent (null) from appearing
				ext = empty_string;
			}
		}

		if (is_dir) {
			WriteOut("%-8s %-3s   %-16s %02d-%02d-%04d %2d:%02d\n",
			         name, ext, "<DIR>", day, month, year, hour, minute);
		} else {
			FormatNumber(size, numformat);
			WriteOut("%-8s %-3s   %16s %02d-%02d-%04d %2d:%02d\n",
			         name, ext, numformat, day, month, year, hour, minute);
		}
		show_press_any_key();
	}

	// Additional newline in case last line in 'Wide list` format was
	// not wrapped automatically.
	if (optW && (w_count % 5)) {
		WriteOut("\n");
		show_press_any_key();
	}

	// Show the summary of results
	if (!optB) {
		FormatNumber(byte_count, numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_USED"), file_count, numformat);
		show_press_any_key();

		Bit8u drive = dta.GetSearchDrive();
		Bitu free_space = 1024 * 1024 * 100;
		if (Drives[drive]) {
			Bit16u bytes_sector;
			Bit8u  sectors_cluster;
			Bit16u total_clusters;
			Bit16u free_clusters;
			Drives[drive]->AllocationInfo(&bytes_sector,
			                              &sectors_cluster,
			                              &total_clusters,
			                              &free_clusters);
			free_space = bytes_sector * sectors_cluster * free_clusters;
		}
		FormatNumber(free_space, numformat);
		WriteOut(MSG_Get("SHELL_CMD_DIR_BYTES_FREE"), dir_count, numformat);
	}
	dos.dta(save_dta);
}

void DOS_Shell::CMD_LS(char *args)
{
	using namespace std::string_literals;

	HELP("LS");

	const RealPt original_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());

	const std::string pattern = to_search_pattern(args);
	if (!DOS_FindFirst(pattern.c_str(), 0xffff & ~DOS_ATTR_VOLUME)) {
		WriteOut(MSG_Get("SHELL_CMD_LS_PATH_ERR"), trim(args));
		dos.dta(original_dta);
		return;
	}

	std::vector<DtaResult> dir_contents;
	// reserve space for as many as we can fit into a single memory page
	// nothing more to it; make it larger if necessary
	dir_contents.reserve(MEM_PAGE_SIZE / sizeof(DtaResult));

	do {
		DtaResult result;
		dta.GetResult(result.name, result.size, result.date,
		              result.time, result.attr);
		if (result.name == "."s || result.name == ".."s)
			continue;
		dir_contents.push_back(result);
	} while (DOS_FindNext());

	const int column_sep = 2; // chars separating columns
	const auto word_widths = to_name_lengths(dir_contents, column_sep);
	const auto column_widths = calc_column_widths(word_widths, column_sep + 1);
	const size_t cols = column_widths.size();

	size_t w_count = 0;

	for (const auto &entry : dir_contents) {
		std::string name = entry.name;
		const bool is_dir = entry.attr & DOS_ATTR_DIRECTORY;
		const size_t col = w_count % cols;
		const int cw = column_widths[col];

		if (is_dir) {
			upcase(name);
			WriteOut("\033[34;1m%-*s\033[0m", cw, name.c_str());
		} else {
			lowcase(name);
			if (is_executable_filename(name))
				WriteOut("\033[32;1m%-*s\033[0m", cw, name.c_str());
			else
				WriteOut("%-*s", cw, name.c_str());
		}

		++w_count;
		if (w_count % cols == 0)
			WriteOut_NoParsing("\n");
	}
	dos.dta(original_dta);
}

struct copysource {
	std::string filename = "";
	bool concat = false;

	copysource() = default;

	copysource(const std::string &file, bool cat)
	        : filename(file),
	          concat(cat)
	{}
};

void DOS_Shell::CMD_COPY(char * args) {
	HELP("COPY");
	static char defaulttarget[] = ".";
	StripSpaces(args);
	/* Command uses dta so set it to our internal dta */
	RealPt save_dta=dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	Bit32u size;Bit16u date;Bit16u time;Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII];
	std::vector<copysource> sources;
	// ignore /b and /t switches: always copy binary
	while (ScanCMDBool(args,"B")) ;
	while (ScanCMDBool(args,"T")) ; //Shouldn't this be A ?
	while (ScanCMDBool(args,"A")) ;
	ScanCMDBool(args,"Y");
	ScanCMDBool(args,"-Y");
	ScanCMDBool(args,"V");

	char * rem=ScanCMDRemain(args);
	if (rem) {
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
		dos.dta(save_dta);
		return;
	}
	// Gather all sources (extension to copy more then 1 file specified at command line)
	// Concatenating files go as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char* source_p = NULL;
	char source_x[DOS_PATHLENGTH+CROSS_LEN];
	while ( (source_p = StripWord(args)) && *source_p ) {
		do {
			char* plus = strchr(source_p,'+');
			// If StripWord() previously cut at a space before a plus then
			// set concatenate flag on last source and remove leading plus.
			if (plus == source_p && sources.size()) {
				sources[sources.size() - 1].concat = true;
				// If spaces also followed plus then item is only a plus.
				if (strlen(++source_p) == 0) break;
				plus = strchr(source_p,'+');
			}
			if (plus) *plus++ = 0;
			safe_strncpy(source_x,source_p,CROSS_LEN);
			bool has_drive_spec = false;
			size_t source_x_len = strlen(source_x);
			if (source_x_len>0) {
				if (source_x[source_x_len-1] == ':') has_drive_spec = true;
			}
			if (!has_drive_spec  && !strpbrk(source_p,"*?") ) { //doubt that fu*\*.* is valid
				if (DOS_FindFirst(source_p,0xffff & ~DOS_ATTR_VOLUME)) {
					dta.GetResult(name,size,date,time,attr);
					if (attr & DOS_ATTR_DIRECTORY)
						strcat(source_x,"\\*.*");
				}
			}
			sources.push_back(copysource(source_x,(plus)?true:false));
			source_p = plus;
		} while (source_p && *source_p);
	}
	// At least one source has to be there
	if (!sources.size() || !sources[0].filename.size()) {
		WriteOut(MSG_Get("SHELL_MISSING_PARAMETER"));
		dos.dta(save_dta);
		return;
	};

	copysource target;
	// If more then one object exists and last target is not part of a
	// concat sequence then make it the target.
	if (sources.size() > 1 && !sources[sources.size() - 2].concat){
		target = sources.back();
		sources.pop_back();
	}
	//If no target => default target with concat flag true to detect a+b+c
	if (target.filename.size() == 0) target = copysource(defaulttarget,true);

	copysource oldsource;
	copysource source;
	Bit32u count = 0;
	while (sources.size()) {
		/* Get next source item and keep track of old source for concat start end */
		oldsource = source;
		source = sources[0];
		sources.erase(sources.begin());

		//Skip first file if doing a+b+c. Set target to first file
		if (!oldsource.concat && source.concat && target.concat) {
			target = source;
			continue;
		}

		/* Make a full path in the args */
		char pathSource[DOS_PATHLENGTH];
		char pathTarget[DOS_PATHLENGTH];

		if (!DOS_Canonicalize(const_cast<char*>(source.filename.c_str()),pathSource)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		// cut search pattern
		char* pos = strrchr(pathSource,'\\');
		if (pos) *(pos+1) = 0;

		if (!DOS_Canonicalize(const_cast<char*>(target.filename.c_str()),pathTarget)) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
		}
		char* temp = strstr(pathTarget,"*.*");
		if (temp) *temp = 0;//strip off *.* from target

		// add '\\' if target is a directory
		bool target_is_file = true;
		if (pathTarget[strlen(pathTarget) - 1]!='\\') {
			if (DOS_FindFirst(pathTarget,0xffff & ~DOS_ATTR_VOLUME)) {
				dta.GetResult(name,size,date,time,attr);
				if (attr & DOS_ATTR_DIRECTORY) {
					strcat(pathTarget,"\\");
					target_is_file = false;
				}
			}
		} else target_is_file = false;

		//Find first sourcefile
		bool ret = DOS_FindFirst(const_cast<char*>(source.filename.c_str()),0xffff & ~DOS_ATTR_VOLUME);
		if (!ret) {
			WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),const_cast<char*>(source.filename.c_str()));
			dos.dta(save_dta);
			return;
		}

		Bit16u sourceHandle,targetHandle;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH];

		bool second_file_of_current_source = false;
		while (ret) {
			dta.GetResult(name,size,date,time,attr);

			if ((attr & DOS_ATTR_DIRECTORY) == 0) {
				safe_strcpy(nameSource, pathSource);
				strcat(nameSource,name);
				// Open Source
				if (DOS_OpenFile(nameSource,0,&sourceHandle)) {
					// Create Target or open it if in concat mode
					safe_strcpy(nameTarget, pathTarget);
					if (nameTarget[strlen(nameTarget) - 1] == '\\') strcat(nameTarget,name);

					//Special variable to ensure that copy * a_file, where a_file is not a directory concats.
					bool special = second_file_of_current_source && target_is_file;
					second_file_of_current_source = true;
					if (special) oldsource.concat = true;
					//Don't create a new file when in concat mode
					if (oldsource.concat || DOS_CreateFile(nameTarget,0,&targetHandle)) {
						Bit32u dummy=0;
						//In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget,OPEN_READWRITE,&targetHandle) &&
					        	                  DOS_SeekFile(targetHandle,&dummy,DOS_SEEK_END))) {
							// Copy
							static Bit8u buffer[0x8000]; // static, otherwise stack overflow possible.
							uint16_t toread = 0x8000;
							do {
								DOS_ReadFile(sourceHandle, buffer, &toread);
								DOS_WriteFile(targetHandle, buffer, &toread);
							} while (toread == 0x8000);
							DOS_CloseFile(sourceHandle);
							DOS_CloseFile(targetHandle);
							WriteOut(" %s\n",name);
							if (!source.concat && !special) count++; //Only count concat files once
						} else {
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
						}
					} else {
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(target.filename.c_str()));
					}
				} else WriteOut(MSG_Get("SHELL_CMD_COPY_FAILURE"),const_cast<char*>(source.filename.c_str()));
			};
			//On to the next file if the previous one wasn't a device
			if ((attr&DOS_ATTR_DEVICE) == 0) ret = DOS_FindNext();
			else ret = false;
		};
	}

	WriteOut(MSG_Get("SHELL_CMD_COPY_SUCCESS"),count);
	dos.dta(save_dta);
}

void DOS_Shell::CMD_SET(char * args) {
	HELP("SET");
	StripSpaces(args);
	std::string line;
	if (!*args) {
		/* No command line show all environment lines */
		Bitu count=GetEnvCount();
		for (Bitu a=0;a<count;a++) {
			if (GetEnvNum(a,line)) WriteOut("%s\n",line.c_str());
		}
		return;
	}
	//There are args:
	char * pcheck = args;
	while ( *pcheck && (*pcheck == ' ' || *pcheck == '\t')) pcheck++;
	if (*pcheck && strlen(pcheck) >3 && (strncasecmp(pcheck,"/p ",3) == 0)) E_Exit("Set /P is not supported. Use Choice!");

	char * p=strpbrk(args, "=");
	if (!p) {
		if (!GetEnvStr(args,line)) WriteOut(MSG_Get("SHELL_CMD_SET_NOT_SET"),args);
		WriteOut("%s\n",line.c_str());
	} else {
		*p++=0;
		/* parse p for envirionment variables */
		char parsed[CMD_MAXLINE];
		char* p_parsed = parsed;
		while (*p) {
			if (*p != '%') *p_parsed++ = *p++; //Just add it (most likely path)
			else if ( *(p+1) == '%') {
				*p_parsed++ = '%'; p += 2; //%% => %
			} else {
				char * second = strchr(++p,'%');
				if (!second) continue;
				*second++ = 0;
				std::string temp;
				if (GetEnvStr(p,temp)) {
					std::string::size_type equals = temp.find('=');
					if (equals == std::string::npos)
						continue;
					const uintptr_t remaining_len = std::min(
					        sizeof(parsed) - static_cast<uintptr_t>(p_parsed - parsed),
					        sizeof(parsed));
					safe_strncpy(p_parsed,
					             temp.substr(equals + 1).c_str(),
					             remaining_len);
					p_parsed += strlen(p_parsed);
				}
				p = second;
			}
		}
		*p_parsed = 0;
		/* Try setting the variable */
		if (!SetEnv(args,parsed)) {
			WriteOut(MSG_Get("SHELL_CMD_SET_OUT_OF_SPACE"));
		}
	}
}

void DOS_Shell::CMD_IF(char * args) {
	HELP("IF");
	StripSpaces(args,'=');
	bool has_not=false;

	while (strncasecmp(args,"NOT",3) == 0) {
		if (!isspace(*reinterpret_cast<unsigned char*>(&args[3])) && (args[3] != '=')) break;
		args += 3;	//skip text
		//skip more spaces
		StripSpaces(args,'=');
		has_not = !has_not;
	}

	if (strncasecmp(args,"ERRORLEVEL",10) == 0) {
		args += 10;	//skip text
		//Strip spaces and ==
		StripSpaces(args,'=');
		char* word = StripWord(args);
		if (!isdigit(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER"));
			return;
		}

		Bit8u n = 0;
		do n = n * 10 + (*word - '0');
		while (isdigit(*++word));
		if (*word && !isspace(*word)) {
			WriteOut(MSG_Get("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER"));
			return;
		}
		/* Read the error code from DOS */
		if ((dos.return_code>=n) ==(!has_not)) DoCommand(args);
		return;
	}

	if (strncasecmp(args,"EXIST ",6) == 0) {
		args += 6; //Skip text
		StripSpaces(args);
		char* word = StripWord(args);
		if (!*word) {
			WriteOut(MSG_Get("SHELL_CMD_IF_EXIST_MISSING_FILENAME"));
			return;
		}

		{	/* DOS_FindFirst uses dta so set it to our internal dta */
			RealPt save_dta=dos.dta();
			dos.dta(dos.tables.tempdta);
			bool ret=DOS_FindFirst(word,0xffff & ~DOS_ATTR_VOLUME);
			dos.dta(save_dta);
			if (ret == (!has_not)) DoCommand(args);
		}
		return;
	}

	/* Normal if string compare */

	char* word1 = args;
	// first word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;
	char* end_word1 = args;

	// scan for =
	while (*args && (*args != '='))
		args++;
	// check for ==
	if ((*args == 0) || (args[1] != '=')) {
		SyntaxError();
		return;
	}
	args += 2;
	StripSpaces(args,'=');

	char* word2 = args;
	// second word is until space or =
	while (*args && !isspace(*reinterpret_cast<unsigned char*>(args)) && (*args != '='))
		args++;

	if (*args) {
		*end_word1 = 0;		// mark end of first word
		*args++ = 0;		// mark end of second word
		StripSpaces(args,'=');

		if ((strcmp(word1,word2) == 0) == (!has_not)) DoCommand(args);
	}
}

void DOS_Shell::CMD_GOTO(char * args) {
	HELP("GOTO");
	StripSpaces(args);
	if (!bf) return;
	if (*args &&(*args == ':')) args++;
	//label ends at the first space
	char* non_space = args;
	while (*non_space) {
		if ((*non_space == ' ') || (*non_space == '\t'))
			*non_space = 0;
		else non_space++;
	}
	if (!*args) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_MISSING_LABEL"));
		return;
	}
	if (!bf->Goto(args)) {
		WriteOut(MSG_Get("SHELL_CMD_GOTO_LABEL_NOT_FOUND"),args);
		return;
	}
}

void DOS_Shell::CMD_SHIFT(char * args ) {
	HELP("SHIFT");
	if (bf) bf->Shift();
}

void DOS_Shell::CMD_TYPE(char * args) {
	HELP("TYPE");
	StripSpaces(args);
	if (!*args) {
		WriteOut(MSG_Get("SHELL_SYNTAXERROR"));
		return;
	}
	Bit16u handle;
	char * word;
nextfile:
	word=StripWord(args);
	if (!DOS_OpenFile(word,0,&handle)) {
		WriteOut(MSG_Get("SHELL_CMD_FILE_NOT_FOUND"),word);
		return;
	}
	Bit16u n;Bit8u c;
	do {
		n=1;
		DOS_ReadFile(handle,&c,&n);
		if (c == 0x1a) break; // stop at EOF
		DOS_WriteFile(STDOUT,&c,&n);
	} while (n);
	DOS_CloseFile(handle);
	if (*args) goto nextfile;
}

void DOS_Shell::CMD_REM(char * args) {
	HELP("REM");
}

void DOS_Shell::CMD_PAUSE(char *args) {
	HELP("PAUSE");
	WriteOut(MSG_Get("SHELL_CMD_PAUSE"));
	uint8_t c;
	uint16_t n = 1;
	DOS_ReadFile(STDIN, &c, &n);
	if (c == 0)
		DOS_ReadFile(STDIN, &c, &n); // read extended key
	WriteOut_NoParsing("\n");
}

void DOS_Shell::CMD_CALL(char * args){
	HELP("CALL");
	this->call=true; /* else the old batchfile will be closed first */
	this->ParseLine(args);
	this->call=false;
}

void DOS_Shell::CMD_DATE(char * args) {
	HELP("DATE");
	if (ScanCMDBool(args,"H")) {
		// synchronize date with host parameter
		time_t curtime;
		struct tm *loctime;
		curtime = time (NULL);
		loctime = localtime (&curtime);

		reg_cx = loctime->tm_year+1900;
		reg_dh = loctime->tm_mon+1;
		reg_dl = loctime->tm_mday;

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		return;
	}
	// check if a date was passed in command line
	Bit32u newday,newmonth,newyear;
	if (sscanf(args,"%u-%u-%u",&newmonth,&newday,&newyear) == 3) {
		reg_cx = static_cast<Bit16u>(newyear);
		reg_dh = static_cast<Bit8u>(newmonth);
		reg_dl = static_cast<Bit8u>(newday);

		reg_ah=0x2b; // set system date
		CALLBACK_RunRealInt(0x21);
		if (reg_al == 0xff) WriteOut(MSG_Get("SHELL_CMD_DATE_ERROR"));
		return;
	}
	// display the current date
	reg_ah=0x2a; // get system date
	CALLBACK_RunRealInt(0x21);

	const char* datestring = MSG_Get("SHELL_CMD_DATE_DAYS");
	Bit32u length;
	char day[6] = {0};
	if (sscanf(datestring,"%u",&length) && (length<5) && (strlen(datestring) == (length*7+1))) {
		// date string appears valid
		for (Bit32u i = 0; i < length; i++) day[i] = datestring[reg_al*length+1+i];
	}
	bool dateonly = ScanCMDBool(args,"T");
	if (!dateonly) WriteOut(MSG_Get("SHELL_CMD_DATE_NOW"));

	const char* formatstring = MSG_Get("SHELL_CMD_DATE_FORMAT");
	if (strlen(formatstring)!=5) return;
	char buffer[15] = {0};
	Bitu bufferptr=0;
	for (Bitu i = 0; i < 5; i++) {
		if (i == 1 || i == 3) {
			buffer[bufferptr] = formatstring[i];
			bufferptr++;
		} else {
			if (formatstring[i] == 'M') bufferptr += sprintf(buffer+bufferptr,"%02u",(Bit8u) reg_dh);
			if (formatstring[i] == 'D') bufferptr += sprintf(buffer+bufferptr,"%02u",(Bit8u) reg_dl);
			if (formatstring[i] == 'Y') bufferptr += sprintf(buffer+bufferptr,"%04u",(Bit16u) reg_cx);
		}
	}
	WriteOut("%s %s\n",day, buffer);
	if (!dateonly) WriteOut(MSG_Get("SHELL_CMD_DATE_SETHLP"));
}

void DOS_Shell::CMD_TIME(char * args) {
	HELP("TIME");
	if (ScanCMDBool(args,"H")) {
		// synchronize time with host parameter
		time_t curtime;
		struct tm *loctime;
		curtime = time (NULL);
		loctime = localtime (&curtime);

		//reg_cx = loctime->;
		//reg_dh = loctime->;
		//reg_dl = loctime->;

		// reg_ah=0x2d; // set system time TODO
		// CALLBACK_RunRealInt(0x21);

		Bit32u ticks=(Bit32u)(((double)(loctime->tm_hour*3600+
										loctime->tm_min*60+
										loctime->tm_sec))*18.206481481);
		mem_writed(BIOS_TIMER,ticks);
		return;
	}
	bool timeonly = ScanCMDBool(args,"T");

	reg_ah=0x2c; // get system time
	CALLBACK_RunRealInt(0x21);
/*
		reg_dl= // 1/100 seconds
		reg_dh= // seconds
		reg_cl= // minutes
		reg_ch= // hours
*/
	if (timeonly) {
		WriteOut("%2u:%02u\n",reg_ch,reg_cl);
	} else {
		WriteOut(MSG_Get("SHELL_CMD_TIME_NOW"));
		WriteOut("%2u:%02u:%02u,%02u\n",reg_ch,reg_cl,reg_dh,reg_dl);
	}
}

void DOS_Shell::CMD_SUBST (char * args) {
/* If more that one type can be substed think of something else
 * E.g. make basedir member dos_drive instead of localdrive
 */
	HELP("SUBST");
	localDrive* ldp=0;
	char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
	char temp_str[2] = { 0,0 };
	try {
		safe_strcpy(mountstring, "MOUNT ");
		StripSpaces(args);
		std::string arg;
		CommandLine command(0,args);

		if (command.GetCount() != 2) throw 0 ;

		command.FindCommand(1,arg);
		if ( (arg.size() > 1) && arg[1] !=':')  throw(0);
		temp_str[0]=(char)toupper(args[0]);
		command.FindCommand(2,arg);
		const auto drive_idx = drive_index(temp_str[0]);
		if ((arg == "/D") || (arg == "/d")) {
			if (!Drives[drive_idx])
				throw 1; // targetdrive not in use
			strcat(mountstring, "-u ");
			strcat(mountstring, temp_str);
			this->ParseLine(mountstring);
			return;
		}
		if (Drives[drive_idx])
			throw 0; // targetdrive in use
		strcat(mountstring, temp_str);
		strcat(mountstring, " ");

   		Bit8u drive;char fulldir[DOS_PATHLENGTH];
		if (!DOS_MakeName(const_cast<char*>(arg.c_str()),fulldir,&drive)) throw 0;

		if ( ( ldp=dynamic_cast<localDrive*>(Drives[drive])) == 0 ) throw 0;
		char newname[CROSS_LEN];
		safe_strcpy(newname, ldp->GetBasedir());
		strcat(newname,fulldir);
		CROSS_FILENAME(newname);
		ldp->dirCache.ExpandName(newname);
		strcat(mountstring,"\"");
		strcat(mountstring, newname);
		strcat(mountstring,"\"");
		this->ParseLine(mountstring);
	}
	catch(int a){
		if (a == 0) {
			WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		} else {
		       	WriteOut(MSG_Get("SHELL_CMD_SUBST_NO_REMOVE"));
		}
		return;
	}
	catch(...) {		//dynamic cast failed =>so no localdrive
		WriteOut(MSG_Get("SHELL_CMD_SUBST_FAILURE"));
		return;
	}

	return;
}

void DOS_Shell::CMD_LOADHIGH(char *args){
	HELP("LOADHIGH");
	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	Bit8u umb_flag=dos_infoblock.GetUMBChainState();
	Bit8u old_memstrat=(Bit8u)(DOS_GetMemAllocStrategy()&0xff);
	if (umb_start == 0x9fff) {
		if ((umb_flag&1) == 0) DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x80);	// search in UMBs first
		this->ParseLine(args);
		Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy
	} else this->ParseLine(args);
}

void DOS_Shell::CMD_CHOICE(char * args){
	HELP("CHOICE");
	static char defchoice[3] = {'y','n',0};
	char *rem = NULL, *ptr;
	bool optN = false;
	bool optS = false;
	if (args) {
		optN = ScanCMDBool(args,"N");
		optS = ScanCMDBool(args,"S"); //Case-sensitive matching
		ScanCMDBool(args,"T"); //Default Choice after timeout
		char *last = strchr(args,0);
		StripSpaces(args);
		rem = ScanCMDRemain(args);

		if (rem && *rem && (tolower(rem[1]) != 'c')) {
			WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"),rem);
			return;
		}
		if (args == rem) {
			assert(args);
			if (rem != nullptr) {
				args = strchr(rem, '\0') + 1;
			}
		}
		if (rem) rem += 2;
		if (rem && rem[0] == ':') rem++; /* optional : after /c */
		if (args > last) args = NULL;
	}
	if (!rem || !*rem) rem = defchoice; /* No choices specified use YN */
	ptr = rem;
	Bit8u c;
	if (!optS) while ((c = *ptr)) *ptr++ = (char)toupper(c); /* When in no case-sensitive mode. make everything upcase */
	if (args && *args ) {
		StripSpaces(args);
		size_t argslen = strlen(args);
		if (argslen > 1 && args[0] == '"' && args[argslen-1] == '"') {
			args[argslen-1] = 0; //Remove quotes
			args++;
		}
		WriteOut(args);
	}
	/* Show question prompt of the form [a,b]? where a b are the choice values */
	if (!optN) {
		if (args && *args) WriteOut(" ");
		WriteOut("[");
		size_t len = strlen(rem);
		for (size_t t = 1; t < len; t++) {
			WriteOut("%c,",rem[t-1]);
		}
		WriteOut("%c]?",rem[len-1]);
	}

	Bit16u n=1;
	do {
		DOS_ReadFile (STDIN,&c,&n);
	} while (!c || !(ptr = strchr(rem,(optS?c:toupper(c)))));
	c = optS?c:(Bit8u)toupper(c);
	DOS_WriteFile(STDOUT, &c, &n);
	WriteOut_NoParsing("\n");
	dos.return_code = (Bit8u)(ptr-rem+1);
}

void DOS_Shell::CMD_ATTRIB(char *args){
	HELP("ATTRIB");
	// No-Op for now.
}

void DOS_Shell::CMD_PATH(char *args){
	HELP("PATH");
	if (args && strlen(args)) {
		char set_path[DOS_PATHLENGTH + CROSS_LEN + 20] = {0};
		while (args && *args && (*args == '='|| *args == ' '))
			args++;
		snprintf(set_path, sizeof(set_path), "set PATH=%s", args);
		this->ParseLine(set_path);
		return;
	} else {
		std::string line;
		if (GetEnvStr("PATH", line))
			WriteOut("%s\n", line.c_str());
		else
			WriteOut("PATH=(null)\n");
	}
}

void DOS_Shell::CMD_VER(char *args)
{
	HELP("VER");
	if (args && strlen(args)) {
		char *word = StripWord(args);
		if (strcasecmp(word, "set"))
			return;
		word = StripWord(args);
		const auto new_version = DOS_ParseVersion(word, args);
		if (new_version.major || new_version.minor) {
			dos.version.major = new_version.major;
			dos.version.minor = new_version.minor;
		} else
			WriteOut(MSG_Get("SHELL_CMD_VER_INVALID"));
	} else
		WriteOut(MSG_Get("SHELL_CMD_VER_VER"), VERSION,
		         dos.version.major, dos.version.minor);
}
