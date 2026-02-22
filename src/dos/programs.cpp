// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/programs.h"

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

#include "capture/capture.h"
#include "config/config.h"
#include "cpu/callback.h"
#include "cpu/registers.h"
#include "dos/programs/more_output.h"
#include "gui/mapper.h"
#include "misc/cross.h"
#include "misc/support.h"
#include "misc/unicode.h"
#include "shell/shell.h"

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

constexpr int WriteOutBufSize = 16384;

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
	// This sets up everything for a program start up call
	Bitu size = sizeof(uint8_t);
	uint8_t index;

	// Sanity check the exec_block size before down-casting
	constexpr auto exec_block_size = exe_block.size();
	static_assert(exec_block_size < UINT16_MAX, "Should only be 19 bytes");

	// Read the index from program code in memory
	auto reader = PhysicalMake(dos.psp(),
	                           256 + static_cast<uint16_t>(exec_block_size));

	auto writer = (HostPt)&index;

	for (; size > 0; size--) {
		*writer++ = mem_readb(reader++);
	}

	const PROGRAMS_Creator& creator = internal_progs.at(index);

	const auto new_program = creator();

	new_program->Run();

	return CBRET_NONE;
}

// Main functions used in all program

Program::Program()
{
	// Find the command line and setup the PSP
	psp = new DOS_PSP(dos.psp());

	// Scan environment for filename
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
	// Get arguments directly from the shell instead of the psp.
	// this is done in securemode: (as then the arguments to mount and
	// friends can only be given on the shell ( so no int 21 4b) Securemode
	// part is disabled as each of the internal command has already
	// protection for it. (and it breaks games like cdman) it is also done
	// for long arguments to as it is convient (as the total commandline can
	// be longer then 127 characters. imgmount with lot's of parameters
	// Length of arguments can be ~120. but switch when above 100 to be sure

	if (/*control->SecureMode() ||*/ cmd->GetNumArguments() > 100) {
		auto temp = new CommandLine(cmd->GetFileName(), full_arguments);
		delete cmd;
		cmd = temp;
	}

	// Clear so it gets even more save
	full_arguments.assign("");
}

bool Program::SuppressWriteOut(const std::string& format) const
{
	// Have we encountered an executable thus far?
	static bool encountered_executable = false;

	if (encountered_executable) {
		return false;
	}

	if (control->GetStartupVerbosity() >= StartupVerbosity::Low) {
		return false;
	}

	if (!control->cmdline->HasExecutableName()) {
		return false;
	}

	// Keep suppressing output until after we hit the first executable.
	encountered_executable = is_executable_filename(format);
	return true;
}

// TODO Only used by the unit tests, try to get rid of it later
void Program::WriteOut(const std::string& format, const char* arguments)
{
	if (SuppressWriteOut(format)) {
		return;
	}

	char buf[WriteOutBufSize];
	std::snprintf(buf, WriteOutBufSize, format.c_str(), arguments);

	CONSOLE_Write(buf);
}

void Program::WriteOut_NoParsing(const std::string& str)
{
	if (SuppressWriteOut(str)) {
		return;
	}

	CONSOLE_Write(str);
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
	void DisplayHelp();

	void HandleHelpCommand(const std::vector<std::string>& parameters);

	void WriteConfig(const std::string& name)
	{
		WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_WHICH"), name.c_str());

		if (!control->WriteConfig(name)) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"), name.c_str());
		}
		return;
	}

	bool CheckSecureMode()
	{
		if (control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return true;
		}
		return false;
	}
};

void CONFIG::DisplayHelp()
{
	MoreOutputStrings output(*this);
	output.AddString(MSG_Get("SHELL_CMD_CONFIG_HELP_LONG"));
	output.Display();
}

