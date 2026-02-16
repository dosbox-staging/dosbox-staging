// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config/setup.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include "config/config.h"
#include "misc/ansi_code_markup.h"
#include "misc/console.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

// Set by parseconfigfile so PropPath can use it to construct the realpath
std_fs::path current_config_dir;

static void write_property(const Property& prop, FILE* outfile)
{
	auto help_text = prop.GetHelpRaw();

	// Percentage signs are encoded as '%%' in the config descriptions
	// because they are sent through printf-like functions (e.g.,
	// WriteOut()). So we need to de-escape them before writing them into
	// the config.
	help_text = format_str(help_text);

	auto lines = split_with_empties(help_text, '\n');

	// Write help text
	for (auto& line : lines) {
		if (line.empty()) {
			fprintf(outfile, "#\n");
		} else {
			fprintf(outfile, "# %s\n", line.c_str());
		}
	}

	fprintf(outfile, "#\n");

	// Write 'setting = value' pair
	fprintf(outfile,
	        "%s = %s\n\n",
	        prop.propname.c_str(),
	        prop.GetValue().ToString().c_str());
}

static void write_section(SectionProp& sec, FILE* outfile)
{
	for (size_t i = 0; const auto prop = sec.GetProperty(i); ++i) {
		if (!prop->IsDeprecated()) {
			write_property(*prop, outfile);
		}
	}
}

bool Config::WriteConfig(const std_fs::path& path) const
{
	char temp[50];
	char helpline[256];

	FILE* outfile = fopen(path.string().c_str(), "w+t");
	if (!outfile) {
		return false;
	}

	// Print start of config file and add a return to improve readibility
	fprintf(outfile, MSG_GetTranslatedRaw("CONFIGFILE_INTRO").c_str(), DOSBOX_VERSION);
	fprintf(outfile, "\n");

	for (auto section = sections.cbegin(); section != sections.cend(); ++section) {
		// Print section header
		safe_strcpy(temp, (*section)->GetName());
		lowcase(temp);
		fprintf(outfile, "[%s]\n\n", temp);

		auto sec = dynamic_cast<SectionProp*>(*section);
		if (sec) {
			write_section(*sec, outfile);

		} else {
			upcase(temp);
			strcat(temp, "_CONFIGFILE_HELP");

			const auto _helpstr = MSG_GetTranslatedRaw(temp);

			const char* helpstr   = _helpstr.c_str();
			const char* linestart = helpstr;
			char* helpwrite       = helpline;

			while (*helpstr && static_cast<size_t>(helpstr - linestart) <
			                           sizeof(helpline)) {
				*helpwrite++ = *helpstr;

				if (*helpstr == '\n') {
					*helpwrite = 0;

					fprintf(outfile, "# %s", helpline);

					helpwrite = helpline;
					linestart = ++helpstr;
				} else {
					++helpstr;
				}
			}
		}

		// This will effectively only print the autoexec section
		// TODO Do this in a nicer way and get rid of PrintData()
		// altogether.
		(*section)->PrintData(outfile);

		fprintf(outfile, "\n");
	}

	fclose(outfile);
	return true;
}

SectionProp* Config::AddSection(const char* section_name)
{
	assertm(std::regex_match(section_name, std::regex{"[a-zA-Z0-9]+"}),
	        "Only letters and digits are allowed in section name");

	auto section_prop = new SectionProp(section_name);

	sections.push_back(section_prop);
	return section_prop;
}

AutoExecSection* Config::AddAutoexecSection()
{
	auto section = new AutoExecSection("autoexec");
	sections.push_back(section);

	return section;
}

// Move assignment operator
Config& Config::operator=(Config&& source) noexcept
{
	if (this == &source) {
		return *this;
	}

	// Move each member
	cmdline         = std::move(source.cmdline);
	sections        = std::move(source.sections);
	secure_mode     = std::move(source.secure_mode);
	startup_params  = std::move(source.startup_params);
	config_files    = std::move(source.config_files);

	loaded_config_paths_canonical = std::move(source.loaded_config_paths_canonical);

	overwritten_autoexec_section = std::move(source.overwritten_autoexec_section);
	overwritten_autoexec_conf = std::move(source.overwritten_autoexec_conf);

	// Hollow-out the source
	source.cmdline                       = {};
	source.overwritten_autoexec_section  = {};
	source.overwritten_autoexec_conf     = {};
	source.secure_mode                   = {};
	source.startup_params                = {};
	source.config_files                  = {};
	source.loaded_config_paths_canonical = {};

	return *this;
}

// Move constructor, leverages move by assignment
Config::Config(Config&& source) noexcept
{
	*this = std::move(source);
}

