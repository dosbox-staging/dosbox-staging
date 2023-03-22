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

#include "programs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

#include "../dos/program_more_output.h"
#include "callback.h"
#include "control.h"
#include "cross.h"
#include "hardware.h"
#include "mapper.h"
#include "regs.h"
#include "shell.h"
#include "string_utils.h"
#include "support.h"

callback_number_t call_program = 0;

/* This registers a file on the virtual drive and creates the correct structure
 * for it*/

constexpr std::array<uint8_t, 19> exe_block = {
        0xbc, 0x00, 0x04,       // MOV SP,0x400  Decrease stack size
        0xbb, 0x40, 0x00,       // MOV BX,0x040  For memory resize
        0xb4, 0x4a,             // MOV AH,0x4A   Resize memory block
        0xcd, 0x21,             // INT 0x21
        0xFE, 0x38, 0x00, 0x00, // 12th position is the CALLBack number
        0xb8, 0x00, 0x4c,       // Mov ax,4c00
        0xcd, 0x21,             // INT 0x21
};

// COM data constants
constexpr int callback_pos = 12;

// Persistent program containers
using comdata_t = std::vector<uint8_t>;
static std::vector<comdata_t> internal_progs_comdata;
static std::vector<PROGRAMS_Creator> internal_progs;

static uint8_t last_written_character = '\n';
constexpr int WriteOutBufSize         = 2048;

static void write_to_stdout(std::string_view output)
{
	dos.internal_output = true;
	for (const auto& chr : output) {
		uint8_t out;
		uint16_t bytes_to_write = 1;
		if (chr == '\n' && last_written_character != '\r') {
			out = '\r';
			DOS_WriteFile(STDOUT, &out, &bytes_to_write);
		}
		out                    = static_cast<uint8_t>(chr);
		last_written_character = out;
		DOS_WriteFile(STDOUT, &out, &bytes_to_write);
	}
	dos.internal_output = false;
}

static void truncated_chars_message(int size)
{
	if (size > WriteOutBufSize) {
		constexpr int MessageSize = 128;
		char message[MessageSize];
		snprintf(message,
		         MessageSize,
		         "\n\nERROR: OUTPUT TOO LONG: %d CHARS TRUNCATED",
		         size - WriteOutBufSize);
		write_to_stdout(message);
	}
}

void PROGRAMS_MakeFile(const char* name, PROGRAMS_Creator creator)
{
	comdata_t comdata(exe_block.begin(), exe_block.end());
	comdata.at(callback_pos) = static_cast<uint8_t>(call_program & 0xff);

	// Taking the upper 8 bits if the callback number is always zero because
	// the maximum callback number is only 128. So we just confirm that here.
	static_assert(sizeof(callback_number_t) < sizeof(uint16_t));
	constexpr uint8_t upper_8_bits_of_callback = 0;
	comdata.at(callback_pos + 1)               = upper_8_bits_of_callback;

	// Save the current program's vector index in its COM data
	const auto index = internal_progs.size();
	assert(index <= UINT8_MAX); // saving the index into an 8-bit space
	comdata.push_back(static_cast<uint8_t>(index));

	// Register the COM program with the Z:\ virtual filesystem
	VFILE_Register(name, comdata.data(), static_cast<uint32_t>(comdata.size()));

	// Register the COM data to prevent it from going out of scope
	internal_progs_comdata.push_back(std::move(comdata));

	// Register the program's main pointer
	// NOTE: This step must come after the index is saved in the COM data
	internal_progs.emplace_back(creator);

	// Register help for command
	creator()->AddToHelpList();
}

static Bitu PROGRAMS_Handler(void)
{
	/* This sets up everything for a program start up call */
	Bitu size = sizeof(uint8_t);
	uint8_t index;

	// Sanity check the exec_block size before down-casting
	constexpr auto exec_block_size = exe_block.size();
	static_assert(exec_block_size < UINT16_MAX, "Should only be 19 bytes");

	/* Read the index from program code in memory */
	PhysPt reader = PhysMake(dos.psp(),
	                         256 + static_cast<uint16_t>(exec_block_size));
	HostPt writer = (HostPt)&index;
	for (; size > 0; size--) {
		*writer++ = mem_readb(reader++);
	}
	const PROGRAMS_Creator& creator = internal_progs.at(index);
	const auto new_program          = creator();
	new_program->Run();
	return CBRET_NONE;
}