void CONFIG::HandleHelpCommand(const std::vector<std::string>& _parameters)
{
	auto parameters = _parameters;

	switch (parameters.size()) {
	case 0: DisplayHelp(); return;

	case 1: {
		if (!strcasecmp("sections", parameters[0].c_str())) {
			// List the sections
			MoreOutputStrings output(*this);
			output.AddString(MSG_Get("PROGRAM_CONFIG_HLP_SECTLIST"));

			for (const Section* sec : *control) {
				if (sec->IsActive()) {
					output.AddString("  - %s\n", sec->GetName());
				}
			}
			output.AddString("\n");
			output.Display();
			return;
		}

		// If it's a section, leave it as single param
		Section* sec = control->GetSection(parameters[0]);
		if (!sec || !sec->IsActive()) {
			// Could be a property
			sec = control->GetSectionFromProperty(parameters[0].c_str());
			if (!sec || !sec->IsActive()) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
				         parameters[0].c_str());
				return;
			}
			parameters.emplace(parameters.begin(), sec->GetName());
		}
		break;
	}

	case 2: {
		Section* sec = control->GetSection(parameters[0]);
		if (sec && !sec->IsActive()) {
			sec = nullptr;
		}

		Section* sec2 = control->GetSectionFromProperty(
		        parameters[1].c_str());

		if (!sec || !sec->IsActive()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
			         parameters[0].c_str());
			return;

		} else if (!sec2 || !sec2->IsActive() || sec != sec2) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
			         parameters[1].c_str());
			return;
		}
		break;
	}

	default: DisplayHelp(); return;
	}

	// If we have a single value in parameters, it's a section.
	// If we have two values, that's a section and a property.
	Section* sec = control->GetSection(parameters[0]);

	if (sec == nullptr || !sec->IsActive()) {
		WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
		         parameters[0].c_str());
		return;
	}

	auto psec = dynamic_cast<SectionProp*>(sec);

	// Special [autoexec] section handling (if has no properties like all
	// the other sections).
	if (psec == nullptr) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_CONFIG_HLP_AUTOEXEC"),
		                 MSG_Get("AUTOEXEC_CONFIGFILE_HELP").c_str());
		output.AddString("\n");
		output.Display();
		return;
	}

	if (parameters.size() == 1) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_CONFIG_HLP_SECTHLP"),
		                 parameters[0].c_str());

		auto i = 0;
		while (true) {
			// List the properties
			Property* p = psec->GetProperty(i++);
			if (p == nullptr) {
				break;
			}
			if (p->IsDeprecated()) {
				continue;
			}
			output.AddString("  - %s\n", p->propname.c_str());
		}
		output.AddString("\n");
		output.Display();

	} else {
		// parameters has more than 1 element
		MoreOutputStrings output(*this);

		// Find the property by its name
		auto i = 0;

		while (true) {
			Property* p = psec->GetProperty(i++);
			if (p == nullptr) {
				break;
			}

			if (!strcasecmp(p->propname.c_str(), parameters[1].c_str())) {
				// Found it; make the list of possible values
				std::string possible_values;
				std::vector<Value> values = p->GetValues();

				if (p->GetType() == Value::V_BOOL) {
					possible_values += "on, off";

				} else if (p->GetType() == Value::V_INT) {
					// Print min & max for integer values if
					// used
					auto pint = dynamic_cast<PropInt*>(p);
					assert(pint);

					if (pint &&
					    pint->GetMin() != pint->GetMax()) {
						std::ostringstream oss;
						oss << pint->GetMin();
						oss << "..";
						oss << pint->GetMax();
						possible_values += oss.str();
					}
				}

				for (size_t k = 0; k < values.size(); ++k) {
					if (values[k].ToString() == "%u") {
						possible_values += MSG_Get(
						        "PROGRAM_CONFIG_HLP_POSINT");
					} else {
						possible_values += values[k].ToString();
					}
					if ((k + 1) < values.size()) {
						possible_values += ", ";
					}
				}

				output.AddString(MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP"),
				                 p->propname.c_str(),
				                 sec->GetName());

				if (p->IsDeprecatedButAllowed()) {
					output.AddString(MSG_Get(
					        "PROGRAM_CONFIG_DEPRECATED_BUT_ALLOWED_WARNING"));
					output.AddString("\n");

				} else if (p->IsDeprecated()) {
					output.AddString(MSG_Get(
					        "PROGRAM_CONFIG_DEPRECATED_WARNING"));
					output.AddString("\n");
				}

				output.AddString(p->GetHelp());
				output.AddString("\n\n");

				auto write_last_newline = false;

				if (!p->IsDeprecated()) {
					if (!possible_values.empty()) {
						output.AddString(
						        MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP_POSSIBLE_VALUES"),
						        possible_values.c_str());
					}

					output.AddString(
					        MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP_DEFAULT_VALUE"),
					        p->GetDefaultValue().ToString().c_str());

					output.AddString(
					        MSG_Get("PROGRAM_CONFIG_HLP_PROPHLP_CURRENT_VALUE"),
					        p->GetValue().ToString().c_str());

					write_last_newline = true;
				}

				// Print 'changability'
				if (p->GetChange() ==
				    Property::Changeable::OnlyAtStart) {
					output.AddString("\n");
					output.AddString(MSG_Get(
					        "PROGRAM_CONFIG_HLP_NOCHANGE"));

					write_last_newline = true;
				}

				if (write_last_newline) {
					output.AddString("\n");
				}
			}
		}

		output.Display();
	}
}