Config::~Config()
{
	for (auto cnt = sections.rbegin(); cnt != sections.rend(); ++cnt) {
		delete (*cnt);
	}
}

Section* Config::GetSection(const std::string_view section_name) const
{
	for (auto* el : sections) {
		if (iequals(el->GetName(), section_name)) {
			return el;
		}
	}
	return nullptr;
}

Section* Config::GetSectionFromProperty(const char* prop) const
{
	for (auto* el : sections) {
		if (el->GetPropertyValue(prop) != NO_SUCH_PROPERTY) {
			return el;
		}
	}
	return nullptr;
}

void Config::OverwriteAutoexec(const std::string& conf, const std::string& line)
{
	// If we're in a new config file, then record that filename and reset
	// the section
	if (overwritten_autoexec_conf != conf) {
		overwritten_autoexec_conf = conf;
		overwritten_autoexec_section.data.clear();
	}
	overwritten_autoexec_section.HandleInputLine(line);
}

const std::string& Config::GetOverwrittenAutoexecConf() const
{
	return overwritten_autoexec_conf;
}

const AutoExecSection& Config::GetOverwrittenAutoexecSection() const
{
	return overwritten_autoexec_section;
}

bool Config::ParseConfigFile(const std::string& type,
                             const std::string& config_file_name)
{
	std::error_code ec;
	const std_fs::path cfg_path = config_file_name;
	const auto canonical_path   = std_fs::canonical(cfg_path, ec);

	if (ec) {
		return false;
	}

	if (contains(loaded_config_paths_canonical, canonical_path)) {
		LOG_INFO("CONFIG: Skipping already loaded config file '%s'",
		         config_file_name.c_str());
		return true;
	}

	std::ifstream input_stream(canonical_path);
	if (!input_stream) {
		return false;
	}

	config_files.push_back(config_file_name);
	loaded_config_paths_canonical.push_back(canonical_path);

	// Get directory from config_file_name, used with relative paths.
	current_config_dir = canonical_path.parent_path();

	// If this is an autoexec section, the above takes care of the joining
	// while this handles the overwrriten mode. We need to be prepared for
	// either scenario to play out because we won't know the users final
	// preference until the very last configuration file is processed.

	std::string line = {};

	Section* current_section = nullptr;
	bool is_autoexec_section = false;
	bool is_autoexec_started = false;

	auto is_empty_line = [](const std::string& line) {
		return line.empty() || line[0] == '\0' || line[0] == '\n' ||
		       line[0] == '\r';
	};

	auto is_comment = [](const std::string& line) {
		return !line.empty() && (line[0] == '%' || line[0] == '#');
	};

	auto is_section_start = [](const std::string& line) {
		return !line.empty() && line[0] == '[';
	};

	auto handle_autoexec_line = [&]() {
		// Ignore all the empty lines until the meaningful [autoexec]
		// content starts
		if (!is_autoexec_started) {
			if (is_empty_line(line) || is_comment(line)) {
				return;
			}
			is_autoexec_started = true;
		}

		if (!is_comment(line)) {
			current_section->HandleInputLine(line);
			OverwriteAutoexec(config_file_name, line);
		}
	};

	while (getline(input_stream, line)) {
		trim(line);

		if (is_section_start(line)) {
			is_autoexec_section = false;
			is_autoexec_started = false;
		}

		// Special handling of [autoexec] section
		if (is_autoexec_section) {
			handle_autoexec_line();
			continue;
		}

		// Strip leading/trailing whitespace, skip unnecessary lines
		if (is_empty_line(line) || is_comment(line)) {
			continue;
		}

		if (is_section_start(line)) {
			// New section
			const auto bracket_pos = line.find(']');
			if (bracket_pos == std::string::npos) {
				continue;
			}
			line.erase(bracket_pos);
			const auto section_name = line.substr(1);
			if (const auto sec = GetSection(section_name); sec) {
				current_section = sec;
				is_autoexec_section = (section_name == "autoexec");
			}
		} else if (current_section) {
			current_section->HandleInputLine(line);
		}
	}

	// So internal changes don't use the path information
	current_config_dir.clear();

	LOG_INFO("CONFIG: Loaded %s config file '%s'",
	         type.c_str(),
	         config_file_name.c_str());

	return true;
}

