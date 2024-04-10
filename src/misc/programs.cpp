/*
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
#include "../capture/capture.h"
#include "callback.h"
#include "control.h"
#include "cross.h"
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
constexpr int WriteOutBufSize         = 4096;

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
	PhysPt reader = PhysicalMake(dos.psp(),
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
	PhysPt envscan = PhysicalMake(psp->GetEnvironment(), 0);
	while (mem_readb(envscan)) {
		envscan += mem_strlen(envscan) + 1;
	}
	envscan += 3;
	CommandTail tail;
	MEM_BlockRead(PhysicalMake(dos.psp(), 128), &tail, 128);
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
	void Run(void) override;

private:
	void restart(const char* useconfig);

	void WriteConfig(const std::string& name)
	{
		WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_WHICH"), name.c_str());
		if (!control->WriteConfig(name)) {
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
	        "-r",        "-wcd",       "-wc",          "-writeconf",
	        "-l",        "-rmconf",    "-h",           "-help",
	        "-?",        "-axclear",   "-axadd",       "-axtype",
	        "-avistart", "-avistop",   "-startmapper", "-get",
	        "-set",      "-writelang", "-wl",          "-securemode",
	        ""};
	enum prs {
		P_NOMATCH,
		P_NOPARAMS, // fixed return values for GetParameterFromList
		P_RESTART,
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
				const auto remaining_args = cmd->GetArguments();
				restart_params.insert(restart_params.end(),
				                      remaining_args.begin(),
				                      remaining_args.end());

				restart_program(restart_params);
			}
			return;

		case P_LISTCONF: {
			Bitu size = control->configfiles.size();
			const std_fs::path config_path = GetConfigDir();
			WriteOut(MSG_Get("PROGRAM_CONFIG_CONFDIR"),
			         DOSBOX_GetVersion(),
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
		case P_WRITECONF_DEFAULT: {
			if (securemode_check()) {
				return;
			}
			if (pvars.size() > 0) {
				WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
				return;
			}
			WriteConfig(GetPrimaryConfigPath().string());
			break;
		}
		case P_WRITECONF:
		case P_WRITECONF2:
			if (securemode_check()) {
				return;
			}
			if (pvars.size() > 1) {
				WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
				return;
			}

			if (pvars.size() == 1) {
				// write config to startup directory
				WriteConfig(pvars[0]);
			} else {
				// -wc without parameter: write dosbox.conf to
				// startup directory
				if (control->configfiles.size()) {
					WriteConfig("dosbox.conf");
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
						if (sec->IsActive()) {
							WriteOut("  - %s\n",
							         sec->GetName());
						}
					}
					return;
				}
				// if it's a section, leave it as one-param
				Section* sec = control->GetSection(pvars[0]);
				if (!sec || !sec->IsActive()) {
					// could be a property
					sec = control->GetSectionFromProperty(
					        pvars[0].c_str());
					if (!sec || !sec->IsActive()) {
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
				Section* sec  = control->GetSection(pvars[0]);
				if (!sec->IsActive()) {
					sec = nullptr;
				}
				Section* sec2 = control->GetSectionFromProperty(
				        pvars[1].c_str());
				if (!sec || !sec->IsActive()) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
					         pvars[0].c_str());
				} else if (!sec2 || !sec2->IsActive() || sec != sec2) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
					         pvars[1].c_str());
				}
				break;
			}
			default: display_help(); return;
			}
			// if we have one value in pvars, it's a section
			// two values are section + property
			Section* sec = control->GetSection(pvars[0]);
			if (sec == nullptr || !sec->IsActive()) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"),
				         pvars[0].c_str());
				return;
			}
			Section_prop* psec = dynamic_cast<Section_prop*>(sec);
			if (psec == nullptr) {
				// failed; maybe it's the autoexec section?
				Section_line* pline = dynamic_cast<Section_line*>(sec);
				if (pline == nullptr) {
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
					if (p == nullptr) {
						break;
					}
					if (p->IsDeprecated()) {
						continue;
					}
					WriteOut("  - %s\n", p->propname.c_str());
				}
			} else {
				// find the property by it's name
				size_t i = 0;
				while (true) {
					Property* p = psec->Get_prop(i++);
					if (p == nullptr) {
						break;
					}
					if (!strcasecmp(p->propname.c_str(),
					                pvars[1].c_str())) {
						// found it; make the list of
						// possible values
						std::string possible_values;
						std::vector<Value> pv = p->GetValues();

						if (p->Get_type() == Value::V_BOOL) {
							// possible values for
							// boolean are true, false
							possible_values += "true, false";
						} else if (p->Get_type() ==
						           Value::V_INT) {
							// print min, max for
							// integer values if used
							Prop_int* pint =
							        dynamic_cast<Prop_int*>(
							                p);
							if (pint == nullptr) {
								E_Exit("Int property dynamic cast failed.");
							}
							if (pint->GetMin() !=
							    pint->GetMax()) {
								std::ostringstream oss;
								oss << pint->GetMin();
								oss << "..";
								oss << pint->GetMax();
								possible_values +=
								        oss.str();
							}
						}
						for (Bitu k = 0; k < pv.size(); k++) {
							if (pv[k].ToString() == "%u") {
								possible_values += MSG_Get(
								        "PROGRAM_CONFIG_HLP_POSINT");
							} else {
								possible_values +=
								        pv[k].ToString();
							}
							if ((k + 1) < pv.size()) {
								possible_values += ", ";
							}
						}

						WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP"),
						         p->propname.c_str(),
						         sec->GetName(),
						         p->GetHelp().c_str());

						if (!p->IsDeprecated()) {
							if (!possible_values.empty()) {
								WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP_POSSIBLE_VALUES"),
								         possible_values
								                 .c_str());
							}

							WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP_DEFAULT_VALUE"),
							         p->GetDefaultValue()
							                 .ToString()
							                 .c_str());

							WriteOut(MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP_CURRENT_VALUE"),
							         p->GetValue()
							                 .ToString()
							                 .c_str());
						}

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
			for (const auto& pvar : pvars) {
				std::string line_utf8 = {};
				dos_to_utf8(pvar, line_utf8);
				sec->HandleInputline(line_utf8);
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
			std::string line_dos = {};
			utf8_to_dos(sec->data, line_dos, UnicodeFallback::Box);
			WriteOut("\n%s", line_dos.c_str());
			break;
		}
		case P_REC_AVI_START: CAPTURE_StartVideoCapture(); break;
		case P_REC_AVI_STOP: CAPTURE_StopVideoCapture(); break;
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
				Section* sec = control->GetSection(pvars[0]);
				if (sec) {
					// list properties in section
					Bitu i = 0;
					Section_prop* psec =
					        dynamic_cast<Section_prop*>(sec);
					if (psec == nullptr) {
						// autoexec section
						Section_line* pline =
						        dynamic_cast<Section_line*>(sec);
						if (pline == nullptr) {
							E_Exit("Section dynamic cast failed.");
						}

						WriteOut("%s", pline->data.c_str());
						break;
					}
					while (true) {
						// list the properties
						Property* p = psec->Get_prop(i++);
						if (p == nullptr) {
							break;
						}
						std::string val_utf8 = p->GetValue().ToString();
						std::string val_dos = {};
						utf8_to_dos(val_utf8, val_dos,
						            UnicodeFallback::Simple);
						WriteOut("%s=%s\n",
						         p->propname.c_str(),
						         val_dos.c_str());
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
					std::string val_utf8 = sec->GetPropValue(pvars[0].c_str());
					std::string val_dos = {};
					utf8_to_dos(val_utf8, val_dos,
					            UnicodeFallback::Simple);
					WriteOut("%s", val_dos.c_str());
					DOS_PSP(psp->GetParent())
					        .SetEnvironmentValue("CONFIG",
					                             val_dos.c_str());
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
				const std::string val_utf8 = sec->GetPropValue(prop_name);
				if (val_utf8 == NO_SUCH_PROPERTY) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
					         prop_name,
					         sec_name);
					return;
				}
				std::string val_dos = {};
				utf8_to_dos(val_utf8, val_dos,
				            UnicodeFallback::Simple);
				WriteOut("%s\n", val_dos.c_str());
				DOS_PSP(psp->GetParent())
				        .SetEnvironmentValue("CONFIG", val_dos.c_str());
				break;
			}
			default:
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}
			return;
		}
		case P_SETPROP: {
			if (pvars.size() == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return;
			}

			// add rest of command
			std::string rest;
			if (cmd->GetStringRemain(rest)) {
				pvars.push_back(rest);
			}
			const char* result = control->SetProp(pvars);
			if (strlen(result)) {
				WriteOut(result);
			} else {
				auto* tsec = dynamic_cast<Section_prop *>(control->GetSection(pvars[0]));
				if (!tsec) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"), pvars[0].c_str());
					return;
				}
				const auto* property = tsec->Get_prop(pvars[1]);
				if (!property) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_PROPERTY_ERROR"), pvars[1].c_str());
					return;
				}
				if (property->GetChange() == Property::Changeable::OnlyAtStart) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NOT_CHANGEABLE"), pvars[1].c_str());
					return;
				}
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
				std::string line_utf8 = {};
				dos_to_utf8(inputline, line_utf8);
				bool change_success = tsec->HandleInputline(
				        line_utf8.c_str());

				if (!change_success) {
					trim(value);
					WriteOut(MSG_Get("PROGRAM_CONFIG_VALUE_ERROR"),
					         value.c_str(),
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
	// Setup a special callback to start virtual programs
	call_program = CALLBACK_Allocate();
	CALLBACK_Setup(call_program, &PROGRAMS_Handler, CB_RETF, "internal program");

	// TODO Cleanup -- allows unit tests to run indefinitely & cleanly
	sec->AddDestroyFunction(&PROGRAMS_Destroy);

	// List config
	MSG_Add("PROGRAM_CONFIG_NOCONFIGFILE", "No config file loaded\n");
	MSG_Add("PROGRAM_CONFIG_PRIMARY_CONF", "[color=white]Primary config file:[reset]\n  %s\n");
	MSG_Add("PROGRAM_CONFIG_ADDITIONAL_CONF", "\n[color=white]Additional config files:[reset]\n  ");

	MSG_Add("PROGRAM_CONFIG_CONFDIR",
	        "[color=white]DOSBox Staging %s configuration directory:[reset]\n  %s\n\n");

	// Write config
	MSG_Add("PROGRAM_CONFIG_FILE_ERROR", "\nCan't open config file '%s'\n");
	MSG_Add("PROGRAM_CONFIG_FILE_WHICH", "Writing current config to '%s'\n");

	// Help
	MSG_Add("SHELL_CMD_CONFIG_HELP_LONG",
	        "Perform configuration management and other miscellaneous actions.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]config[reset] [color=white]COMMAND[reset] [color=light-cyan][PARAMETERS][reset]\n"
	        "\n"
	        "Where [color=white]COMMAND[reset] is one of:\n"
	        "  -writeconf\n"
	        "  -wc               write the current configuration to the local `dosbox.conf`\n"
	        "                    config file in the current working directory\n"
	        "\n"
	        "  -writeconf [color=white]PATH[reset]\n"
	        "  -wc [color=white]PATH          [reset]if [color=white]PATH[reset] is a filename, write the current configuration to\n"
	        "                    that file in the current working directory, otherwise to the\n"
	        "                    specified absolute or relative path\n"
	        "\n"
	        "  -wcd              write the current configuration to the primary (default)\n"
	        "                    `dosbox-staging.conf` config file in the configuration\n"
	        "                    directory\n"
	        "\n"
	        "  -writelang [color=white]FILENAME[reset]\n"
	        "  -wl [color=white]FILENAME      [reset]write the current language strings to [color=white]FILENAME [reset]in the\n"
	        "                    current working directory\n"
	        "\n"
	        "  -r [color=light-cyan][PROPERTY1=VALUE1 [PROPERTY2=VALUE2 ...]][reset]\n"
	        "                    restart DOSBox with the optionally supplied config\n"
	        "                    properties\n"
	        "\n"
	        "  -l                show the currently loaded config files and command line\n"
	        "                    arguments provided at startup\n"
	        "\n"
	        "  -help [color=white]SECTION[reset]\n"
	        "  -h    [color=white]SECTION[reset]\n"
	        "  -?    [color=white]SECTION     [reset]list the names of all properties in a config section\n"
	        "\n"
	        "  -help [color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset]\n"
	        "  -h    [color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset]\n"
	        "  -?    [color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset]\n"
	        "                    show the description and the current value of a config\n"
	        "                    property\n"
	        "\n"
	        "  -axclear          clear the [autoexec] section\n"
	        "  -axadd [color=white]LINE[reset]       append a line to the end of the [autoexec] section\n"
	        "  -axtype           show the contents of the [autoexec] section\n"
	        "  -securemode       switch to secure mode\n"
	        "  -avistart         start AVI recording\n"
	        "  -avistop          stop AVI recording\n"
	        "  -startmapper      start the keymapper\n"
	        "\n"
	        "  -get [color=white]SECTION      [reset]show all properties and their values in a config section\n"
	        "  -get [color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset]\n"
	        "                    show the value of a single config property\n"
	        "\n"
	        "  -set [color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset][=][color=white]VALUE[reset]\n"
	        "                    set the value of a config property");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP",
	        "[color=white]Purpose of property [color=light-green]'%s'[color=white] "
	        "(contained in section [color=light-cyan][%s][color=white]):[reset]\n\n%s\n\n");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP_POSSIBLE_VALUES",
	        "[color=white]Possible values:[reset]  %s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP_DEFAULT_VALUE",
	        "[color=white]Default value:[reset]    %s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP_CURRENT_VALUE",
	        "[color=white]Current value:[reset]    %s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_LINEHLP",
	        "[color=white]Purpose of section [%s]:[reset]\n"
	        "%s\n[color=white]Current value:[reset]\n%s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_NOCHANGE",
	        "This property cannot be changed at runtime.\n");

	MSG_Add("PROGRAM_CONFIG_HLP_POSINT", "positive integer");

	MSG_Add("PROGRAM_CONFIG_HLP_SECTHLP",
	        "[color=white]Section [color=light-cyan][%s] [color=white]contains the following properties:[reset]\n");

	MSG_Add("PROGRAM_CONFIG_HLP_SECTLIST",
	        "[color=white]DOSBox configuration contains the following sections:[reset]\n");

	MSG_Add("PROGRAM_CONFIG_SECURE_ON", "Switched to secure mode.\n");

	MSG_Add("PROGRAM_CONFIG_SECURE_DISALLOW",
	        "This operation is not permitted in secure mode.\n");

	MSG_Add("PROGRAM_CONFIG_SECTION_ERROR", "Section [%s] doesn't exist.\n");

	MSG_Add("PROGRAM_CONFIG_VALUE_ERROR",
	        "'%s' is not a valid value for property '%s'.\n");

	MSG_Add("PROGRAM_CONFIG_GET_SYNTAX",
	        "Usage: [color=light-green]config[reset] -get "
	        "[color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset]\n");

	MSG_Add("PROGRAM_CONFIG_PRINT_STARTUP",
	        "\n[color=white]DOSBox was started with the following command line arguments:[reset]\n  %s\n");

	MSG_Add("PROGRAM_CONFIG_MISSINGPARAM", "Missing parameter.\n");

	MSG_Add("PROGRAM_PATH_TOO_LONG",
	        "The path '%s' exceeds the DOS limit of %d characters.\n");

	MSG_Add("PROGRAM_EXECUTABLE_MISSING", "Executable file not found: '%s'\n");

	MSG_Add("CONJUNCTION_AND", "and");

	MSG_Add("PROGRAM_CONFIG_NOT_CHANGEABLE", "Property '%s' is not changeable at runtime.");
}
