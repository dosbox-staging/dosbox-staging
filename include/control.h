/*
 *  Copyright (C) 2019-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_CONTROL_H
#define DOSBOX_CONTROL_H

#include "dosbox.h"

#include <cassert>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "programs.h"
#include "setup.h"
#include "std_filesystem.h"

enum class Verbosity : int8_t {
	//                Welcome | Early Stdout |
	Quiet,         //   no    |    no        |
	InstantLaunch, //   no    |    yes       |
	Low,           //   no    |    yes       |
	High,          //   yes   |    yes       |
};

struct CommandLineArguments {
	bool printconf;
	bool noprimaryconf;
	bool nolocalconf;
	bool fullscreen;
	bool list_glshaders;
	bool version;
	bool help;
	bool eraseconf;
	bool erasemapper;
	bool noconsole;
	bool startmapper;
	bool exit;
	bool securemode;
	bool noautoexec;
	std::string working_dir;
	std::string lang;
	std::string machine;
	std::vector<std::string> conf;
	std::vector<std::string> set;
	std::optional<std::vector<std::string>> editconf;
	std::optional<int> socket;
};

class Config {
public:
	CommandLine* cmdline = nullptr;

	CommandLineArguments arguments = {};

private:
	std::deque<Section*> sectionlist          = {};
	Section_line overwritten_autoexec_section = {};
	std::string overwritten_autoexec_conf     = {};

	void (*_start_function)(void) = nullptr;

	bool secure_mode = false;

	void ParseArguments();

public:
	std::vector<std::string> startup_params        = {};
	std::vector<std::string> configfiles           = {};
	std::vector<std_fs::path> configFilesCanonical = {};

	Config(CommandLine* cmd)
	        : cmdline(cmd),
	          overwritten_autoexec_section("overwritten-autoexec")
	{
		assert(cmdline);
		startup_params.push_back(cmdline->GetFileName());
		cmdline->FillVector(startup_params);
		ParseArguments();
	}

	Config() = default;
	Config(Config&& source) noexcept;            // move constructor
	Config(const Config&) = delete;              // block construct-by-value
	Config& operator=(Config&& source) noexcept; // move assignment
	Config& operator=(const Config&) = delete;   // block assign-by-value

	~Config();

	// Add a conf section with its supporting modules. Athough some conf
	// sections only have a single module such as the audio devices, others
	// might have many modules.
	
	// For example, the "dosbox" section has supporting BIOS, MEMORY, PAGING,
	// modules that need be aware of the "machine", and "memsize" setting to
	// configure themselves appropriately.
	//
	// When the given conf section is changed at runtime, all the modules in the
	// section will receive the updated `Section*` settings for them to decide
	// how they should handle the new settings. Some modules might simply ignore
	// it while others might simply make a runtime adjustment, and even others
	// might destroy themselves.
	//
	template <typename... ConfigureFunctions>
	Section_prop* AddSection(const char* section_name,
	                         ConfigureFunctions... module_config_functions)
	{
		const auto section = new Section_prop(section_name);
		(section->AddModule(module_config_functions), ...);
		sectionlist.push_back(section);
		return section;
	}

	template <typename... ConfigureFunctions>
	Section_line* AddLineSection(const char* section_name,
	                             ConfigureFunctions... module_config_functions)
	{
		const auto section = new Section_line(section_name);
		(section->AddModule(module_config_functions), ...);
		sectionlist.push_back(section);
		return section;
	}

	Section_prop* AddEarlySectionProp(const char* name, SectionFunction func,
	                                  bool changeable_at_runtime = false);

	Section_line* AddSection_line(const char* section_name, SectionFunction func);

	Section_prop* AddSection_prop(const char* section_name, SectionFunction func,
	                              bool changeable_at_runtime = false);

	auto begin()
	{
		return sectionlist.begin();
	}
	auto end()
	{
		return sectionlist.end();
	}

	Section* GetSection(const std::string& section_name) const;
	Section* GetSectionFromProperty(const char* prop) const;

	void OverwriteAutoexec(const std::string& conf, const std::string& line);
	const Section_line& GetOverwrittenAutoexecSection() const;
	const std::string& GetOverwrittenAutoexecConf() const;

	void SetStartUp(void (*_function)(void));
	void Init() const;
	void ShutDown();
	void StartUp();

	bool WriteConfig(const std_fs::path& path) const;
	bool ParseConfigFile(const std::string& type,
	                     const std::string& config_file_name);

	void ParseEnv();

	bool SecureMode() const
	{
		return secure_mode;
	}

	void SwitchToSecureMode()
	{
		secure_mode = true;
	}

	Verbosity GetStartupVerbosity() const;
};

#endif