void CONFIG::Run(void)
{
	// TODO use a map instead
	static const char* const AllCommandLineParamFlags[] = {
	        "-r", // Restart

	        "-wcd",       // WriteDefaultConfig
	        "-wc",        // WriteConfig1
	        "-writeconf", // WriteConfig2
	        "-l",         // ListConfig

	        "-h",    // ShowHelp1
	        "-help", // ShowHelp2
	        "-?",    // ShowHelp3

	        "-axclear", // ClearAutoexec
	        "-axadd",   // AppendLineToAutoexec
	        "-axtype",  // ShowAutoexec

	        "-avistart", // StartVideoCapture
	        "-avistop",  // StopVideoCapture

	        "-startmapper", // StartMapper

	        "-get", // GetProperty
	        "-set", // SetProperty

	        "-writelang", // WriteLanguageFile1
	        "-wl",        // WriteLanguageFile2

	        "-securemode", // EnableSecureMode
	        ""};

	enum Parameter {
		NoMatch  = 0,
		NoParams = 1, // fixed return values for GetParameterFromList

		Restart = 2,

		WriteDefaultConfig,
		WriteConfig1,
		WriteConfig2,
		ListConfig,

		ShowHelp1,
		ShowHelp2,
		ShowHelp3,

		ClearAutoexec,
		AppendLineToAutoexec,
		ShowAutoexec,

		StartVideoCapture,
		StopVideoCapture,

		StartMapper,
		GetProperty,
		SetProperty,

		WriteLanguageFile1,
		WriteLanguageFile2,

		EnableSecureMode
	} param_result = NoMatch;

	bool first = true;

	std::vector<std::string> parameters = {};

	// Loop through the passed parameters
	while (param_result != NoParams) {
		param_result = (enum Parameter)cmd->GetParameterFromList(
		        AllCommandLineParamFlags, parameters);

		switch (param_result) {
		case Restart:
			if (CheckSecureMode()) {
				return;
			}
			if (parameters.empty()) {
				DOSBOX_Restart();
			} else {
				std::vector<std::string> restart_params;

				restart_params.emplace_back(
				        control->cmdline->GetFileName());

				for (size_t i = 0; i < parameters.size(); i++) {
					restart_params.emplace_back(parameters[i]);
				}

				const auto remaining_args = cmd->GetArguments();
				restart_params.insert(restart_params.end(),
				                      remaining_args.begin(),
				                      remaining_args.end());

				DOSBOX_Restart(restart_params);
			}
			return;

		case ListConfig: {
			auto size = control->config_files.size();
			const std_fs::path config_path = get_config_dir();

			WriteOut(MSG_Get("PROGRAM_CONFIG_CONFDIR"),
			         DOSBOX_GetVersion(),
			         config_path.c_str());

			if (size == 0) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));

			} else {
				WriteOut(MSG_Get("PROGRAM_CONFIG_PRIMARY_CONF"),
				         control->config_files.front().c_str());

				if (size > 1) {
					WriteOut(MSG_Get(
					        "PROGRAM_CONFIG_ADDITIONAL_CONF"));

					for (size_t i = 1; i < size; ++i) {
						WriteOut("%s\n",
						         control->config_files[i]
						                 .c_str());
					}
				}
			}

			if (!control->startup_params.empty()) {
				std::string test;
				for (size_t k = 0;
				     k < control->startup_params.size();
				     ++k) {
					test += control->startup_params[k] + " ";
				}

				WriteOut(MSG_Get("PROGRAM_CONFIG_PRINT_STARTUP"),
				         test.c_str());
			}

			WriteOut("\n");
			break;
		}

		case WriteDefaultConfig: {
			if (CheckSecureMode()) {
				return;
			}
			if (!parameters.empty()) {
				WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
				return;
			}
			WriteConfig(get_primary_config_path().string());
			break;
		}

		case WriteConfig1:
		case WriteConfig2:
			if (CheckSecureMode()) {
				return;
			}
			if (parameters.size() > 1) {
				WriteOut(MSG_Get("SHELL_TOO_MANY_PARAMETERS"));
				return;
			}

			if (parameters.size() == 1) {
				// write config to startup directory
				WriteConfig(parameters[0]);
			} else {
				// -wc without parameter: write dosbox.conf to
				// startup directory
				if (control->config_files.size()) {
					WriteConfig("dosbox.conf");
				} else {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NOCONFIGFILE"));
				}
			}
			break;

		case NoParams:
			if (!first) {
				break;
			}
			[[fallthrough]];

		case NoMatch: DisplayHelp(); return;

		case ShowHelp1:
		case ShowHelp2:
		case ShowHelp3: HandleHelpCommand(parameters); return;

		case ClearAutoexec: {
			auto sec = dynamic_cast<AutoExecSection*>(
			        control->GetSection(std::string("autoexec")));
			if (!sec) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"));
				return;
			}
			sec->data.clear();
			break;
		}

		case AppendLineToAutoexec: {
			if (parameters.empty()) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_MISSINGPARAM"));
				return;
			}
			auto sec = dynamic_cast<AutoExecSection*>(
			        control->GetSection(std::string("autoexec")));
			if (!sec) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"));
				return;
			}
			for (const auto& param : parameters) {
				const auto line_utf8 = dos_to_utf8(
				        param, DosStringConvertMode::WithControlCodes);
				sec->HandleInputLine(line_utf8);
			}
			break;
		}

		case ShowAutoexec: {
			auto sec = dynamic_cast<AutoExecSection*>(
			        control->GetSection(std::string("autoexec")));

			if (!sec) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"));
				return;
			}

			const auto line_dos = utf8_to_dos(sec->data,
			                                  DosStringConvertMode::WithControlCodes,
			                                  UnicodeFallback::Box);

			MoreOutputStrings output(*this);
			output.AddString("\n%s\n\n", line_dos.c_str());
			output.Display();
			break;
		}

		case StartVideoCapture: CAPTURE_StartVideoCapture(); break;
		case StopVideoCapture: CAPTURE_StopVideoCapture(); break;
		case StartMapper:
			if (CheckSecureMode()) {
				return;
			}
			MAPPER_Run(false);
			break;

		case GetProperty: {
			// "section property"
			// "property"
			// "section"
			// "section" "property"
			if (parameters.empty()) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}

			std::string::size_type spcpos = parameters[0].find_first_of(' ');

			// split on the ' '
			if (spcpos != std::string::npos) {
				parameters.emplace(parameters.begin() + 1,
				                   parameters[0].substr(spcpos + 1));
				parameters[0].erase(spcpos);
			}

			switch (parameters.size()) {
			case 1: {
				// property/section only
				// is it a section?
				Section* sec = control->GetSection(parameters[0]);
				if (sec) {
					// list properties in section
					auto i = 0;
					auto psec = dynamic_cast<SectionProp*>(sec);

					if (psec == nullptr) {
						auto pline = dynamic_cast<AutoExecSection*>(
						        sec);
						assert(pline);

						if (pline) {
							WriteOut("%s",
							         pline->data.c_str());
						}
						break;
					}
					while (true) {
						// list the properties
						Property* p = psec->GetProperty(i++);
						if (p == nullptr) {
							break;
						}
						const auto val_dos = utf8_to_dos(
						        p->GetValue().ToString(),
						        DosStringConvertMode::NoSpecialCharacters,
						        UnicodeFallback::Simple);
						WriteOut("%s=%s\n",
						         p->propname.c_str(),
						         val_dos.c_str());
					}

				} else {
					// no: maybe it's a property?
					sec = control->GetSectionFromProperty(
					        parameters[0].c_str());
					if (!sec) {
						WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
						         parameters[0].c_str());
						return;
					}
					// it's a property name
					const auto val_dos = utf8_to_dos(
					        sec->GetPropertyValue(parameters[0]),
					        DosStringConvertMode::NoSpecialCharacters,
					        UnicodeFallback::Simple);

					WriteOut("%s", val_dos.c_str());
					DOS_PSP(psp->GetParent())
					        .SetEnvironmentValue("CONFIG",
					                             val_dos);
				}
				break;
			}

			case 2: {
				// section + property
				const char* sec_name  = parameters[0].c_str();
				const char* prop_name = parameters[1].c_str();
				const Section* sec = control->GetSection(sec_name);
				if (!sec) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_ERROR"),
					         sec_name);
					return;
				}
				const std::string val_utf8 = sec->GetPropertyValue(
				        prop_name);
				if (val_utf8 == NO_SUCH_PROPERTY) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
					         prop_name,
					         sec_name);
					return;
				}

				const auto val_dos = utf8_to_dos(
				        val_utf8,
				        DosStringConvertMode::NoSpecialCharacters,
				        UnicodeFallback::Simple);

				WriteOut("%s\n", val_dos.c_str());

				DOS_PSP(psp->GetParent()).SetEnvironmentValue("CONFIG", val_dos);
				break;
			}
			default:
				WriteOut(MSG_Get("PROGRAM_CONFIG_GET_SYNTAX"));
				return;
			}
			return;
		}

		case SetProperty: {
			if (parameters.empty()) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
				return;
			}

			// add rest of command
			std::string rest;
			if (cmd->GetStringRemain(rest)) {
				parameters.push_back(rest);
			}

			if (const auto warning_message = control->SetPropertyFromCli(
			            parameters);
			    !warning_message.empty()) {

				WriteOut(warning_message);

			} else {
				auto* section = dynamic_cast<SectionProp*>(
				        control->GetSection(parameters[0]));
				if (!section) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
					         parameters[0].c_str());
					return;
				}

				auto* property = section->GetProperty(parameters[1]);

				if (!property) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
					         parameters[1].c_str());
					return;
				}

				// Input has been parsed (param[0]=section,
				// [1]=property, [2]=value) now execute
				std::string value(parameters[2]);

				// Due to parsing there can be a = at the start
				// of value.
				while (value.size() && (value.at(0) == ' ' ||
				                        value.at(0) == '=')) {
					value.erase(0, 1);
				}

				for (size_t i = 3; i < parameters.size(); ++i) {
					value += (std::string(" ") + parameters[i]);
				}

				if (value.empty()) {
					WriteOut(MSG_Get("PROGRAM_CONFIG_SET_SYNTAX"));
					return;
				}

				// If the value can't be set at runtime then
				// queue it and let the user know
				if (property->GetChange() ==
				    Property::Changeable::OnlyAtStart) {

					WriteOut(MSG_Get("PROGRAM_CONFIG_NOT_CHANGEABLE"),
					         parameters[1].c_str());

					property->SetQueuedValue(value);
					return;
				}

				const auto input_line = parameters[1] + "=" + value;
				const auto line_utf8 = dos_to_utf8(
				        input_line,
				        DosStringConvertMode::NoSpecialCharacters);

				section->HandleInputLine(line_utf8);
				section->ExecuteUpdate(*property);
			}
			return;
		}

		case WriteLanguageFile1:
		case WriteLanguageFile2:
			// In secure mode don't allow a new languagefile to be
			// created Who knows which kind of file we would overwrite.
			if (CheckSecureMode()) {
				return;
			}

			if (parameters.size() < 1) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_MISSINGPARAM"));
				return;
			}

			if (!MSG_WriteToFile(parameters[0])) {
				WriteOut(MSG_Get("PROGRAM_CONFIG_FILE_ERROR"),
				         parameters[0].c_str());
				return;
			}
			break;

		case EnableSecureMode:
			// Code for switching to secure mode
			control->SwitchToSecureMode();
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_ON"));
			return;

		default: assert(false); break;
		}

		first = false;
	}
}