/* Main functions used in all program */

Program::Program()
{
	/* Find the command line and setup the PSP */
	psp = new DOS_PSP(dos.psp());
	/* Scan environment for filename */
	PhysPt envscan = PhysMake(psp->GetEnvironment(), 0);
	while (mem_readb(envscan)) {
		envscan += mem_strlen(envscan) + 1;
	}
	envscan += 3;
	CommandTail tail;
	MEM_BlockRead(PhysMake(dos.psp(), 128), &tail, 128);
	if (tail.count < 127) {
		tail.buffer[tail.count] = 0;
	} else {
		tail.buffer[126] = 0;
	}
	char filename[256 + 1];
	MEM_StrCopy(envscan, filename, 256);
	cmd = new CommandLine(filename, tail.buffer);
}

extern std::string full_arguments;

void Program::ChangeToLongCmd()
{
	/*
	 * Get arguments directly from the shell instead of the psp.
	 * this is done in securemode: (as then the arguments to mount and
	 * friends can only be given on the shell ( so no int 21 4b) Securemode
	 * part is disabled as each of the internal command has already protection
	 * for it. (and it breaks games like cdman) it is also done for long
	 * arguments to as it is convient (as the total commandline can be
	 * longer then 127 characters. imgmount with lot's of parameters Length
	 * of arguments can be ~120. but switch when above 100 to be sure
	 */

	if (/*control->SecureMode() ||*/ cmd->Get_arglength() > 100) {
		CommandLine* temp = new CommandLine(cmd->GetFileName(),
		                                    full_arguments.c_str());
		delete cmd;
		cmd = temp;
	}
	full_arguments.assign(""); // Clear so it gets even more save
}

bool Program::SuppressWriteOut(const char* format)
{
	// Have we encountered an executable thus far?
	static bool encountered_executable = false;
	if (encountered_executable) {
		return false;
	}
	if (control->GetStartupVerbosity() >= Verbosity::Low) {
		return false;
	}
	if (!control->cmdline->HasExecutableName()) {
		return false;
	}

	// Keep suppressing output until after we hit the first executable.
	encountered_executable = is_executable_filename(format);
	return true;
}

// TODO: Refactor messages and related functions like WriteOut that use variadic
// args to instead use format strings when dosbox-staging starts using C++20

void Program::WriteOut(const char* format, ...)
{
	if (SuppressWriteOut(format)) {
		return;
	}

	char buf[WriteOutBufSize];
	std::va_list msg;

	va_start(msg, format);
	const int size = std::vsnprintf(buf, WriteOutBufSize, format, msg) + 1;
	va_end(msg);

	write_to_stdout(buf);
	truncated_chars_message(size);
}

void Program::WriteOut(const char* format, const char* arguments)
{
	if (SuppressWriteOut(format)) {
		return;
	}

	char buf[WriteOutBufSize];
	const int size = std::snprintf(buf, WriteOutBufSize, format, arguments) + 1;

	write_to_stdout(buf);
	truncated_chars_message(size);
}

void Program::WriteOut_NoParsing(const char* str)
{
	if (SuppressWriteOut(str)) {
		return;
	}

	write_to_stdout(str);
}

void Program::ResetLastWrittenChar(char c)
{
	last_written_character = c;
}

void Program::InjectMissingNewline()
{
	if (last_written_character == '\n') {
		return;
	}

	uint16_t n          = 2;
	uint8_t dos_nl[]    = "\r\n";
	dos.internal_output = true;
	DOS_WriteFile(STDOUT, dos_nl, &n);
	dos.internal_output    = false;
	last_written_character = '\n';
}

