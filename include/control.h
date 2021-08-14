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

enum class Verbosity : int8_t {
	//                  Splash | Welcome | Early Stdout |
	High = 4,       //   yes   |   yes   |    yes       |
	Medium = 3,     //   no    |   yes   |    yes       |
	Low = 2,        //   no    |   no    |    yes       |
	SplashOnly = 1, //   yes   |   no    |    no        |
	Quiet = 0       //   no    |   no    |    no        |
};

class Config {
public:
	CommandLine * cmdline;
private:
	std::deque<Section*> sectionlist;
	void (* _start_function)(void);
	bool secure_mode; //Sandbox mode
public:
	std::vector<std::string> startup_params;
	std::vector<std::string> configfiles;

	Config(CommandLine *cmd)
		: cmdline(cmd),
		  sectionlist{},
		  _start_function(nullptr),
		  secure_mode(false),
		  startup_params{},
		  configfiles{}
	{
		assert(cmdline);
		startup_params.push_back(cmdline->GetFileName());
		cmdline->FillVector(startup_params);
	}

	Config(const Config&) = delete; // prevent copying
	Config& operator=(const Config&) = delete; // prevent assignment

	~Config();

	Section_prop *AddEarlySectionProp(const char *name,
	                                  SectionFunction func,
	                                  bool changeable_at_runtime = false);

	Section_line *AddSection_line(const char *section_name, SectionFunction func);

	Section_prop *AddSection_prop(const char *section_name,
	                              SectionFunction func,
	                              bool changeable_at_runtime = false);

	auto begin() { return sectionlist.begin(); }
	auto end() { return sectionlist.end(); }

	Section *GetSection(const std::string &section_name) const;
	Section *GetSectionFromProperty(const char *prop) const;

	void SetStartUp(void (*_function)(void));
	void Init() const;
	void ShutDown();
	void StartUp();
	bool PrintConfig(const std::string &filename) const;
	bool ParseConfigFile(char const * const configfilename);
	void ParseEnv();
	bool SecureMode() const { return secure_mode; }
	void SwitchToSecureMode() { secure_mode = true; }//can't be undone
	Verbosity GetStartupVerbosity() const;
};

#endif
