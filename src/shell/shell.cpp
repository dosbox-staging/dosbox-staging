/*
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <list>
#include <memory>

#include "../dos/program_more_output.h"
#include "../dos/program_setver.h"
#include "autoexec.h"
#include "callback.h"
#include "control.h"
#include "fs_utils.h"
#include "mapper.h"
#include "regs.h"
#include "string_utils.h"
#include "support.h"
#include "timer.h"

callback_number_t call_shellstop = 0;
/* Larger scope so shell_del autoexec can use it to
 * remove things from the environment */
DOS_Shell *first_shell = nullptr;
std::unique_ptr<ShellHistory> global_shell_history;

static Bitu shellstop_handler()
{
	return CBRET_STOP;
}

std::unique_ptr<Program> SHELL_ProgramCreate() {
	return ProgramCreate<DOS_Shell>();
}

DOS_Shell::DOS_Shell()
{
	AddShellCmdsToHelpList();
	help_detail = {HELP_Filter::All,
	               HELP_Category::Misc,
	               HELP_CmdType::Program,
	               "COMMAND"};
}

void DOS_Shell::GetRedirection(char *line,
                               std::string &in_file,
                               std::string &out_file,
                               std::string &pipe_file,
                               bool *append)
{
	char *line_read = line;
	char *line_write = line;
	char character = 0;
	bool quote = false;
	size_t found = 0;
	size_t temp_len = 0;
	std::string redir = "";
	std::string find_chars = "";
	std::string *output;
	*append = false;
	while ((character = *line_read++)) {
		if (quote && character != '"') { /* don't parse redirection
			                            within quotes. Not perfect
			                            yet. Escaped quotes will
			                            mess the count up */
			*line_write++ = character;
			continue;
		}
		if (character == '"') {
			quote = !quote;
		} else if (character == '>' || character == '<' || character == '|') {
			// Overwrite with >, and append with >>
			if (character == '>' && (*append = (*line_read == '>')))
				line_read++;
			// Get the current content of the redirection
			redir = line_read = ltrim(line_read);
			// Try to find the characters for string split
			find_chars = character == '|'
			                     ? ""
			                     : (character != '<' ? " |<" : " |>");
			found = redir.find_first_of(find_chars);
			// Get the length of the substring before the
			// characters, or the entire string if not found
			if (found == std::string::npos) {
				temp_len = redir.size();
			} else {
				temp_len = found;
			}

			// Ignore trailing ':' character
			if (temp_len > 0 && redir[temp_len - 1] == ':') {
				--temp_len;
			}

			// Assign substring content of length to output parameters
			output = (character == '>'
			                  ? &out_file
			                  : (character == '<' ? &in_file : &pipe_file));
			*output = redir.substr(0, temp_len);
			line_read += temp_len;
			continue;
		}
		*line_write++ = character;
	}
	*line_write = 0;
}

bool get_pipe_status(const char *out_file,
                     const char *pipe_file,
                     char (&pipe_tempfile)[270],
                     const bool append,
                     bool &failed_pipe)
{
	FatAttributeFlags fattr = {};
	uint16_t dummy = 0;
	uint16_t dummy2 = 0;
	uint32_t bigdummy = 0;
	bool status = true;
	/* Create if not exist. Open if exist. Both in read/write mode */
	if (!pipe_file && append) {
		if (DOS_GetFileAttr(out_file, &fattr) && fattr.read_only) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			status = false;
		} else if ((status = DOS_OpenFile(out_file, OPEN_READWRITE, &dummy))) {
			DOS_SeekFile(1, &bigdummy, DOS_SEEK_END);
		} else {
			// Create if not exists.
			status = DOS_CreateFile(out_file,
			                        FatAttributeFlags::Archive,
			                        &dummy);
		}
	} else if (!pipe_file && DOS_GetFileAttr(out_file, &fattr) && fattr.read_only) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		status = false;
	} else {
		if (pipe_file &&
		    DOS_FindFirst(pipe_tempfile, FatAttributeFlags::NotVolume) &&
		    !DOS_UnlinkFile(pipe_tempfile)) {
			failed_pipe = true;
		}
		status = DOS_OpenFileExtended(pipe_file && !failed_pipe ? pipe_tempfile
		                                                        : out_file,
		                              OPEN_READWRITE,
		                              FatAttributeFlags::Archive,
		                              0x12,
		                              &dummy,
		                              &dummy2);
		if (pipe_file && (failed_pipe || !status) &&
		    (Drives[0] || Drives[2] || Drives[24]) &&
		    !strchr(pipe_tempfile, '\\')) {
			// Insert a drive prefix into the pipe filename path.
			// Note that the safe_strcpy truncates excess to prevent
			// writing beyond pipe_tempfile's fixed size.
			const std::string drive_prefix =
			        Drives[2] ? "c:\\" : (Drives[0] ? "a:\\" : "y:\\");
			const std::string pipe_full_path = drive_prefix + pipe_tempfile;
			safe_strcpy(pipe_tempfile, pipe_full_path.c_str());

			failed_pipe = false;
			if (DOS_FindFirst(pipe_tempfile, FatAttributeFlags::NotVolume) &&
			    !DOS_UnlinkFile(pipe_tempfile)) {
				failed_pipe = true;
			} else {
				status = DOS_OpenFileExtended(pipe_tempfile,
				                              OPEN_READWRITE,
				                              FatAttributeFlags::Archive,
				                              0x12,
				                              &dummy,
				                              &dummy2);
			}
		}
	}
	return status;
}

constexpr uint16_t failed_open = 0xff;
uint16_t open_stdin_as(const char *name)
{
	uint16_t success_open;
	if (DOS_OpenFile(name, OPEN_READ, &success_open))
		return success_open;
	else
		return failed_open;
}

uint16_t open_stdout_as(const char *name)
{
	uint16_t success_open;
	if (DOS_OpenFile(name, OPEN_READWRITE, &success_open))
		return success_open;
	else
		return failed_open;
}

void close_stdin(const bool condition = true)
{
	if (condition)
		DOS_CloseFile(0);
}

void close_stdout(const bool condition = true)
{
	if (condition)
		DOS_CloseFile(1);
}

void open_console_device(const bool condition = true)
{
	if (condition) {
		uint16_t dummy;
		DOS_OpenFile("con", OPEN_READWRITE, &dummy);
	}
}

uint16_t get_tick_random_number() {
	constexpr uint16_t random_uplimit = 10000;
	return (uint16_t)(GetTicks() % random_uplimit);
}

