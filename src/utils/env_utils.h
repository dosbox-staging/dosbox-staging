// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ENV_UTILS_H
#define DOSBOX_ENV_UTILS_H

#include <string>
#include <vector>

#if defined(WIN32)
	inline constexpr char env_path_separator[] = ";";
#else
	inline constexpr char env_path_separator[] = ":";
#endif

namespace Env {
	inline constexpr int Overwrite   = 1;
	inline constexpr int NoOverwrite = 0;
}

// Get the environment variable value from the provided name, 
// if the variable exists. Returns an empty string if the 
// variable does not exist, or is empty
std::string get_env_var(std::string const& var_name);
std::string get_env_var(const char* var_name);
void set_env_var(const char* var_name, const char* value, int overwrite);

#endif // DOSBOX_ENV_UTILS_H