// Applies queued configuration settings to CLI arguments. It replaces any
// existing settings with their latest values. For example, if "machine=value"
// was set multiple times, only the most recent value is preserved in the final
// CLI args.
void Config::ApplyQueuedValuesToCli(std::vector<std::string>& args) const
{
	for (const auto section : sections) {
		const auto properties = dynamic_cast<SectionProp*>(section);
		if (!properties) {
			continue;
		}

		for (const auto property : *properties) {
			const auto queued_value = property->GetQueuedValue();
			if (!queued_value) {
				continue;
			}

			constexpr auto set_prefix = "--set";

			const auto key_prefix = format_str("%s=",
			                                   property->propname.c_str());

			// Remove existing matches
			auto it = args.begin();
			while (it != args.end() && std::next(it) != args.end()) {
				const auto current = it;
				const auto next    = std::next(it);

				if (*current == set_prefix &&
				    next->starts_with(key_prefix)) {
					it = args.erase(current, std::next(next));
				} else {
					++it;
				}
			}

			// Add the new arguments with the queued value
			args.emplace_back(set_prefix);
			args.emplace_back(key_prefix + *queued_value);
		}
	}
}

StartupVerbosity Config::GetStartupVerbosity() const
{
	const Section* s = GetSection("dosbox");
	assert(s);
	const std::string user_choice = s->GetPropertyValue("startup_verbosity");

	if (user_choice == "high") {
		return StartupVerbosity::High;
	}
	if (user_choice == "low") {
		return StartupVerbosity::Low;
	}
	if (user_choice == "quiet") {
		return StartupVerbosity::Quiet;
	}
	if (user_choice == "auto") {
		return (cmdline->HasDirectory() || cmdline->HasExecutableName())
		             ? StartupVerbosity::Low
		             : StartupVerbosity::High;
	}

	NOTIFY_DisplayWarning(Notification::Source::Console,
	                      "CONFIG",
	                      "Invalid [color=light-green]'startup_verbosity'[reset] setting: "
	                      "[color=white]'%s'[reset], using [color=white]'high'[reset]",
	                      user_choice.c_str());

	return StartupVerbosity::High;
}

const std::string& Config::GetArgumentLanguage()
{
	return arguments.lang;
}

// Parse the user's configuration files starting with the primary, then custom
// -conf's, and finally the local dosbox.conf
void Config::ParseConfigFiles(const std_fs::path& config_dir)
{
	// First: parse the user's primary 'dosbox-staging.conf' config file
	const bool load_primary_config = !arguments.noprimaryconf;

	if (load_primary_config) {
		const auto config_path = config_dir / get_primary_config_name();
		ParseConfigFile("primary", config_path.string());
	}

	// Second: parse the local 'dosbox.conf', if present
	const bool load_local_config = !arguments.nolocalconf;

	if (load_local_config) {
		ParseConfigFile("local", "dosbox.conf");
	}

	// Finally: layer on additional config files specified with the '-conf'
	// switch
	for (const auto& conf_file : arguments.conf) {
		if (!ParseConfigFile("custom", conf_file)) {
			// Try to load it from the user directory
			const auto cfg = config_dir / conf_file;
			if (!ParseConfigFile("custom", cfg.string())) {
				LOG_WARNING("CONFIG: Can't open custom config file '%s'",
				            conf_file.c_str());
			}
		}
	}

	// Once we've parsed all the potential config files, we've down our best
	// to discover the user's desired language. At this point, we can now
	// initialise the messaging system which honours the language and loads
	// those messages.
	MSG_LoadMessages();
}

