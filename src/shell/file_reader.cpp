/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include <cassert>
#include <utility>

#include "file_reader.h"

std::optional<FileReader> FileReader::GetFileReader(const std::string& filename)
{
	std::uint16_t handle = {};
	if (!DOS_OpenFile(filename.c_str(), (DOS_NOT_INHERIT | OPEN_READ), &handle)) {
		return {};
	}
	return FileReader(handle);
}

FileReader::FileReader(const uint16_t file_handle) : handle(file_handle) {}

FileReader::FileReader(FileReader&& other) noexcept
        : handle(std::exchange(other.handle, {}))
{}

FileReader& FileReader::operator=(FileReader&& other) noexcept
{
	if (this == &other) {
		return *this;
	}

	if (handle) {
		DOS_CloseFile(*handle);
	}
	handle = std::exchange(other.handle, {});
	return *this;
}

std::optional<uint8_t> FileReader::Read()
{
	if (!handle) {
		return {};
	}

	std::uint8_t data        = 0;
	std::uint16_t bytes_read = 1;

	const bool result = DOS_ReadFile(*handle, &data, &bytes_read);
	if (!result || bytes_read == 0) {
		return {};
	}

	return data;
}

void FileReader::Reset()
{
	if (!handle) {
		return;
	}
	std::uint32_t cursor = 0;
	DOS_SeekFile(*handle, &cursor, DOS_SEEK_SET);
}

FileReader::~FileReader()
{
	if (!handle) {
		return;
	}
	DOS_CloseFile(*handle);
}