bool Program::GetEnvStr(const char* entry, std::string& result) const
{
	/* Walk through the internal environment and see for a match */
	PhysPt env_read = PhysMake(psp->GetEnvironment(), 0);

	char env_string[1024 + 1];
	result.erase();
	if (!entry[0]) {
		return false;
	}
	do {
		MEM_StrCopy(env_read, env_string, 1024);
		if (!env_string[0]) {
			return false;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		char* equal = strchr(env_string, '=');
		if (!equal) {
			continue;
		}
		/* replace the = with \0 to get the length */
		*equal = 0;
		if (strlen(env_string) != strlen(entry)) {
			continue;
		}
		if (strcasecmp(entry, env_string) != 0) {
			continue;
		}
		/* restore the = to get the original result */
		*equal = '=';
		result = env_string;
		return true;
	} while (1);
	return false;
}

bool Program::GetEnvNum(Bitu num, std::string& result) const
{
	char env_string[1024 + 1];
	PhysPt env_read = PhysMake(psp->GetEnvironment(), 0);
	do {
		MEM_StrCopy(env_read, env_string, 1024);
		if (!env_string[0]) {
			break;
		}
		if (!num) {
			result = env_string;
			return true;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		num--;
	} while (1);
	return false;
}

Bitu Program::GetEnvCount() const
{
	PhysPt env_read = PhysMake(psp->GetEnvironment(), 0);
	Bitu num        = 0;
	while (mem_readb(env_read) != 0) {
		for (; mem_readb(env_read); env_read++) {
		};
		env_read++;
		num++;
	}
	return num;
}

bool Program::SetEnv(const char* entry, const char* new_string)
{
	PhysPt env_read = PhysMake(psp->GetEnvironment(), 0);

	// Get size of environment.
	DOS_MCB mcb(psp->GetEnvironment() - 1);
	uint16_t envsize = mcb.GetSize() * 16;

	PhysPt env_write          = env_read;
	PhysPt env_write_start    = env_read;
	char env_string[1024 + 1] = {0};
	do {
		MEM_StrCopy(env_read, env_string, 1024);
		if (!env_string[0]) {
			break;
		}
		env_read += (PhysPt)(safe_strlen(env_string) + 1);
		if (!strchr(env_string, '=')) {
			continue; /* Remove corrupt entry? */
		}
		if ((strncasecmp(entry, env_string, strlen(entry)) == 0) &&
		    env_string[strlen(entry)] == '=') {
			continue;
		}
		MEM_BlockWrite(env_write,
		               env_string,
		               (Bitu)(safe_strlen(env_string) + 1));
		env_write += (PhysPt)(safe_strlen(env_string) + 1);
	} while (1);
	/* TODO Maybe save the program name sometime. not really needed though */
	/* Save the new entry */

	// ensure room
	if (envsize <= (env_write - env_write_start) + strlen(entry) + 1 +
	                       strlen(new_string) + 2) {
		return false;
	}

	if (new_string[0]) {
		std::string bigentry(entry);
		for (std::string::iterator it = bigentry.begin();
		     it != bigentry.end();
		     ++it) {
			*it = toupper(*it);
		}
		snprintf(env_string, 1024 + 1, "%s=%s", bigentry.c_str(), new_string);
		MEM_BlockWrite(env_write,
		               env_string,
		               (Bitu)(safe_strlen(env_string) + 1));
		env_write += (PhysPt)(safe_strlen(env_string) + 1);
	}
	/* Clear out the final piece of the environment */
	mem_writeb(env_write, 0);
	return true;
}

bool Program::HelpRequested()
{
	return cmd->FindExist("/?", false) || cmd->FindExist("-h", false) ||
	       cmd->FindExist("--help", false);
}

void Program::AddToHelpList()
{
	if (help_detail.name.size()) {
		HELP_AddToHelpList(help_detail.name, help_detail);
	}
}

bool MSG_Write(const char*);
void restart_program(std::vector<std::string>& parameters);

class CONFIG final : public Program {
public:
	CONFIG()
	{
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "CONFIG"};
	}
	void Run(void);

private:
	void restart(const char* useconfig);

	void writeconf(std::string name, bool configdir)
	{
		if (configdir) {
			// write file to the default config directory
			std::string config_path;
			Cross::GetPlatformConfigDir(config_path);
			name = config_path + name;
		}
		WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_WHICH"), name.c_str());
		if (!control->PrintConfig(name)) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"), name.c_str());
		}
		return;
	}

	bool securemode_check()
	{
		if (control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return true;
		}
		return false;
	}
};

