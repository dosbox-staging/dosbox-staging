// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_H
#define DOSBOX_CONFIG_H

#include "dosbox.h"

#include <cassert>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "dos/programs.h"
#include "config/setup.h"
#include "misc/std_filesystem.h"

// clang-format off
enum class StartupVerbosity {
	//        Welcome | Early Stdout |
	Quiet, //   no    |     no       |
	Low,   //   no    |     yes      |
	High,  //   yes   |     yes      |
};
// clang-format on

struct CommandLineArguments {
	bool printconf;
	bool noprimaryconf;
	bool nolocalconf;
	bool fullscreen;
	bool list_countries;
	bool list_layouts;
	bool list_code_pages;
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
	std::optional<int> wait_pid;
};

class Config {
public:
	CommandLine* cmdline = nullptr;

	CommandLineArguments arguments = {};

private:
	std::deque<Section*> sectionlist          = {};
	SectionLine overwritten_autoexec_section = {};
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

	SectionProp* AddEarlySectionProp(const char* name, SectionFunction func,
	                                  bool changeable_at_runtime = false);

	SectionLine* AddSectionLine(const char* section_name, SectionFunction func);

	SectionProp* AddInactiveSectionProp(const char* section_name);
	SectionProp* AddSectionProp(const char* section_name, SectionFunction func,
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
	const SectionLine& GetOverwrittenAutoexecSection() const;
	const std::string& GetOverwrittenAutoexecConf() const;

	void ApplyQueuedValuesToCli(std::vector<std::string>& args) const;

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

	StartupVerbosity GetStartupVerbosity() const;
};

using ConfigPtr = std::unique_ptr<Config>;
extern ConfigPtr control;

void DOSBOX_Restart(std::vector<std::string> &parameters = control->startup_params);

#endif // DOSBOX_CONFIG_H