void DOS_Shell::ParseLine(char *line)
{
	LOG(LOG_EXEC, LOG_ERROR)("Parsing command line: %s", line);
	/* Check for a leading @ */
	if (line[0] == '@')
		line[0] = ' ';
	line = trim(line);

	/* Do redirection and pipe checks */
	std::string in_file = "";
	std::string out_file = "";
	std::string pipe_file = "";

	uint16_t dummy    = 0;
	bool append = false;
	bool normalstdin = false;  /* whether stdin/out are open on start. */
	bool normalstdout = false; /* Bug: Assumed is they are "con"      */

	GetRedirection(line, in_file, out_file, pipe_file, &append);
	if (in_file.length() || out_file.length() || pipe_file.length()) {
		normalstdin = (psp->GetFileHandle(0) != failed_open);
		normalstdout = (psp->GetFileHandle(1) != failed_open);
	}
	if (in_file.length()) {
		if ((dummy = open_stdin_as(in_file.c_str())) != failed_open) { // Test if
			// file exists
			DOS_CloseFile(dummy);
			LOG_MSG("SHELL: Redirect input from %s", in_file.c_str());
			close_stdin(normalstdin);
			open_stdin_as(in_file.c_str()); // Open new stdin
		} else {
			WriteOut(MSG_Get(dos.errorcode == DOSERR_ACCESS_DENIED
			                         ? "SHELL_ACCESS_DENIED"
			                         : "SHELL_FILE_OPEN_ERROR"),
			         in_file.c_str());
			return;
		}
	}
	bool failed_pipe = false;
	char pipe_tempfile[270]; // Piping requires the use of a temporary file
	FatAttributeFlags fattr = {};
	if (!pipe_file.empty()) {
		auto env_temp_path = psp->GetEnvironmentValue("TEMP");
		if (!env_temp_path) {
			env_temp_path = psp->GetEnvironmentValue("TMP");
		}
		if (!env_temp_path ||
		    (!DOS_GetFileAttr(env_temp_path->c_str(), &fattr) ||
		     !fattr.directory)) {
			safe_sprintf(pipe_tempfile,
			             "pipe%d.tmp",
			             get_tick_random_number());
		} else {
			safe_sprintf(pipe_tempfile,
			             "%s\\pipe%d.tmp",
			             env_temp_path->c_str(),
			             get_tick_random_number());
		}
	}
	if (out_file.length() || pipe_file.length()) {
		if (out_file.length() && pipe_file.length())
			WriteOut(MSG_Get("SHELL_CMD_DUPLICATE_REDIRECTION"),
			         out_file.c_str());
		// LOG_MSG("SHELL: Redirecting output to %s",
		//         pipe_file.length() ? pipe_tempfile : out_file.c_str());
		close_stdout(normalstdout);
		open_console_device(!normalstdin && !in_file.length());
		if (!get_pipe_status(out_file.length() ? out_file.c_str() : nullptr,
		                     pipe_file.length() ? pipe_file.c_str() : nullptr,
		                     pipe_tempfile, append, failed_pipe) &&
		    normalstdout) {
			// Read only file, open con again
			open_console_device();
			if (!pipe_file.length()) {
				WriteOut(MSG_Get(dos.errorcode == DOSERR_ACCESS_DENIED
				                         ? "SHELL_ACCESS_DENIED"
				                         : "SHELL_FILE_CREATE_ERROR"),
				         out_file.length() ? out_file.c_str()
				                           : "(unnamed)");
				close_stdout();
				open_stdout_as("nul");
			}
		}
		close_stdin(!normalstdin && !in_file.length());
	}
	/* Run the actual command */
	DoCommand(line);
	/* Restore handles */
	if (in_file.length()) {
		close_stdin();
		open_console_device(normalstdin);
	}
	if (out_file.length() || pipe_file.length()) {
		close_stdout();
		open_console_device(!normalstdin);
		open_console_device(normalstdout);
		close_stdin(!normalstdin);
	}
	if (pipe_file.length()) {
		// Test if file can be opened for reading
		if (!failed_pipe &&
		    (dummy = open_stdin_as(pipe_tempfile)) != failed_open) {
			DOS_CloseFile(dummy);
			close_stdin(normalstdin);
			open_stdin_as(pipe_tempfile); // Open new stdin
			ParseLine((char *)pipe_file.c_str());
			close_stdin();
			open_console_device(normalstdin);
		} else {
			WriteOut(MSG_Get("SHELL_CMD_FAILED_PIPE"));
			LOG_MSG("SHELL: Failed to write pipe content to temporary file");
		}
		if (DOS_FindFirst(pipe_tempfile, FatAttributeFlags::NotVolume)) {
			if (!DOS_UnlinkFile(pipe_tempfile)) {
				LOG_WARNING("SHELL: Failed to delete the pipe's temporary file, '%s'",
				            pipe_tempfile);
			}
		}
	}
}

void DOS_Shell::RunBatchFile()
{
	char input_line[CMD_MAXLINE] = {0};
	while (!batchfiles.empty() && !shutdown_requested && !exit_cmd_called) {
		if (batchfiles.top().ReadLine(input_line)) {
			if (batchfiles.top().Echo()) {
				if (input_line[0] != '@') {
					ShowPrompt();
					WriteOut_NoParsing(input_line);
					WriteOut_NoParsing("\n");
				}
			}
			ParseLine(input_line);
		} else {
			batchfiles.pop();
		}
	}
}

void DOS_Shell::Run()
{
	// COMMAND.COM's /C and /INIT spawn sub-commands. When parsing help, we need
	// to be sure the /? and -? are intended for us and not part of the
	// sub-command.
	if (cmd->ExistsPriorTo({"/?", "-?"}, {"/C", "/INIT"})) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_COMMAND_HELP_LONG"));
		output.Display();
		return;
	}
	char input_line[CMD_MAXLINE] = {0};
	std::string line = {};
	if (cmd->FindStringRemainBegin("/C",line)) {
		safe_strcpy(input_line, line.c_str());
		char* sep = strpbrk(input_line, "\r\n"); //GTA installer
		if (sep) *sep = 0;
		DOS_Shell temp;
		temp.echo = echo;
		temp.ParseLine(input_line);
		temp.RunBatchFile();
		return;
	}
	/* Start a normal shell and check for a first command init */
	if (cmd->FindString("/INIT",line,true)) {
		const bool wants_welcome_banner = control->GetStartupVerbosity() >=
		                                  Verbosity::High;
		if (wants_welcome_banner) {
			WriteOut(MSG_Get("SHELL_STARTUP_BEGIN"),
			         DOSBOX_GetDetailedVersion(), PRIMARY_MOD_NAME,
			         PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_PAD,
			         PRIMARY_MOD_NAME, PRIMARY_MOD_PAD);
#if C_DEBUG
			WriteOut(MSG_Get("SHELL_STARTUP_DEBUG"), MMOD2_NAME);
#endif
			if (machine == MCH_CGA) {
				if (mono_cga)
					WriteOut(MSG_Get("SHELL_STARTUP_CGA_MONO"),
					         MMOD2_NAME);
				else
					WriteOut(MSG_Get("SHELL_STARTUP_CGA"),
					         MMOD2_NAME, MMOD1_NAME,
					         MMOD2_NAME, PRIMARY_MOD_PAD);
			}
			if (machine == MCH_HERC)
				WriteOut(MSG_Get("SHELL_STARTUP_HERC"));
			WriteOut(MSG_Get("SHELL_STARTUP_END"));
		}
		safe_strcpy(input_line, line.c_str());
		line.erase();
		ParseLine(input_line);
	} else {
		WriteOut(MSG_Get("SHELL_STARTUP_SUB"), DOSBOX_GetDetailedVersion());
	}
	while (!exit_cmd_called && !shutdown_requested) {
		if (!batchfiles.empty()){
			RunBatchFile();
		} else {
			if (echo) ShowPrompt();
			InputCommand(input_line);
			ParseLine(input_line);
		}
	}
}

void DOS_Shell::SyntaxError()
{
	WriteOut(MSG_Get("SHELL_SYNTAX_ERROR"));
}

extern int64_t ticks_at_program_launch;

// Specify a 'Drive' config object with allowed key and value types
static std::unique_ptr<Config> specify_drive_config()
{
	auto conf = std::make_unique<Config>();

	// Define the [drive] section
	constexpr auto changeable_at_runtime = false;
	auto prop = conf->AddSection_prop("drive", nullptr, changeable_at_runtime);

	// Define the allowed keys and types
	constexpr auto on_startup = Property::Changeable::OnlyAtStart;
	prop->Add_string("type", on_startup, "")
	        ->Set_values({"dir", "floppy", "cdrom", "overlay"});
	prop->Add_string("label", on_startup, "");
	prop->Add_string("path", on_startup, "");
	prop->Add_string("override_drive", on_startup, "");
	prop->Add_bool("verbose", on_startup, true);

	return conf;
}