void CONFIG::Run(void)
{
	static const char* const params[] = {
	        "-r",          "-wcp",      "-wcd",       "-wc",
	        "-writeconf",  "-l",        "-rmconf",    "-h",
	        "-help",       "-?",        "-axclear",   "-axadd",
	        "-axtype",     "-avistart", "-avistop",   "-startmapper",
	        "-get",        "-set",      "-writelang", "-wl",
	        "-securemode", ""};
	enum prs {
		P_NOMATCH,
		P_NOPARAMS, // fixed return values for GetParameterFromList
		P_RESTART,
		P_WRITECONF_PORTABLE,
		P_WRITECONF_DEFAULT,
		P_WRITECONF,
		P_WRITECONF2,
		P_LISTCONF,
		P_KILLCONF,
		P_HELP,
		P_HELP2,
		P_HELP3,
		P_AUTOEXEC_CLEAR,
		P_AUTOEXEC_ADD,
		P_AUTOEXEC_TYPE,
		P_REC_AVI_START,
		P_REC_AVI_STOP,
		P_START_MAPPER,
		P_GETPROP,
		P_SETPROP,
		P_WRITELANG,
		P_WRITELANG2,
		P_SECURE
	} presult = P_NOMATCH;

	auto display_help = [this]() {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("SHELL_CMD_CONFIG_HELP_LONG"));
		output.Display();
	};

	bool first = true;
	std::vector<std::string> pvars;
	// Loop through the passed parameters
	while (presult != P_NOPARAMS) {
		presult = (enum prs)cmd->GetParameterFromList(params, pvars);
		switch (presult) {
		case P_RESTART:
			if (securemode_check()) {
				return;
			}
			if (pvars.size() == 0) {
				restart_program(control->startup_params);
			} else {
				std::vector<std::string> restart_params;
				restart_params.push_back(
				        control->cmdline->GetFileName());
				for (size_t i = 0; i < pvars.size(); i++) {
					restart_params.push_back(pvars[i]);
				}
				// the rest on the commandline, too
				cmd->FillVector(restart_params);
				restart_program(restart_params);
			}
			return;

		case P_LISTCONF: {
			Bitu size = control->configfiles.size();
			std::string config_path;
			Cross::GetPlatformConfigDir(config_path);
			WriteOut(MSG_Get("PROGRAM_CONFIG_CONFDIR"),
			         VERSION,
			         config_path.c_str());
			if (size == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
			} else {
				WriteOut(MSG_Get("PROGRAM_CONFIG_PRIMARY_CONF"),
				         control->configfiles.front().c_str());
				if (size > 1) {
					WriteOut(MSG_Get(
					        "PROGRAM_CONFIG_ADDITIONAL_CONF"));
					for (Bitu i = 1; i < size; i++) {
						WriteOut("%s\n",
						         control->configfiles[i].c_str());
					}
				}
			}
			if (control->startup_params.size() > 0) {
				std::string test;
				for (size_t k = 0;
				     k < control->startup_params.size();
				     k++) {
					test += control->startup_params[k] + " ";
				}
				WriteOut(MSG_Get("PROGRAM_CONFIG_PRINT_STARTUP"),
				         test.c_str());
			}
			break;
		}
		case P_WRITECONF:
		case P_WRITECONF2:
			if (securemode_check()) {
				return;
			}
			if (pvars.size() > 1) {
				return;
			} else if (pvars.size() == 1) {
				// write config to specific file, except if it
				// is an absolute path
				writeconf(pvars[0],
				          !Cross::IsPathAbsolute(pvars[0]));
			} else {
				// -wc without parameter: write primary config file
				if (control->configfiles.size()) {
					writeconf(control->configfiles[0], false);
				} else {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
				}
			}
			break;
		case P_WRITECONF_DEFAULT: {
			// write to /userdir/dosbox0.xx.conf
			if (securemode_check()) {
				return;
			}
			if (pvars.size() > 0) {
				return;
			}
			std::string confname;
			Cross::GetPlatformConfigName(confname);
			writeconf(confname, true);
			break;
		}
		case P_WRITECONF_PORTABLE:
			if (securemode_check()) {
				return;
			}
			if (pvars.size() > 1) {
				return;
			} else if (pvars.size() == 1) {
				// write config to startup directory
				writeconf(pvars[0], false);
			} else {
				// -wcp without parameter: write dosbox.conf to
				// startup directory
				if (control->configfiles.size()) {
					writeconf(std::string("dosbox.conf"), false);
				} else {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
				}
			}
			break;

		case P_NOPARAMS:
			if (!first) {
				break;
			}
			[[fallthrough]];

		case P_NOMATCH: display_help(); return;

		case P_HELP:
		case P_HELP2:
		case P_HELP3: {
			switch (pvars.size()) {
			case 0: display_help(); return;
			case 1: {
				if (!strcasecmp("sections", pvars[0].c_str())) {
					// list the sections
					WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_SECTLIST"));
					for (const Section* sec : *control) {
						WriteOut("%s\n", sec->GetName());
					}
					return;
				}
				// if it's a section, leave it as one-param
				Section* sec = control->GetSection(pvars[0].c_str());
				if (!sec) {
					// could be a property
					sec = control->GetSectionFromProperty(
					        pvars[0].c_str());
					if (!sec) {
						WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
						         pvars[0].c_str());
						return;
					}
					pvars.insert(pvars.begin(),
					             std::string(sec->GetName()));
				}
				break;
			}
			case 2: {
				// sanity check
				Section* sec = control->GetSection(pvars[0].c_str());
				Section* sec2 = control->GetSectionFromProperty(
				        pvars[1].c_str());
				if (!sec) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
					         pvars[0].c_str());
				} else if (!sec2 || sec != sec2) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
					         pvars[1].c_str());
				}
				break;
			}
			default: display_help(); return;
			}
			// if we have one value in pvars, it's a section
			// two values are section + property
			Section* sec = control->GetSection(pvars[0].c_str());
			if (sec == NULL) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
				         pvars[0].c_str());
				return;
			}
			Section_prop* psec = dynamic_cast<Section_prop*>(sec);
			if (psec == NULL) {
				// failed; maybe it's the autoexec section?
				Section_line* pline = dynamic_cast<Section_line*>(sec);
				if (pline == NULL) {
					E_Exit("Section dynamic cast failed.");
				}

				WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_LINEHLP"),
				         pline->GetName(),
				         // this is 'unclean' but the autoexec
				         // section has no help associated
				         MSG_Get("AUTOEXEC_CONFIGFILE_HELP"),
				         pline->data.c_str());
				return;
			}
			if (pvars.size() == 1) {
				size_t i = 0;
				WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_SECTHLP"),
				         pvars[0].c_str());
				while (true) {
					// list the properties
					Property* p = psec->Get_prop(i++);
					if (p == NULL) {
						break;
					}
					WriteOut("%s\n", p->propname.c_str());
				}
			} else {
				// find the property by it's name
				size_t i = 0;
				while (true) {
					Property* p = psec->Get_prop(i++);
					if (p == NULL) {
						break;
					}
					if (!strcasecmp(p->propname.c_str(),
					                pvars[1].c_str())) {
						// found it; make the list of
						// possible values
						std::string propvalues;
						std::vector<Value> pv = p->GetValues();

						if (p->Get_type() == Value::V_BOOL) {
							// possible values for
							// boolean are true, false
							propvalues += "true, false";
						} else if (p->Get_type() ==
						           Value::V_INT) {
							// print min, max for
							// integer values if used
							Prop_int* pint =
							        dynamic_cast<Prop_int*>(
							                p);
							if (pint == NULL) {
								E_Exit("Int property dynamic cast failed.");
							}
							if (pint->GetMin() !=
							    pint->GetMax()) {
								std::ostringstream oss;
								oss << pint->GetMin();
								oss << "..";
								oss << pint->GetMax();
								propvalues += oss.str();
							}
						}
						for (Bitu k = 0; k < pv.size(); k++) {
							if (pv[k].ToString() == "%u") {
								propvalues += MSG_Get(
								        "PROGRAM_CONFIG_HLP_POSINT");
							} else {
								propvalues += pv[k].ToString();
							}
							if ((k + 1) < pv.size()) {
								propvalues += ", ";
							}
						}

						WriteOut(
						        MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP"),
						        p->propname.c_str(),
						        sec->GetName(),
						        p->GetHelp(),
						        propvalues.c_str(),
						        p->GetDefaultValue()
						                .ToString()
						                .c_str(),
						        p->GetValue().ToString().c_str());
						// print 'changability'
						if (p->GetChange() ==
						    Property::Changeable::OnlyAtStart) {
							WriteOut(MSG_Get(
							        "PROGRAM_CONFIG_HLP_NOCHANGE"));
						}
						return;
					}
				}
				break;
			}
			return;
		}
		case P_AUTOEXEC_CLEAR: {
			Section_line* sec = dynamic_cast<Section_line*>(
			        control->GetSection(std::string("autoexec")));
			if (!sec) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"));
				return;
			}
			sec->data.clear();
			break;
		}
		case P_AUTOEXEC_ADD: {
			if (pvars.size() == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_MISSINGPARAM"));
				return;
			}
			Section_line* sec = dynamic_cast<Section_line*>(
			        control->GetSection(std::string("autoexec")));
			if (!sec) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"));
				return;
			}
			for (Bitu i = 0; i < pvars.size(); i++) {
				sec->HandleInputline(pvars[i]);
			}
			break;
		}
		case P_AUTOEXEC_TYPE: {
			Section_line* sec = dynamic_cast<Section_line*>(
			        control->GetSection(std::string("autoexec")));
			if (!sec) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"));
				return;
			}
			WriteOut("\n%s", sec->data.c_str());
			break;
		}
		case P_REC_AVI_START: CAPTURE_VideoStart(); break;
		case P_REC_AVI_STOP: CAPTURE_VideoStop(); break;
		case P_START_MAPPER:
			if (securemode_check()) {
				return;
			}
			MAPPER_Run(false);
			break;
		case P_GETPROP: {
			// "section property"
			// "property"
			// "section"
			// "section" "property"
			if (pvars.size() == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}
			std::string::size_type spcpos = pvars[0].find_first_of(' ');
			// split on the ' '
			if (spcpos != std::string::npos) {
				pvars.insert(pvars.begin() + 1,
				             pvars[0].substr(spcpos + 1));
				pvars[0].erase(spcpos);
			}
			switch (pvars.size()) {
			case 1: {
				// property/section only
				// is it a section?
				Section* sec = control->GetSection(pvars[0].c_str());
				if (sec) {
					// list properties in section
					Bitu i = 0;
					Section_prop* psec =
					        dynamic_cast<Section_prop*>(sec);
					if (psec == NULL) {
						// autoexec section
						Section_line* pline =
						        dynamic_cast<Section_line*>(sec);
						if (pline == NULL) {
							E_Exit("Section dynamic cast failed.");
						}

						WriteOut("%s", pline->data.c_str());
						break;
					}
					while (true) {
						// list the properties
						Property* p = psec->Get_prop(i++);
						if (p == NULL) {
							break;
						}
						WriteOut(
						        "%s=%s\n",
						        p->propname.c_str(),
						        p->GetValue().ToString().c_str());
					}
				} else {
					// no: maybe it's a property?
					sec = control->GetSectionFromProperty(
					        pvars[0].c_str());
					if (!sec) {
						WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
						         pvars[0].c_str());
						return;
					}
					// it's a property name
					std::string val = sec->GetPropValue(
					        pvars[0].c_str());
					WriteOut("%s", val.c_str());
					first_shell->SetEnv("CONFIG", val.c_str());
				}
				break;
			}
			case 2: {
				// section + property
				const char* sec_name  = pvars[0].c_str();
				const char* prop_name = pvars[1].c_str();
				const Section* sec = control->GetSection(sec_name);
				if (!sec) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"),
					         sec_name);
					return;
				}
				const std::string val = sec->GetPropValue(prop_name);
				if (val == NO_SUCH_PROPERTY) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
					         prop_name,
					         sec_name);
					return;
				}
				WriteOut("%s\n", val.c_str());
				first_shell->SetEnv("CONFIG", val.c_str());
				break;
			}
			default:
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}
			return;
		}
		case P_SETPROP: {
			// Code for the configuration changes
			// Official format: config -set "section property=value"
			// Accepted: with or without -set,
			// "section property value"
			// "section property=value"
			// "property" "value"
			// "section" "property=value"
			// "section" "property=value" "value" "value" ...
			// "section" "property" "value" "value" ...
			// "section property" "value" "value" ...
			// "property" "value" "value" ...
			// "property=value" "value" "value" ...

			if (pvars.size() == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return;
			}

			// add rest of command
			std::string rest;
			if (cmd->GetStringRemain(rest)) {
				pvars.push_back(rest);
			}
			const char* result = SetProp(pvars);
			if (strlen(result)) {
				WriteOut(result);
			} else {
				Section* tsec = control->GetSection(pvars[0]);
				// Input has been parsed (pvar[0]=section,
				// [1]=property, [2]=value) now execute
				std::string value(pvars[2]);
				// Due to parsing there can be a = at the start
				// of value.
				while (value.size() && (value.at(0) == ' ' ||
				                        value.at(0) == '=')) {
					value.erase(0, 1);
				}
				for (Bitu i = 3; i < pvars.size(); i++) {
					value += (std::string(" ") + pvars[i]);
				}
				if (value.empty()) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
					return;
				}
				std::string inputline = pvars[1] + "=" + value;
				tsec->ExecuteDestroy(false);
				bool change_success = tsec->HandleInputline(
				        inputline.c_str());

				if (!change_success) {
					auto val = value;
					trim(val);
					WriteOut(MSG_Get("PROGRAM_CONFIG_VALUE_ERROR"),
					         val.c_str(),
					         pvars[1].c_str());
				}
				tsec->ExecuteInit(false);
			}
			return;
		}
		case P_WRITELANG:
		case P_WRITELANG2:
			// In secure mode don't allow a new languagefile to be
			// created Who knows which kind of file we would overwrite.
			if (securemode_check()) {
				return;
			}
			if (pvars.size() < 1) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_MISSINGPARAM"));
				return;
			}
			if (!MSG_Write(pvars[0].c_str())) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),
				         pvars[0].c_str());
				return;
			}
			break;

		case P_SECURE:
			// Code for switching to secure mode
			control->SwitchToSecureMode();
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_ON"));
			return;

		default: E_Exit("bug"); break;
		}
		first = false;
	}
}

