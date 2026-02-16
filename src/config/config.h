// SPDX-FileCopyrightText:  2019-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_H
#define DOSBOX_CONFIG_H

#include <cassert>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "config/setup.h"
#include "shell/command_line.h"
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
	bool list_shaders;
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
	std::deque<Section*> sections            = {};
	AutoExecSection overwritten_autoexec_section = {};
	std::string overwritten_autoexec_conf    = {};

	bool secure_mode = false;

	void ParseArguments();

public:
	std::vector<std::string> startup_params                 = {};
	std::vector<std::string> config_files                   = {};
	std::vector<std_fs::path> loaded_config_paths_canonical = {};

	Config(CommandLine* cmd)
	        : cmdline(cmd),
	          overwritten_autoexec_section("overwritten-autoexec")
	{
		assert(cmdline);
		startup_params = cmdline->GetArguments();
		startup_params.emplace(startup_params.begin(),
		                       cmdline->GetFileName());

		ParseArguments();
	}

	Config() = default;

	// move constructor
	Config(Config&& source) noexcept;

	// prevent copying
	Config(const Config&) = delete;

	// prevent assignment assignment
	Config& operator=(Config&& source) noexcept;
	Config& operator=(const Config&) = delete;

	~Config();

	SectionProp* AddSection(const char* section_name);
	AutoExecSection* AddAutoexecSection();

	auto begin()
	{
		return sections.begin();
	}
	auto end()
	{
		return sections.end();
	}

	Section* GetSection(const std::string_view section_name) const;
	Section* GetSectionFromProperty(const char* prop) const;

	void OverwriteAutoexec(const std::string& conf, const std::string& line);
	const AutoExecSection& GetOverwrittenAutoexecSection() const;
	const std::string& GetOverwrittenAutoexecConf() const;

	void ApplyQueuedValuesToCli(std::vector<std::string>& args) const;

	void Init() const;

	bool WriteConfig(const std_fs::path& path) const;
	bool ParseConfigFile(const std::string& type,
	                     const std::string& config_file_name);

	void ParseConfigFiles(const std_fs::path& config_path);

	std::string SetProperty(std::vector<std::string>& pvars);

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

extern std_fs::path current_config_dir;

#endif // DOSBOX_CONFIG_H
