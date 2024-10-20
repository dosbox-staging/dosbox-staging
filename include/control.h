/*
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
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
	bool list_countries;
	bool list_layouts;
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
		startup_params = cmdline->GetArguments();
		startup_params.insert(startup_params.begin(),
		                      cmdline->GetFileName());

		ParseArguments();
	}

	Config() = default;
	Config(Config&& source) noexcept;            // move constructor
	Config(const Config&) = delete;              // block construct-by-value
	Config& operator=(Config&& source) noexcept; // move assignment
	Config& operator=(const Config&) = delete;   // block assign-by-value

	~Config();

	Section_prop* AddEarlySectionProp(const char* name, SectionFunction func,
	                                  bool changeable_at_runtime = false);

	Section_line* AddSection_line(const char* section_name, SectionFunction func);

	Section_prop* AddInactiveSectionProp(const char* section_name);
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

	Section* GetSection(const std::string_view section_name) const;
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
	void ParseConfigFiles(const std_fs::path& config_path);
	const char* SetProp(std::vector<std::string>& pvars);

	const std::string& GetArgumentLanguage();

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

using ConfigPtr = std::unique_ptr<Config>;
extern ConfigPtr control;

void restart_dosbox(std::vector<std::string> &parameters = control->startup_params);

#endif