std::unique_ptr<Program> CONFIG_ProgramCreate()
{
	return ProgramCreate<CONFIG>();
}

void PROGRAMS_Destroy([[maybe_unused]] Section* sec)
{
	internal_progs_comdata.clear();
	internal_progs.clear();
}

void PROGRAMS_Init(Section* sec)
{
	/* Setup a special callback to start virtual programs */
	call_program = CALLBACK_Allocate();
	CALLBACK_Setup(call_program, &PROGRAMS_Handler, CB_RETF, "internal program");

	// Cleanup -- allows unit tests to run indefinitely & cleanly
	sec->AddDestroyFunction(&PROGRAMS_Destroy);

	// listconf
	MSG_Add("PROGRAM_CONFIG_NOCONFIGFILE", "No config file loaded!\n");
	MSG_Add("PROGRAM_CONFIG_PRIMARY_CONF", "Primary config file: \n%s\n");
	MSG_Add("PROGRAM_CONFIG_ADDITIONAL_CONF", "Additional config files:\n");
	MSG_Add("PROGRAM_CONFIG_CONFDIR",
	        "DOSBox Staging %s configuration directory: \n%s\n\n");

	// writeconf
	MSG_Add("PROGRAM_CONFIG_FILE_ERROR", "\nCan't open file %s\n");
	MSG_Add("PROGRAM_CONFIG_FILE_WHICH", "Writing config file %s\n");

	// help
	MSG_Add("SHELL_CMD_CONFIG_HELP_LONG",
	        "Adjusts DOSBox Staging's configurable parameters.\n"
	        "-writeconf or -wc without parameter: write to primary loaded config file.\n"
	        "-writeconf or -wc with filename: write file to config directory.\n"
	        "Use -writelang or -wl filename to write the current language strings.\n"
	        "-r [parameters]\n"
	        " Restart DOSBox, either using the previous parameters or any that are appended.\n"
	        "-wcp [filename]\n"
	        " Write config file to the program directory, dosbox.conf or the specified\n"
	        " filename.\n"
	        "-wcd\n"
	        " Write to the default config file in the config directory.\n"
	        "-l lists configuration parameters.\n"
	        "-h, -help, -? sections / sectionname / propertyname\n"
	        " Without parameters, displays this help screen. Add \"sections\" for a list of\n"
	        " sections."
	        " For info about a specific section or property add its name behind.\n"
	        "-axclear clears the autoexec section.\n"
	        "-axadd [line] adds a line to the autoexec section.\n"
	        "-axtype prints the content of the autoexec section.\n"
	        "-securemode switches to secure mode.\n"
	        "-avistart starts AVI recording.\n"
	        "-avistop stops AVI recording.\n"
	        "-startmapper starts the keymapper.\n"
	        "-get \"section property\" returns the value of the property.\n"
	        "-set \"section property=value\" sets the value.\n");
	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP",
	        "Purpose of property \"%s\" (contained in section \"%s\"):\n%s\n\nPossible Values: %s\nDefault value: %s\nCurrent value: %s\n");
	MSG_Add("PROGRAM_CONFIG_HLP_LINEHLP",
	        "Purpose of section \"%s\":\n%s\nCurrent value:\n%s\n");
	MSG_Add("PROGRAM_CONFIG_HLP_NOCHANGE",
	        "This property cannot be changed at runtime.\n");
	MSG_Add("PROGRAM_CONFIG_HLP_POSINT", "positive integer");
	MSG_Add("PROGRAM_CONFIG_HLP_SECTHLP",
	        "Section %s contains the following properties:\n");
	MSG_Add("PROGRAM_CONFIG_HLP_SECTLIST",
	        "DOSBox configuration contains the following sections:\n\n");

	MSG_Add("PROGRAM_CONFIG_SECURE_ON", "Switched to secure mode.\n");
	MSG_Add("PROGRAM_CONFIG_SECURE_DISALLOW",
	        "This operation is not permitted in secure mode.\n");
	MSG_Add("PROGRAM_CONFIG_SECTION_ERROR", "Section \"%s\" doesn't exist.\n");
	MSG_Add("PROGRAM_CONFIG_VALUE_ERROR",
	        "\"%s\" is not a valid value for property \"%s\".\n");
	MSG_Add("PROGRAM_CONFIG_GET_SYNTAX",
	        "Correct syntax: config -get \"section property\".\n");
	MSG_Add("PROGRAM_CONFIG_PRINT_STARTUP",
	        "\nDOSBox was started with the following command line parameters:\n%s\n");
	MSG_Add("PROGRAM_CONFIG_MISSINGPARAM", "Missing parameter.\n");
	MSG_Add("PROGRAM_PATH_TOO_LONG",
	        "The path \"%s\" exceeds the DOS maximum length of %d characters\n");
	MSG_Add("PROGRAM_EXECUTABLE_MISSING", "Executable file not found: %s\n");
	MSG_Add("CONJUNCTION_AND", "and");
}
