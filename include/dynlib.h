/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_DYNLIB_H
#define DOSBOX_DYNLIB_H

#include "config.h"
#include "std_filesystem.h"

enum class DynLibResult
{
	Success,
	LibOpenErr,
	ResolveSymErr,
};

#ifdef WIN32

#include <windows.h>
#include <libloaderapi.h>

using dynlib_handle = HINSTANCE;

// Loads a dynamic-link library if it hasn't been opened yet, otherwise returns
// a reference to it.
inline dynlib_handle dynlib_open(const std_fs::path& path) noexcept
{
	return LoadLibraryA(path.string().c_str());
}

// Retrieves the address of an exported function of the dynamic-link library by
// name
inline void* dynlib_get_symbol(dynlib_handle lib, const char* name) noexcept
{
	// returns a FARPROC, which is just a void pointer
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
	return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
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