// Parse a 'Drive' config file and return object with allowed key and value types
std::tuple<std::string, std::string, std::string, bool> parse_drive_conf(
        std::string drive_letter, const std_fs::path& conf_path)
{
	// Default return values
	constexpr auto default_args = "";
	constexpr auto default_path = "";
	constexpr auto default_verbosity = false;

	// If the conf path doesn't exist, at least return the default quiet arg
	if (!path_exists(conf_path))
		return {drive_letter, default_args, default_path, default_verbosity};

	// If we couldn't parse it, return the defaults
	auto conf = specify_drive_config();
	assert(conf);
	if (!conf->ParseConfigFile("auto-mounted drive", conf_path.string()))
		return {drive_letter, default_args, default_path, default_verbosity};

	const auto settings = static_cast<Section_prop *>(conf->GetSection("drive"));

	// Construct the mount arguments
	const std::string override_drive = settings->Get_string("override_drive");
	if (override_drive.length() == 1 && override_drive[0] >= 'a' && override_drive[0] <= 'y')
		drive_letter = override_drive;
	else if (!override_drive.empty()) {
		LOG_ERR("AUTOMOUNT: %s: setting 'override_drive = %s' is invalid", conf_path.string().c_str(), override_drive.c_str());
		LOG_ERR("AUTOMOUNT: The override_drive setting can be left empty or a drive letter from 'a' to 'y'");
	}

	std::string drive_type = settings->Get_string("type");
	if (!drive_type.empty()) {
		drive_type.insert(0, " -t ");
	}

	std::string drive_label = settings->Get_string("label");
	if (!drive_label.empty()) {
		drive_label.insert(0, " -label ");
	}

	const auto mount_args = drive_type + drive_label;

	const std::string path_val = settings->Get_string("path");

	const auto is_verbose = settings->Get_bool("verbose");

	return {drive_letter, mount_args, path_val, is_verbose};
}

static Bitu INT2E_Handler()
{
	/* Save return address and current process */
	RealPt save_ret=real_readd(SegValue(ss),reg_sp);
	uint16_t save_psp=dos.psp();

	/* Set first shell as process and copy command */
	dos.psp(DOS_FIRST_SHELL);
	DOS_PSP psp(DOS_FIRST_SHELL);
	psp.SetCommandTail(RealMakeSeg(ds,reg_si));
	SegSet16(ss,RealSegment(psp.GetStack()));
	reg_sp=2046;

	/* Read and fix up command string */
	CommandTail tail;
	MEM_BlockRead(PhysicalMake(dos.psp(),128),&tail,128);
	if (tail.count<127) tail.buffer[tail.count]=0;
	else tail.buffer[126]=0;
	char* crlf=strpbrk(tail.buffer, "\r\n");
	if (crlf) *crlf=0;

	/* Execute command */
	if (safe_strlen(tail.buffer)) {
		DOS_Shell temp;
		temp.ParseLine(tail.buffer);
		temp.RunBatchFile();
	}

	/* Restore process and "return" to caller */
	dos.psp(save_psp);
	SegSet16(cs,RealSegment(save_ret));
	reg_ip=RealOffset(save_ret);
	reg_ax=0;
	return CBRET_NONE;
}

static const char* const path_string    = "PATH=Z:\\";
static const char* const comspec_string = "COMSPEC=Z:\\COMMAND.COM";
static const char* const full_name      = "Z:\\COMMAND.COM";
static const char* const init_line      = "/INIT AUTOEXEC.BAT";