std::unique_ptr<Program> CONFIG_ProgramCreate()
{
	return ProgramCreate<CONFIG>();
}

void PROGRAMS_AddMessages()
{
	// List config
	MSG_Add("PROGRAM_CONFIG_NOCONFIGFILE", "No config file loaded\n");

	MSG_Add("PROGRAM_CONFIG_PRIMARY_CONF",
	        "[color=white]Primary config file:[reset]\n  %s\n");

	MSG_Add("PROGRAM_CONFIG_ADDITIONAL_CONF",
	        "\n[color=white]Additional config files:[reset]\n  ");

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
	        "  -help sections\n"
	        "  -h    sections\n"
	        "  -?    sections    [reset]list the names of all config sections\n"
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
	        "                    set the value of a config property"
	        "\n\n"
	        "  -securemode       enable secure mode");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP",
	        "[color=white]Description of the [color=light-green]'%s'[color=white] "
	        "setting in the [color=light-cyan][%s][color=white] section:[reset]\n\n");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP_POSSIBLE_VALUES",
	        "[color=white]Possible values:[reset]  %s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP_DEFAULT_VALUE",
	        "[color=white]Default value:[reset]    %s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_PROPHLP_CURRENT_VALUE",
	        "[color=white]Current value:[reset]    %s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_AUTOEXEC",
	        "[color=white]Description of the "
	        "[color=light-cyan][autoexec][color=white] section:[reset]\n\n"
	        "%s\n");

	MSG_Add("PROGRAM_CONFIG_HLP_NOCHANGE",
	        "[color=yellow]This setting cannot be changed at runtime.[reset]\n");

	MSG_Add("PROGRAM_CONFIG_HLP_POSINT", "positive integer");

	MSG_Add("PROGRAM_CONFIG_HLP_SECTHLP",
	        "[color=white]List of settings in the "
	        "[color=light-cyan][%s][color=white] section:[reset]\n");

	MSG_Add("PROGRAM_CONFIG_HLP_SECTLIST",
	        "[color=white]List of configuration sections:[reset]\n");

	MSG_Add("PROGRAM_CONFIG_SECURE_ON", "Secure mode enabled.\n");

	MSG_Add("PROGRAM_CONFIG_SECURE_DISALLOW",
	        "This operation is not permitted in secure mode.\n");

	MSG_Add("PROGRAM_CONFIG_SECTION_ERROR",
	        "Section [color=light-cyan][%s][reset] doesn't exist.\n");

	MSG_Add("PROGRAM_CONFIG_GET_SYNTAX",
	        "Usage: [color=light-green]config[reset] -get "
	        "[color=light-cyan][SECTION][reset] [color=white]PROPERTY[reset]\n");

	MSG_Add("PROGRAM_CONFIG_PRINT_STARTUP",
	        "\n[color=white]DOSBox was started with the following command "
	        "line arguments:[reset]\n  %s\n");

	MSG_Add("PROGRAM_CONFIG_MISSINGPARAM", "Missing parameter.\n");

	MSG_Add("PROGRAM_PATH_TOO_LONG",
	        "The path '%s' exceeds the DOS limit of %d characters.\n");

	MSG_Add("PROGRAM_EXECUTABLE_MISSING", "Executable file not found: '%s'\n");

	MSG_Add("CONJUNCTION_AND", "and");

	MSG_Add("PROGRAM_CONFIG_NOT_CHANGEABLE",
	        "[color=yellow]The '%s' setting can't be changed at runtime.[reset]\n"
	        "However, it will be applied on restart by running 'CONFIG -r' or via the\n"
	        "restart hotkey.\n");

	MSG_Add("PROGRAM_CONFIG_DEPRECATED_BUT_ALLOWED_WARNING",
	        "[color=light-red]This is a deprecated setting only kept for "
	        "compatibility with old configs.\n"
	        "Please use the suggested alternatives; support will be removed "
	        "in the future.[reset]\n");

	MSG_Add("PROGRAM_CONFIG_DEPRECATED_WARNING",
	        "[color=light-red]This setting is no longer available; "
	        "please use the suggested alternatives.[reset]\n");

	MSG_Add("PROGRAM_CONFIG_NO_PROPERTY",
	        "There is no property [color=light-green]'%s'[reset] in section "
	        "[color=light-cyan][%s][reset]\n");

	MSG_Add("PROGRAM_CONFIG_SET_SYNTAX",
	        "Usage: [color=light-green]config [reset]-set [color=light-cyan][SECTION][reset] "
	        "[color=white]PROPERTY[reset][=][color=white]VALUE[reset]\n");

	MSG_Add("PROGRAM_CONFIG_INVALID_SETTING",
	        "Invalid [color=light-green]'%s'[reset] setting: [color=white]'%s'[reset];\n"
	        "using [color=white]'%s'[reset]");

	MSG_Add("PROGRAM_CONFIG_INVALID_SETTING_WITH_DETAILS",
	        "Invalid [color=light-green]'%s'[reset] setting: [color=white]'%s'[reset].\n"
	        "%s; using [color=white]'%s'[reset]");

	MSG_Add("PROGRAM_CONFIG_DEPRECATED_SETTING_VALUE",
	        "Deprecated [color=light-green]'%s'[reset] setting: [color=white]'%s'[reset];\n"
	        "using [color=white]'%s'[reset]");

	MSG_Add("PROGRAM_CONFIG_INVALID_INTEGER_SETTING",
	        "Invalid [color=light-green]'%s'[reset] setting: [color=white]'%s'[reset];\n"
	        "must be an integer, using [color=white]'%s'[reset]");

	MSG_Add("PROGRAM_CONFIG_INVALID_INTEGER_SETTING_OUTSIDE_VALID_RANGE",
	        "Invalid [color=light-green]'%s'[reset] setting: [color=white]'%s'[reset];\n"
	        "must be between %s and %s, using [color=white]'%s'[reset]");

	MSG_Add("PROGRAM_CONFIG_NO_HELP",
	        "No help available for the setting [color=light-green]'%s'[reset].");

	MSG_Add("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND",
	        "No config section or setting exists with the name [color=light-green]'%s'[reset]\n");

	MSG_Add("PROGRAM_CONFIG_DEPRECATED_SETTING",
	        "Deprecated setting [color=light-green]'%s'[reset]");

	MSG_Add("PROGRAM_CONFIG_VALID_VALUES", "Possible values");

	MSG_Add("PROGRAM_CONFIG_DEPRECATED_VALUES", "Deprecated values");
}

void PROGRAMS_Init()
{
	// Set up a special callback to start virtual programs
	call_program = CALLBACK_Allocate();
	CALLBACK_Setup(call_program, &PROGRAMS_Handler, CB_RETF, "internal program");
}

void PROGRAMS_Destroy()
{
	internal_progs_comdata.clear();
	internal_progs.clear();
}
