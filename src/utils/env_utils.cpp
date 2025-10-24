// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/env_utils.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <cstdlib>

std::string get_env_var(std::string const& var_name)
{
    return get_env_var(var_name.c_str());
}

// Get the environment variable value from the provided name, 
// if the variable exists. Returns an empty string if the 
// variable does not exist, or is empty.
// NOTE: getenv on POSIX systems IS NOT thread safe. Care
//       should be taken when setting environment variables
//       then getting them.
std::string get_env_var(const char* var_name)
{
    std::string env_var = {};
#ifdef _WIN32
    auto size = GetEnvironmentVariableA(var_name, nullptr, 0);
    if (size > 0) {
        // Note, 'size' includes the null terminator
        env_var.resize(size - 1);
        GetEnvironmentVariableA(var_name, env_var.data(), size);
    }
#else
    const char* env_var_c_str = getenv(var_name);
    if (env_var_c_str) {
        env_var = env_var_c_str;
    }
#endif
    return env_var;
}

// Set an environment variable using the provided name and value.
// NOTE: getenv on POSIX systems IS NOT thread safe. Care
//       should be taken when setting environment variables
//       then getting them.
void set_env_var(const char* var_name, const char* value, [[maybe_unused]] int overwrite)
{
#ifdef _WIN32
    SetEnvironmentVariableA(var_name, value);
#else
    setenv(var_name, value, overwrite);
#endif
}