std::string Config::SetProperty(std::vector<std::string>& pvars)
{
	// Attempt to split off the first word
	std::string::size_type spcpos = pvars[0].find_first_of(' ');
	std::string::size_type equpos = pvars[0].find_first_of('=');

	if ((equpos != std::string::npos) &&
	    ((spcpos == std::string::npos) || (equpos < spcpos))) {

		// If we have a '=' possibly before a ' ' split on the =
		pvars.emplace(pvars.begin() + 1, pvars[0].substr(equpos + 1));
		pvars[0].erase(equpos);

		// As we had a = the first thing must be a property now
		Section* sec = GetSectionFromProperty(pvars[0].c_str());

		if (sec) {
			pvars.emplace(pvars.begin(), std::string(sec->GetName()));
		} else {
			return format_str(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
			                  pvars[0].c_str());
		}
		// Order in the vector should be ok now

	} else {
		if ((spcpos != std::string::npos) &&
		    ((equpos == std::string::npos) || (spcpos < equpos))) {

			// ' ' before a possible '=', split on the ' '
			pvars.emplace(pvars.begin() + 1,
			              pvars[0].substr(spcpos + 1));
			pvars[0].erase(spcpos);
		}

		// Check if the first parameter is a section or property
		Section* sec = GetSection(pvars[0]);

		if (!sec) {
			// Not a section: little duplicate from above
			Section* secprop = GetSectionFromProperty(pvars[0].c_str());

			if (secprop) {
				pvars.emplace(pvars.begin(),
				              std::string(secprop->GetName()));
			} else {
				return format_str(MSG_Get("PROGRAM_CONFIG_SECTION_OR_SETTING_NOT_FOUND"),
				                  pvars[0].c_str());
			}
		} else {
			// First of pvars is most likely a section, but could
			// still be gus have a look at the second parameter
			if (pvars.size() < 2) {
				return MSG_Get("PROGRAM_CONFIG_SET_SYNTAX");
			}

			std::string::size_type spcpos2 = pvars[1].find_first_of(' ');
			std::string::size_type equpos2 = pvars[1].find_first_of('=');

			if ((equpos2 != std::string::npos) &&
			    ((spcpos2 == std::string::npos) || (equpos2 < spcpos2))) {
				// Split on the =
				pvars.emplace(pvars.begin() + 2,
				              pvars[1].substr(equpos2 + 1));

				pvars[1].erase(equpos2);

			} else if ((spcpos2 != std::string::npos) &&
			           ((equpos2 == std::string::npos) ||
			            (spcpos2 < equpos2))) {
				// Split on the ' '
				pvars.emplace(pvars.begin() + 2,
				              pvars[1].substr(spcpos2 + 1));

				pvars[1].erase(spcpos2);
			}

			// Is this a property?
			Section* sec2 = GetSectionFromProperty(pvars[1].c_str());

			if (!sec2) {
				// Not a property
				Section* sec3 = GetSectionFromProperty(
				        pvars[0].c_str());
				if (sec3) {
					// Section and property name are identical
					pvars.emplace(pvars.begin(), pvars[0]);
				}
				// else has been checked above already
			}
		}
	}

	if (pvars.size() < 3) {
		return MSG_Get("PROGRAM_CONFIG_SET_SYNTAX");
	}

	// Check if the property actually exists in the section
	const auto sec2 = GetSectionFromProperty(pvars[1].c_str());
	if (!sec2) {
		return format_str(MSG_Get("PROGRAM_CONFIG_NO_PROPERTY"),
		                  pvars[1].c_str(),
		                  pvars[0].c_str());
	}

	return "";
}

void Config::ParseArguments()
{
	arguments.printconf = cmdline->FindRemoveBoolArgument("printconf");
	arguments.noprimaryconf = cmdline->FindRemoveBoolArgument("noprimaryconf");
	arguments.nolocalconf = cmdline->FindRemoveBoolArgument("nolocalconf");
	arguments.fullscreen  = cmdline->FindRemoveBoolArgument("fullscreen");
	arguments.list_countries = cmdline->FindRemoveBoolArgument("list-countries");
	arguments.list_layouts = cmdline->FindRemoveBoolArgument("list-layouts");
	arguments.list_code_pages = cmdline->FindRemoveBoolArgument("list-code-pages");
	arguments.list_shaders = cmdline->FindRemoveBoolArgument("list-shaders");
	arguments.noconsole   = cmdline->FindRemoveBoolArgument("noconsole");
	arguments.startmapper = cmdline->FindRemoveBoolArgument("startmapper");
	arguments.exit        = cmdline->FindRemoveBoolArgument("exit");
	arguments.securemode  = cmdline->FindRemoveBoolArgument("securemode");
	arguments.noautoexec  = cmdline->FindRemoveBoolArgument("noautoexec");

	arguments.eraseconf = cmdline->FindRemoveBoolArgument("eraseconf") ||
	                      cmdline->FindRemoveBoolArgument("resetconf");
	arguments.erasemapper = cmdline->FindRemoveBoolArgument("erasemapper") ||
	                        cmdline->FindRemoveBoolArgument("resetmapper");

	arguments.version = cmdline->FindRemoveBoolArgument("version", 'V');
	arguments.help    = (cmdline->FindRemoveBoolArgument("help", 'h') ||
                          cmdline->FindRemoveBoolArgument("help", '?'));

	arguments.working_dir = cmdline->FindRemoveStringArgument("working-dir");
	arguments.lang    = cmdline->FindRemoveStringArgument("lang");
	arguments.machine = cmdline->FindRemoveStringArgument("machine");

	arguments.socket   = cmdline->FindRemoveIntArgument("socket");
	arguments.wait_pid = cmdline->FindRemoveIntArgument("waitpid");

	arguments.conf = cmdline->FindRemoveVectorArgument("conf");
	arguments.set  = cmdline->FindRemoveVectorArgument("set");

	arguments.editconf = cmdline->FindRemoveOptionalArgument("editconf");
}
