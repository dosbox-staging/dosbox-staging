// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DYNLIB_H
#define DOSBOX_DYNLIB_H

#include "dosbox_config.h"
#include "logging.h"
#include "misc/std_filesystem.h"

enum class DynLibResult {
	Success,
	LibOpenErr,
	ResolveSymErr,
};

#ifdef WIN32

// clang-format off
// 'windows.h' must be included first, otherwise we'll get compilation errors.
// Defining WIN32_LEAN_AND_MEAN ensures that winsock.h is not included, which 
// conflicts with winsock2.h used by slirp
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <libloaderapi.h>
// clang-format on

using dynlib_handle = HINSTANCE;

inline std::string get_last_error_string()
{
	LPVOID buf = {};

	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
	                       FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL,
	               GetLastError(),
	               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	               (LPWSTR)&buf,
	               0,
	               NULL);

	auto wstr = std::wstring((LPWSTR)buf);
	LocalFree(buf);

	return std::string(wstr.begin(), wstr.end());
}

// Loads a dynamic-link library if it hasn't been opened yet, otherwise returns
// a reference to it.
inline dynlib_handle dynlib_open(const std_fs::path& path) noexcept
{
	auto result = LoadLibraryA(path.string().c_str());
	if (result == nullptr) {
		LOG_ERR("DYNLIB: Error opening '%s', details: %s",
		        path.string().c_str(),
		        get_last_error_string().c_str());
	}
	return result;
}

// Retrieves the address of an exported function of the dynamic-link library by
// name
inline void* dynlib_get_symbol(dynlib_handle lib, const char* name) noexcept
{
	// Returns a FARPROC, which is just a void pointer
	return reinterpret_cast<void*>(GetProcAddress(lib, name));
}

// Decrement the reference counter of the dynamic-link library. If it reaches
// zero, the library is unloaded from memory.
inline void dynlib_close(dynlib_handle lib) noexcept
{
	FreeLibrary(lib);
}

#else // macOS & Linux

#include <dlfcn.h>

using dynlib_handle = void*;

inline dynlib_handle dynlib_open(const std_fs::path& path) noexcept
{
	// RTLD_GLOBAL is the default on macOS which allows de-duplicating
	// symbols between different dynamic libraries instances. This can lead
	// to crashes when two dynamic libraries have different versions of the
	// same statically compiled in library as the symbols might not be
	// binary compatible. To fix this, RTLD_LOCAL must be used.
	auto result = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);

	if (result == nullptr) {
		LOG_ERR("DYNLIB: Error opening '%s', details: %s",
		        path.c_str(),
		        dlerror());
	}
	return result;
}

inline void* dynlib_get_symbol(dynlib_handle lib, const char* name) noexcept
{
	return dlsym(lib, name);
}

inline void dynlib_close(dynlib_handle lib) noexcept
{
	dlclose(lib);
}

#endif /// macOS & Linux

#endif // DOSBOX_DYNLIB_H