void SHELL_Init() {
	// Generic messages, to be used by any command or DOS program
	MSG_Add("SHELL_ILLEGAL_PATH", "Illegal path.\n");
	MSG_Add("SHELL_ILLEGAL_FILE_NAME", "Illegal filename.\n");
	MSG_Add("SHELL_ILLEGAL_SWITCH", "Illegal switch: %s\n");
	MSG_Add("SHELL_ILLEGAL_SWITCH_COMBO", "Illegal switch combination.\n");
	MSG_Add("SHELL_MISSING_PARAMETER", "Required parameter missing.\n");
	MSG_Add("SHELL_TOO_MANY_PARAMETERS", "Too many parameters.\n");
	MSG_Add("SHELL_EXPECTED_FILE_NOT_DIR", "Expected a file, not a directory.\n");
	MSG_Add("SHELL_SYNTAX_ERROR", "Incorrect command syntax.\n");
	MSG_Add("SHELL_ACCESS_DENIED", "Access denied - '%s'\n");
	MSG_Add("SHELL_FILE_CREATE_ERROR", "File creation error - '%s'\n");
	MSG_Add("SHELL_FILE_OPEN_ERROR", "File open error - '%s'\n");
	MSG_Add("SHELL_FILE_NOT_FOUND", "File not found - '%s'\n");
	MSG_Add("SHELL_FILE_EXISTS", "File '%s' already exists.\n");
	MSG_Add("SHELL_DIRECTORY_NOT_FOUND", "Directory not found - '%s'\n");
	MSG_Add("SHELL_NO_SUBDIRS_TO_DISPLAY", "No subdirectories to display.\n");
	MSG_Add("SHELL_NO_FILES_SUBDIRS_TO_DISPLAY", "No files or subdirectories to display.\n");
	MSG_Add("SHELL_READ_ERROR", "Error reading file - '%s'\n");
	MSG_Add("SHELL_WRITE_ERROR", "Error writing file - '%s'\n");

	// Command specific messages
	MSG_Add("SHELL_CMD_HELP", "If you want a list of all supported commands, run [color=yellow]help /all[reset]\n"
			"A short list of the most often used commands:\n");
	MSG_Add("SHELL_CMD_COMMAND_HELP_LONG",
	        "Start the DOSBox Staging command shell.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]command[reset]\n"
	        "  [color=light-green]command[reset] /c (or /init) [color=light-cyan]COMMAND[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]COMMAND[reset]  DOS command, game, or program to run\n"
	        "\n"
	        "Notes:\n"
	        "  - DOSBox Staging automatically starts a DOS command shell by invoking this\n"
	        "    command with /init option when it starts, which shows the welcome banner.\n"
	        "  - You can load a new instance of the command shell by running [color=light-green]command[reset].\n"
	        "  - Adding a /c option along with [color=light-cyan]COMMAND[reset] allows this command to run the\n"
	        "    specified command (optionally with parameters) and then exit automatically.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]command[reset]\n"
	        "  [color=light-green]command[reset] /c [color=light-cyan]echo[reset] [color=white]Hello world![reset]\n"
	        "  [color=light-green]command[reset] /init [color=light-cyan]dir[reset]\n");

	MSG_Add("SHELL_CMD_ECHO_ON", "Echo is on.\n");
	MSG_Add("SHELL_CMD_ECHO_OFF", "Echo is off.\n");

	MSG_Add("SHELL_CMD_CHDIR_ERROR", "Unable to change to: %s\n");
	MSG_Add("SHELL_CMD_CHDIR_HINT", "Hint: To change to a different drive, run [color=yellow]%c:[reset]\n");

	MSG_Add("SHELL_CMD_CHDIR_HINT_2",
	        "Directory name is longer than 8 characters and/or contains spaces.\n"
	        "Try [color=yellow]cd %s[reset]\n");
	MSG_Add("SHELL_CMD_CHDIR_HINT_3", "You are still on drive Z:; change to a mounted drive with [color=yellow]C:[reset].\n");

	MSG_Add("SHELL_CMD_DATE_HELP", "Display or change the internal date.\n");
	MSG_Add("SHELL_CMD_DATE_ERROR", "The specified date is not correct.\n");
	MSG_Add("SHELL_CMD_DATE_DAYS", "3SunMonTueWedThuFriSat"); // "2SoMoDiMiDoFrSa"
	MSG_Add("SHELL_CMD_DATE_NOW", "Current date: ");
	MSG_Add("SHELL_CMD_DATE_SETHLP", "Run [color=yellow]date %s[reset] to change the current date.\n");

	MSG_Add("SHELL_CMD_DATE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]date[reset] [/t]\n"
	        "  [color=light-green]date[reset] /h\n"
	        "  [color=light-green]date[reset] [color=light-cyan]DATE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DATE[reset]  new date to set to, in the format of [color=light-cyan]%s[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]date[reset] without an argument shows the current date, or a simple date\n"
	        "  with the /t option. You can force a date synchronization with the host system\n"
	        "  with the /h option, or manually specify a new date to set to.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]date[reset]\n"
	        "  [color=light-green]date[reset] /h\n"
	        "  [color=light-green]date[reset] [color=light-cyan]%s[reset]\n");

	MSG_Add("SHELL_CMD_TIME_HELP", "Display or change the internal time.\n");
	MSG_Add("SHELL_CMD_TIME_ERROR", "The specified time is not correct.\n");
	MSG_Add("SHELL_CMD_TIME_NOW", "Current time: ");
	MSG_Add("SHELL_CMD_TIME_SETHLP", "Run [color=yellow]time %s[reset] to change the current time.\n");
	MSG_Add("SHELL_CMD_TIME_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]time[reset] [/t]\n"
	        "  [color=light-green]time[reset] /h\n"
	        "  [color=light-green]time[reset] [color=light-cyan]TIME[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]TIME[reset]  new time to set to, in the format of [color=light-cyan]%s[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]time[reset] without an argument shows the current time, or a simple time\n"
	        "  with the /t option. You can force a time synchronization with the host system\n"
	        "  with the /h option, or manually specify a new time to set to.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]time[reset]\n"
	        "  [color=light-green]time[reset] /h\n"
	        "  [color=light-green]time[reset] [color=light-cyan]%s[reset]\n");

	MSG_Add("SHELL_CMD_MKDIR_ERROR", "Unable to make: %s.\n");
	MSG_Add("SHELL_CMD_RMDIR_ERROR", "Unable to remove: %s.\n");

	MSG_Add("SHELL_CMD_DEL_ERROR", "Unable to delete: %s.\n");

	MSG_Add("SHELL_CMD_SET_NOT_SET", "Environment variable '%s' not defined.\n");
	MSG_Add("SHELL_CMD_SET_OUT_OF_SPACE", "Not enough environment space left.\n");

	MSG_Add("SHELL_CMD_IF_EXIST_MISSING_FILENAME", "IF EXIST: Missing filename.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_MISSING_NUMBER", "IF ERRORLEVEL: Missing number.\n");
	MSG_Add("SHELL_CMD_IF_ERRORLEVEL_INVALID_NUMBER", "IF ERRORLEVEL: Invalid number.\n");

	MSG_Add("SHELL_CMD_GOTO_MISSING_LABEL", "No label supplied to GOTO command.\n");
	MSG_Add("SHELL_CMD_GOTO_LABEL_NOT_FOUND", "GOTO: Label '%s' not found.\n");

	MSG_Add("SHELL_CMD_DUPLICATE_REDIRECTION", "Duplicate redirection: %s\n");

	MSG_Add("SHELL_CMD_FAILED_PIPE", "\nFailed to create/open a temporary file for piping. Check the %%TEMP%% variable.\n");

	MSG_Add("SHELL_CMD_DIR_VOLUME", " Volume in drive %c is %s\n");
	MSG_Add("SHELL_CMD_DIR_INTRO", " Directory of %s\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_USED", "%17d file(s) %21s bytes\n");
	MSG_Add("SHELL_CMD_DIR_BYTES_FREE", "%17d dir(s)  %21s bytes free\n");

	MSG_Add("SHELL_EXECUTE_DRIVE_NOT_FOUND", "Drive %c does not exist!\nYou must [color=yellow]mount[reset] it first. "
			"Run [color=yellow]intro[reset] or [color=yellow]intro mount[reset] for more information.\n");

	MSG_Add("SHELL_EXECUTE_ILLEGAL_COMMAND", "Illegal command: %s\n");
	MSG_Add("SHELL_CMD_PAUSE", "Press any key to continue...");
	MSG_Add("SHELL_CMD_PAUSE_HELP", "Wait for a keystroke to continue.\n");

	MSG_Add("SHELL_CMD_PAUSE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]pause[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  This command has no parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  This command is especially useful in batch programs to allow a user to\n"
	        "  continue the batch program execution with a key press. The user can press\n"
	        "  any key on the keyboard (except for certain control keys) to continue.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]pause[reset]\n");

	MSG_Add("SHELL_CMD_COPY_FAILURE", "Copy failure: %s.\n");
	MSG_Add("SHELL_CMD_COPY_SUCCESS", "   %d File(s) copied.\n");
	MSG_Add("SHELL_CMD_SUBST_NO_REMOVE", "Unable to remove, drive not in use.\n");
	MSG_Add("SHELL_CMD_SUBST_FAILURE", "SUBST failed, the target drive may already exist.\nNote it is only possible to use SUBST on local drives.");

	MSG_Add("SHELL_STARTUP_BEGIN",
	        "[bgcolor=blue][color=white]╔════════════════════════════════════════════════════════════════════╗\n"
	        "║ [color=light-green]Welcome to DOSBox Staging %-40s[color=white] ║\n"
	        "║                                                                    ║\n"
	        "║ For a short introduction for new users type: [color=yellow]INTRO[color=white]                 ║\n"
	        "║ For supported shell commands type: [color=yellow]HELP[color=white]                            ║\n"
	        "║                                                                    ║\n"
	        "║ To adjust the emulated CPU speed, use [color=light-red]%s+F11[color=white] and [color=light-red]%s+F12[color=white].%s%s       ║\n"
	        "║ To activate the keymapper [color=light-red]%s+F1[color=white].%s                                 ║\n"
	        "║ For more information read the [color=light-cyan]README[color=white] file in the DOSBox directory. ║\n"
	        "║                                                                    ║\n");
	MSG_Add("SHELL_STARTUP_CGA",
	        "║ DOSBox supports Composite CGA mode.                                ║\n"
	        "║ Use [color=light-red]F12[color=white] to set composite output ON, OFF, or AUTO (default).        ║\n"
	        "║ [color=light-red]F10[color=white] selects the CGA settings to change and [color=light-red](%s+)F11[color=white] changes it.   ║\n"
	        "║                                                                    ║\n");
	MSG_Add("SHELL_STARTUP_CGA_MONO",
	        "║ Use [color=light-red]F11[color=white] to cycle through green, amber, white and paper-white mode, ║\n"
	        "║ and [color=light-red]%s+F11[color=white] to change contrast/brightness settings.                ║\n"
	        "║                                                                    ║\n");
	MSG_Add("SHELL_STARTUP_HERC",
	        "║ Use [color=light-red]F11[color=white] to cycle through white, amber, and green monochrome color. ║\n"
	        "║                                                                    ║\n");
	MSG_Add("SHELL_STARTUP_DEBUG",
	        "║ Press [color=light-red]%s+Pause[color=white] to enter the debugger or start the exe with [color=yellow]DEBUG[color=white]. ║\n"
	        "║                                                                    ║\n");
	MSG_Add("SHELL_STARTUP_END",
	        "║ [color=yellow]https://dosbox-staging.github.io[color=white]                                   ║\n"
	        "╚════════════════════════════════════════════════════════════════════╝[reset]\n"
	        "\n");

	MSG_Add("SHELL_STARTUP_SUB",
	        "[color=light-green]" DOSBOX_PROJECT_NAME " %s[reset]\n");

	MSG_Add("SHELL_CMD_CHDIR_HELP", "Display or change the current directory.\n");
	MSG_Add("SHELL_CMD_CHDIR_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]cd[reset] [color=light-cyan]DIRECTORY[reset]\n"
	        "  [color=light-green]chdir[reset] [color=light-cyan]DIRECTORY[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DIRECTORY[reset]  name of the directory to change to\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]cd[reset] without an argument displays the current directory.\n"
	        "  With [color=light-cyan]DIRECTORY[reset] the command only changes the directory, not the current drive.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]cd[reset]\n"
	        "  [color=light-green]cd[reset] [color=light-cyan]mydir[reset]\n");

	MSG_Add("SHELL_CMD_CLS_HELP", "Clear the DOS screen.\n");
	MSG_Add("SHELL_CMD_CLS_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]cls[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  This command has no parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]cls[reset] clears all text on the DOS screen, except for the command\n"
	        "  prompt (e.g., [color=white]Z:\\>[reset] or [color=white]C:\\GAMES>[reset]) on the top-left corner of the screen.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]cls[reset]\n");

	MSG_Add("SHELL_CMD_DIR_HELP",
	        "Display a list of files and subdirectories in a directory.\n");
	MSG_Add("SHELL_CMD_DIR_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]dir[reset] [color=light-cyan][PATTERN][reset] [/w] \\[/b] [/p] [ad] [a-d] [/o[color=white]ORDER[reset]]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]PATTERN[reset]  either an exact filename or an inexact filename with wildcards, which\n"
	        "           are the asterisk (*) and the question mark (?); a path can be\n"
	        "           specified in the pattern to list contents in the specified directory\n"
	        "  [color=white]ORDER[reset]    listing order, including [color=white]n[reset] (by name, alphabetic), [color=white]s[reset] (by size,\n"
	        "           smallest first), [color=white]e[reset] (by extension, alphabetic), and [color=white]d[reset] (by date/time,\n"
	        "           oldest first), with an optional [color=white]-[reset] prefix to reverse order\n"
	        "  /w       list 5 files/directories in a row;  /b       list the names only\n"
	        "  /o[color=white]ORDER[reset]  order the list (see above);         /p       pause after each screen\n"
	        "  /ad      list all directories;               /a-d     list all files\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]dir[reset] without an argument lists all files and subdirectories in the\n"
	        "  current directory, which is the same as [color=light-green]dir[reset] [color=light-cyan]*.*[reset].\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]dir[reset] [color=light-cyan][reset]\n"
	        "  [color=light-green]dir[reset] [color=light-cyan]games.*[reset] /p\n"
	        "  [color=light-green]dir[reset] [color=light-cyan]c:\\games\\*.exe[reset] /b /o[color=white]-d[reset]\n");

	MSG_Add("SHELL_CMD_ECHO_HELP",
	        "Display messages and enable/disable command echoing.\n");
	MSG_Add("SHELL_CMD_ECHO_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]echo[reset] [color=light-cyan][on|off][reset]\n"
	        "  [color=light-green]echo[reset] [color=light-cyan][MESSAGE][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]on|off[reset]   turn on/off command echoing\n"
	        "  [color=light-cyan]MESSAGE[reset]  message to display\n"
	        "\n"
	        "Notes:\n"
	        "  - Running [color=light-green]echo[reset] without an argument shows the current on or off status.\n"
	        "  - Echo is especially useful when writing or debugging batch files.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]echo[reset] [color=light-cyan]off[reset]\n"
	        "  [color=light-green]echo[reset] [color=light-cyan]Hello world![reset]\n");

	MSG_Add("SHELL_CMD_EXIT_HELP", "Exit from the DOS shell.\n");
	MSG_Add("SHELL_CMD_EXIT_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]exit[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  This command has no parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  If you start a DOS shell from a program, running [color=light-green]exit[reset] returns to the program.\n"
	        "  If there is no DOS program running, the command quits from DOSBox Staging.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]exit[reset]\n");
	MSG_Add("SHELL_CMD_EXIT_TOO_SOON", "Preventing an early 'exit' call from terminating.\n");

	MSG_Add("SHELL_CMD_HELP_HELP",
	        "Display help information for DOS commands.\n");
	MSG_Add("SHELL_CMD_HELP_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]help[reset]\n"
	        "  [color=light-green]help[reset] /a[ll]\n"
	        "  [color=light-green]help[reset] [color=light-cyan]COMMAND[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]COMMAND[reset]  name of an internal DOS command, such as [color=light-cyan]dir[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  - Running [color=light-green]help[reset] without an argument displays a DOS command list.\n"
	        "  - You can view a full list of internal commands with the /a or /all option.\n"
	        "  - Instead of [color=light-green]help[reset] [color=light-cyan]COMMAND[reset], you can also get command help with [color=light-cyan]COMMAND[reset] /?.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]help[reset] [color=light-cyan]dir[reset]\n"
	        "  [color=light-green]help[reset] /all\n");

	MSG_Add("SHELL_CMD_MKDIR_HELP", "Create a directory.\n");
	MSG_Add("SHELL_CMD_MKDIR_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]md[reset] [color=light-cyan]DIRECTORY[reset]\n"
	        "  [color=light-green]mkdir[reset] [color=light-cyan]DIRECTORY[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DIRECTORY[reset]  exact name of the directory to create\n"
	        "\n"
	        "Notes:\n"
	        "  - The directory must not exist yet.\n"
	        "  - You can specify a path where the directory will be created.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]md[reset] [color=light-cyan]newdir[reset]\n"
	        "  [color=light-green]md[reset] [color=light-cyan]c:\\games\\dir[reset]\n");

	MSG_Add("SHELL_CMD_RMDIR_HELP", "Remove a directory.\n");
	MSG_Add("SHELL_CMD_RMDIR_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]rd[reset] [color=light-cyan]DIRECTORY[reset]\n"
	        "  [color=light-green]rmdir[reset] [color=light-cyan]DIRECTORY[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DIRECTORY[reset]  name of the directory to remove\n"
	        "\n"
	        "Notes:\n"
	        "  The directory must be empty, with no files or subdirectories.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]rd[reset] [color=light-cyan]emptydir[reset]\n");

	MSG_Add("SHELL_CMD_SET_HELP", "Display or change environment variables.\n");
	MSG_Add("SHELL_CMD_SET_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]set[reset]\n"
	        "  [color=light-green]set[reset] [color=white]VARIABLE[reset]=[color=light-cyan][STRING][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]VARIABLE[reset]  name of the environment variable\n"
	        "  [color=light-cyan]STRING[reset]    series of characters to assign to the variable\n"
	        "\n"
	        "Notes:\n"
	        "  - Assigning an empty string to the variable removes the variable.\n"
	        "  - The command without a parameter displays current environment variables.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]set[reset]\n"
	        "  [color=light-green]set[reset] [color=white]name[reset]=[color=light-cyan]value[reset]\n");

	MSG_Add("SHELL_CMD_IF_HELP",
	        "Perform conditional processing in batch programs.\n");
	MSG_Add("SHELL_CMD_IF_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]if[reset] [color=light-magenta][not][reset] [color=light-cyan]errorlevel[reset] [color=white]NUMBER[reset] COMMAND\n"
	        "  [color=light-green]if[reset] [color=light-magenta][not][reset] [color=white]STR1==STR2[reset] COMMAND\n"
	        "  [color=light-green]if[reset] [color=light-magenta][not][reset] [color=light-cyan]exist[reset] [color=white]FILE[reset] COMMAND\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]NUMBER[reset]      positive integer less or equal to the desired value\n"
	        "  [color=white]STR1==STR2[reset]  compare two text strings (case-sensitive)\n"
	        "  [color=white]FILE[reset]        exact filename to check for existence\n"
	        "  COMMAND     DOS command or program to run, optionally with parameters\n"
	        "\n"
	        "Notes:\n"
	        "  - The COMMAND is run if any of the three conditions in the usage are met.\n"
	        "  - If [color=light-magenta]not[reset] is specified, then the command runs only with the false condition.\n"
	        "  - The [color=light-cyan]errorlevel[reset] condition is useful for checking if a programs ran correctly.\n"
	        "  - If either [color=white]STR1[reset] or [color=white]STR2[reset] may be empty, you can enclose them in quotes (\").\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]if[reset] [color=light-cyan]errorlevel[reset] [color=white]2[reset] dir\n"
	        "  [color=light-green]if[reset] [color=white]\"%%myvar%%\"==\"mystring\"[reset] echo Hello world!\n"
	        "  [color=light-green]if[reset] [color=light-magenta]not[reset] [color=light-cyan]exist[reset] [color=white]file.txt[reset] exit\n");

	MSG_Add("SHELL_CMD_GOTO_HELP",
	        "Jump to a labeled line in a batch program.\n");
	MSG_Add("SHELL_CMD_GOTO_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]goto[reset] [color=light-cyan]LABEL[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]LABEL[reset]  text string used in the batch program as a label\n"
	        "\n"
	        "Notes:\n"
	        "  - A label is on a line by itself, beginning with a colon (:).\n"
	        "  - The label must be unique, and can be anywhere within the batch program.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]goto[reset] [color=light-cyan]mylabel[reset]\n");

	MSG_Add("SHELL_CMD_SHIFT_HELP", "Left-shift command-line parameters in a batch program.\n");
	MSG_Add("SHELL_CMD_SHIFT_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]shift[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  This command has no parameters.\n"
	        "\n"
	        "Notes:\n"
	        "  This command allows a DOS batch program to accept more than 9 parameters.\n"
	        "  Running [color=light-green]shift[reset] left-shifts the batch program variable %%1 to %%0, %%2 to %%1, etc.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]shift[reset]\n");

	MSG_Add("SHELL_CMD_TYPE_HELP", "Display the contents of a text file.\n");
	MSG_Add("SHELL_CMD_TYPE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]type[reset] [color=light-cyan]FILE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]FILE[reset]  name of the file to display\n"
	        "\n"
	        "Notes:\n"
	        "  - The filename must be exact, optionally with a path.\n"
	        "  - This command is only for viewing text files, not binary files.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]type[reset] [color=light-cyan]text.txt[reset]\n"
	        "  [color=light-green]type[reset] [color=light-cyan]c:\\dos\\readme.txt[reset]\n");

	MSG_Add("SHELL_CMD_REM_HELP", "Add comments in a batch program.\n");
	MSG_Add("SHELL_CMD_REM_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]rem[reset] [color=light-cyan]COMMENT[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]COMMENT[reset]  any comment you want to add\n"
	        "\n"
	        "Notes:\n"
	        "  - Adding comments to a batch program can make it easier to understand.\n"
	        "  - You can also temporarily comment out some commands with this command.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]rem[reset] [color=light-cyan]This is my test batch program.[reset]\n");

	MSG_Add("SHELL_CMD_NO_WILD", "This is a simple version of the command, no wildcards allowed!\n");

	MSG_Add("SHELL_CMD_RENAME_HELP", "Rename one or more files.\n");
	MSG_Add("SHELL_CMD_RENAME_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]ren[reset] [color=white]SOURCE[reset] [color=light-cyan]DESTINATION[reset]\n"
	        "  [color=light-green]rename[reset] [color=white]SOURCE[reset] [color=light-cyan]DESTINATION[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]SOURCE[reset]       name of the file to rename\n"
	        "  [color=light-cyan]DESTINATION[reset]  new name for the renamed file\n"
	        "\n"
	        "Notes:\n"
	        "  - The source filename must be exact, optionally with a path.\n"
	        "  - The destination filename must be exact without a path.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]ren[reset] [color=white]oldname[reset] [color=light-cyan]newname[reset]\n"
	        "  [color=light-green]ren[reset] [color=white]c:\\dos\\file.txt[reset] [color=light-cyan]f.txt[reset]\n");

	MSG_Add("SHELL_CMD_DELETE_HELP", "Remove one or more files.\n");
	MSG_Add("SHELL_CMD_DELETE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]del[reset] [color=light-cyan]PATTERN[reset]\n"
	        "  [color=light-green]erase[reset] [color=light-cyan]PATTERN[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]PATTERN[reset]  either an exact filename (such as [color=light-cyan]file.txt[reset]) or an inexact filename\n"
	        "           using one or more wildcards, which are the asterisk (*) representing\n"
	        "           any sequence of one or more characters, and the question mark (?)\n"
	        "           representing any single character, such as [color=light-cyan]*.bat[reset] and [color=light-cyan]c?.txt[reset].\n"
	        "\n"
	        "Warning:\n"
	        "  Be careful when using a pattern with wildcards, especially [color=light-cyan]*.*[reset], as all files\n"
	        "  matching the pattern will be deleted.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]del[reset] [color=light-cyan]test.bat[reset]\n"
	        "  [color=light-green]del[reset] [color=light-cyan]c*.*[reset]\n"
	        "  [color=light-green]del[reset] [color=light-cyan]a?b.c*[reset]\n");

	MSG_Add("SHELL_CMD_COPY_HELP", "Copy one or more files.\n");
	MSG_Add("SHELL_CMD_COPY_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]copy[reset] [color=white]SOURCE[reset] [color=light-cyan][DESTINATION][reset]\n"
	        "  [color=light-green]copy[reset] [color=white]SOURCE1+SOURCE2[+...][reset] [color=light-cyan][DESTINATION][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]SOURCE[reset]       either an exact filename or an inexact filename with wildcards,\n"
	        "               which are the asterisk (*) and the question mark (?)\n"
	        "  [color=light-cyan]DESTINATION[reset]  exact filename or directory, not containing any wildcards\n"
	        "\n"
	        "Notes:\n"
	        "  - The [color=white]+[reset] operator combines multiple source files provided to a single file.\n"
	        "  - [color=light-cyan]DESTINATION[reset] is optional: if omitted, files are copied to the current path.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]copy[reset] [color=white]source.bat[reset] [color=light-cyan]new.bat[reset]\n"
	        "  [color=light-green]copy[reset] [color=white]file1.txt+file2.txt[reset] [color=light-cyan]file3.txt[reset]\n"
	        "  [color=light-green]copy[reset] [color=white]..\\c*.*[reset]\n");

	MSG_Add("SHELL_CMD_CALL_HELP",
	        "Start a batch program from within another batch program.\n");
	MSG_Add("SHELL_CMD_CALL_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]call[reset] [color=white]BATCH[reset] [color=light-cyan][PARAMETERS][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]BATCH[reset]       batch program to launch\n"
	        "  [color=light-cyan]PARAMETERS[reset]  optional parameters for the batch program\n"
	        "\n"
	        "Notes:\n"
	        "  After calling another batch program, the original batch program will resume\n"
	        "  running after the other batch program ends.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]call[reset] [color=white]mybatch.bat[reset]\n"
	        "  [color=light-green]call[reset] [color=white]file.bat[reset] [color=light-cyan]Hello world![reset]\n");
	MSG_Add("SHELL_CMD_SUBST_HELP", "Assign an internal directory to a drive.\n");
	MSG_Add("SHELL_CMD_SUBST_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]subst[reset] [color=white]DRIVE[reset] [color=light-cyan]PATH[reset]\n"
	        "  [color=light-green]subst[reset] [color=white]DRIVE[reset] /d\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]DRIVE[reset]  drive to which you want to assign a path\n"
	        "  [color=light-cyan]PATH[reset]   mounted DOS path you want to assign to\n"
	        "\n"
	        "Notes:\n"
	        "  - The path must be on a drive mounted by the [color=light-green]mount[reset] command.\n"
	        "  - You can remove an assigned drive with the /d option.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]subst[reset] [color=white]d:[reset] [color=light-cyan]c:\\games[reset]\n"
	        "  [color=light-green]subst[reset] [color=white]e:[reset] [color=light-cyan]/d[reset]\n");

	MSG_Add("SHELL_CMD_LOADHIGH_HELP", "Load a DOS program into upper memory.\n");
	MSG_Add("SHELL_CMD_LOADHIGH_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]lh[reset] [color=light-cyan]PROGRAM[reset] [color=white][PARAMETERS][reset]\n"
	        "  [color=light-green]loadhigh[reset] [color=light-cyan]PROGRAM[reset] [color=white][PARAMETERS][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]PROGRAM[reset]  DOS TSR program to load, optionally with parameters\n"
	        "\n"
	        "Notes:\n"
	        "  This command intends to save the conventional memory by loading specified DOS\n"
	        "  TSR programs into upper memory if possible. Such programs may be required for\n"
	        "  some DOS games; XMS and UMB memory must be enabled (xms=true and umb=true).\n"
	        "  Not all DOS TSR programs can be loaded into upper memory with this command.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]lh[reset] [color=light-cyan]tsrapp[reset] [color=white]args[reset]\n");

	MSG_Add("SHELL_CMD_ATTRIB_HELP",
			"Display or change file attributes.\n");
	MSG_Add("SHELL_CMD_ATTRIB_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]attrib[reset] [color=white][ATTRIBUTES][reset] [color=light-cyan]PATTERN[reset] [/S]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]ATTRIBUTES[reset]  attributes to apply, including one or more of the following:\n"
	        "              [color=white]+R[reset], [color=white]-R[reset], [color=white]+A[reset], [color=white]-A[reset], [color=white]+S[reset], [color=white]-S[reset], [color=white]+H[reset], [color=white]-H[reset]\n"
	        "              where: R = Read-only, A = Archive, S = System, H = Hidden\n"
	        "  [color=light-cyan]PATTERN[reset]     either an exact filename or an inexact filename with wildcards,\n"
	        "              which are the asterisk (*) and the question mark (?), or an exact\n"
	        "              name of a directory\n"
	        "\n"
	        "Notes:\n"
	        "  - Multiple attributes can be specified in the command, separated by spaces.\n"
	        "  - If not specified, the command shows the current file/directory attributes.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]attrib[reset] [color=light-cyan]file.txt[reset]\n"
	        "  [color=light-green]attrib[reset] [color=white]+R[reset] [color=white]-A[reset] [color=light-cyan]*.txt[reset]\n");
	MSG_Add("SHELL_CMD_ATTRIB_GET_ERROR", "Unable to get attributes: %s\n");
	MSG_Add("SHELL_CMD_ATTRIB_SET_ERROR", "Unable to set attributes: %s\n");

	MSG_Add("SHELL_CMD_CHOICE_HELP",
	        "Wait for a keypress and set an ERRORLEVEL value.\n");
	MSG_Add("SHELL_CMD_CHOICE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]choice[reset] [color=light-cyan][TEXT][reset]\n"
	        "  [color=light-green]choice[reset] /c[:][color=white]CHOICES[reset] /n /s /t[:][color=white]c[reset],[color=light-magenta]nn[reset] [color=light-cyan][TEXT][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]TEXT[reset]          text to display as a prompt, or empty\n"
	        "  /c[:][color=white]CHOICES[reset]  specify allowable keys, which default to [color=white]yn[reset]\n"
	        "  /n            do not display the choices at end of prompt\n"
	        "  /s            enable case-sensitive choices to be selected\n"
	        "  /t[:][color=white]c[reset],[color=light-magenta]nn[reset]     choose [color=white]c[reset] by default after [color=light-magenta]nn[reset] seconds\n"
	        "\n"
	        "Notes:\n"
	        "  This command sets an ERRORLEVEL value starting from 1 according to the\n"
	        "  allowable keys specified in /c option, and the user input can then be checked\n"
	        "  with the [color=light-green]if[reset] command. With /n option only the specified text will be displayed,\n"
	        "  but not the actual choices (such as the default [color=white][Y,N]?[reset]) in the end.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]choice[reset] /t:[color=white]y[reset],[color=light-magenta]2[reset] [color=light-cyan]Continue[reset]\n"
	        "  [color=light-green]choice[reset] /c:[color=white]abc[reset] /s [color=light-cyan]Type the letter a, b, or c[reset]\n");
	MSG_Add("SHELL_CMD_CHOICE_EOF", "\n[color=light-red]Choice failed[reset]: the input stream ended without a valid choice.\n");
	MSG_Add("SHELL_CMD_CHOICE_ABORTED", "\n[color=yellow]Choice aborted.[reset]\n");

	MSG_Add("SHELL_CMD_PATH_HELP",
	        "Display or set a search path for executable files.\n");
	MSG_Add("SHELL_CMD_PATH_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]path[reset]\n"
	        "  [color=light-green]path[reset] [color=light-cyan][[DRIVE:]PATH[;...]][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan][[DRIVE:]PATH[;...]][reset]  path(s) containing a drive and directory\n"
	        "\n"
	        "Notes:\n"
	        "  - More than one path can be specified, separated by a semi-colon (;).\n"
	        "  - Parameter with only a semi-colon (;) clears all search path settings.\n"
	        "  - The path can also be set using the [color=light-green]set[reset] command, e.g. [color=light-green]set[reset] [color=white]path[reset]=[color=light-cyan]Z:\\[reset]\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]path[reset]\n"
	        "  [color=light-green]path[reset] [color=light-cyan]Z:\\;C:\\DOS[reset]\n");

	MSG_Add("SHELL_CMD_VER_HELP", "Display the DOS version.\n");
	MSG_Add("SHELL_CMD_VER_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]ver[reset]\n"
	        "\n"
	        "Notes:\n"
	        "  - The DOS version can be set in the configuration file under the [dos]\n"
	        "    section, using the 'ver = [color=light-cyan]VERSION[reset]' setting.\n"
	        "  - The DOS version reported to applications can be changed using the [color=light-green]setver[reset]\n"
	        "    command.\n"
	        "  - The old '[color=light-green]ver[reset] [color=white]set[reset] [color=light-cyan]VERSION[reset]' syntax to change the DOS version is deprecated.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]ver[reset]\n");
	MSG_Add("SHELL_CMD_VER_VER", "DOSBox Staging version %s\n"
	                             "DOS version %d.%02d\n");
	MSG_Add("SHELL_CMD_VER_INVALID", "The specified DOS version is not correct.\n");

	MSG_Add("SHELL_CMD_VOL_HELP",
	        "Display the disk volume and serial number, if they exist.\n");
	MSG_Add("SHELL_CMD_VOL_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]vol[reset] [color=light-cyan][DRIVE:][reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DRIVE[reset]  drive letter followed by a colon\n"
	        "\n"
	        "Notes:\n"
	        "  Running [color=light-green]vol[reset] without an argument uses the current drive.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]vol[reset]\n"
	        "  [color=light-green]vol[reset] [color=light-cyan]c:[reset]\n");
	MSG_Add("SHELL_CMD_VOL_OUTPUT",
	        "\n"
	        " Volume in drive %c is %s\n"
	        " Volume Serial Number is %04X-%04X\n"
	        "\n");

	MSG_Add("SHELL_CMD_MOVE_HELP",
	        "Move files and rename files and directories.\n");
	MSG_Add("SHELL_CMD_MOVE_HELP_LONG",
	        "Usage:\n"
	        "  [color=light-green]move[reset] [color=white]FILENAME1[,FILENAME2,...][reset] [color=light-cyan]DESTINATION[reset]\n"
	        "  [color=light-green]move[reset] [color=white]DIRECTORY1[reset] [color=light-cyan]DIRECTORY2[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]FILENAME[reset]     either an exact filename or an inexact filename with wildcards,\n"
	        "               which are the asterisk (*) and the question mark (?);\n"
	        "               multiple, comma-separated, filenames can be provided\n"
	        "  [color=white]DIRECTORY[reset]    exact directory name, not containing any wildcards\n"
	        "  [color=light-cyan]DESTINATION[reset]  exact filename or directory, not containing any wildcards\n"
	        "\n"
	        "Notes:\n"
	        "  - If multiple source files are specified, [color=light-cyan]DESTINATION[reset] must be a directory.\n"
	        "    If not, one will be created for you.\n"
	        "  - If a single source file is specified, it will overwrite [color=light-cyan]DESTINATION[reset].\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]move[reset] [color=white]source.bat[reset] [color=light-cyan]new.bat[reset]\n"
	        "  [color=light-green]move[reset] [color=white]file1.txt,file2.txt[reset] [color=light-cyan]mydir[reset]\n");
	MSG_Add("SHELL_CMD_MOVE_MULTIPLE_TO_SINGLE",
	        "Cannot move multiple files to a single file.\n");
	MSG_Add("SHELL_CMD_FOR_HELP",
	        "Run a specified command for each string in a set.\n");
	MSG_Add("SHELL_CMD_FOR_HELP_LONG",
		"Usage:\n"
		"  [color=light-green]for[reset] [color=white]%VAR[reset] [color=light-cyan]in[reset] [color=white](SET)[reset] [color=light-cyan]do[reset] [color=white]COMMAND[reset]\n"
		"\n"
		"Parameters:\n"
		"  [color=white]%VAR[reset]     single character representing a variable, prefixed by a '%'\n"
		"  [color=light-cyan]in[reset]       case-insensitive keyword\n"
		"  [color=white](SET)[reset]    set of strings to replace [color=white]%VAR[reset] instances in [color=white]COMMAND[reset]\n"
		"  [color=light-cyan]do[reset]       case-insensitive keyword\n"
		"  [color=white]COMMAND[reset]  command to repeat for each string in [color=white](SET)[reset]\n"
		"\n"
		"Notes:\n"
		"  - In batch files, [color=white]%VAR[reset] must be written as [color=white]%%VAR[reset] (two percent signs) instead.\n"
		"  - Strings in [color=white](SET)[reset] may be separated by any valid DOS separator.\n"
		"  - Any string in [color=white](SET)[reset] containing wildcards (* or ?) will expand to\n" 
		"    the set of matching files in the current directory.\n"
		"  - Using another [color=light-green]for[reset] command as [color=white]COMMAND[reset] is not permitted.\n"
		"\n"
		"Examples:\n"
		"  [color=light-green]for[reset] [color=white]%C[reset] [color=light-cyan]in[reset] [color=white](ONE TWO)[reset] [color=light-cyan]do[reset] [color=white]MKDIR[reset] [color=white]%C[reset]\n"
		"  [color=light-green]for[reset] [color=white]%D[reset] [color=light-cyan]in[reset] [color=white](*.TXT)[reset] [color=light-cyan]do[reset] [color=white]ECHO[reset] [color=white]%D[reset]\n"
	);

	/* Ensure help categories are loaded into the message vector */
	HELP_AddMessages();

	/* Regular startup */
	call_shellstop=CALLBACK_Allocate();
	/* Setup the startup CS:IP to kill the last running machine when exitted */
	RealPt newcsip=CALLBACK_RealPointer(call_shellstop);
	SegSet16(cs,RealSegment(newcsip));
	reg_ip=RealOffset(newcsip);

	CALLBACK_Setup(call_shellstop,shellstop_handler,CB_IRET, "shell stop");
	PROGRAMS_MakeFile("COMMAND.COM",SHELL_ProgramCreate);

	/* Now call up the shell for the first time */
	uint16_t psp_seg=DOS_FIRST_SHELL;
	uint16_t env_seg=DOS_FIRST_SHELL+19; //DOS_GetMemory(1+(4096/16))+1;
	uint16_t stack_seg=DOS_GetMemory(2048/16);
	SegSet16(ss,stack_seg);
	reg_sp=2046;

	/* Set up int 24 and psp (Telarium games) */
	real_writeb(psp_seg+16+1,0,0xea);		/* far jmp */
	real_writed(psp_seg+16+1,1,real_readd(0,0x24*4));
	real_writed(0,0x24*4,((uint32_t)psp_seg<<16) | ((16+1)<<4));

	/* Set up int 23 to "int 20" in the psp. Fixes what.exe */
	real_writed(0,0x23*4,((uint32_t)psp_seg<<16));

	/* Set up int 2e handler */
	Bitu call_int2e=CALLBACK_Allocate();
	RealPt addr_int2e=RealMake(psp_seg+16+1,8);
	CALLBACK_Setup(call_int2e,&INT2E_Handler,CB_IRET_STI,RealToPhysical(addr_int2e), "Shell Int 2e");
	RealSetVec(0x2e,addr_int2e);

	/* Setup MCBs */
	DOS_MCB pspmcb((uint16_t)(psp_seg-1));
	pspmcb.SetPSPSeg(psp_seg);	// MCB of the command shell psp
	pspmcb.SetSize(0x10+2);
	pspmcb.SetType(0x4d);
	DOS_MCB envmcb((uint16_t)(env_seg-1));
	envmcb.SetPSPSeg(psp_seg);	// MCB of the command shell environment
	envmcb.SetSize(DOS_MEM_START-env_seg);
	envmcb.SetType(0x4d);

	/* Setup environment */
	PhysPt env_write=PhysicalMake(env_seg,0);
	MEM_BlockWrite(env_write,path_string,(Bitu)(strlen(path_string)+1));
	env_write += (PhysPt)(strlen(path_string)+1);
	MEM_BlockWrite(env_write,comspec_string,(Bitu)(strlen(comspec_string)+1));
	env_write += (PhysPt)(strlen(comspec_string)+1);
	mem_writeb(env_write++,0);
	mem_writew(env_write,1);
	env_write+=2;
	MEM_BlockWrite(env_write,full_name,(Bitu)(strlen(full_name)+1));

	DOS_PSP psp(psp_seg);
	psp.MakeNew(0);
	dos.psp(psp_seg);

	/* The start of the filetable in the psp must look like this:
	 * 01 01 01 00 02
	 * In order to achieve this: First open 2 files. Close the first and
	 * duplicate the second (so the entries get 01) */
	uint16_t dummy=0;
	DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDIN  */
	DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDOUT */
	DOS_CloseFile(0);							/* Close STDIN */
	DOS_ForceDuplicateEntry(1,0);				/* "new" STDIN */
	DOS_ForceDuplicateEntry(1,2);				/* STDERR */
	DOS_OpenFile("CON",OPEN_READWRITE,&dummy);	/* STDAUX */
	DOS_OpenFile("PRN",OPEN_READWRITE,&dummy);	/* STDPRN */

	/* Create appearance of handle inheritance by first shell */
	for (uint16_t i=0;i<5;i++) {
		uint8_t handle=psp.GetFileHandle(i);
		if (Files[handle]) Files[handle]->AddRef();
	}

	psp.SetParent(psp_seg);
	/* Set the environment */
	psp.SetEnvironment(env_seg);
	/* Set the command line for the shell start up */
	CommandTail tail;
	tail.count=(uint8_t)strlen(init_line);
	memset(&tail.buffer,0,127);
	safe_strcpy(tail.buffer, init_line);
	MEM_BlockWrite(PhysicalMake(psp_seg,128),&tail,128);

	/* Setup internal DOS Variables */
	dos.dta(RealMake(psp_seg,0x80));
	dos.psp(psp_seg);

	// Load SETVER fake version table from external file
	SETVER::LoadTableFromFile();

	// first_shell is only setup here, so may as well invoke
	// it's constructor directly
	first_shell = new DOS_Shell;
	first_shell->Run();
	delete first_shell;
	first_shell = nullptr; // Make clear that it shouldn't be used anymore
	
	global_shell_history.reset();
}
