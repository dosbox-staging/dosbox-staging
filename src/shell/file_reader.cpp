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

#include "file_reader.h"

std::optional<std::unique_ptr<FileReader>> FileReader::GetFileReader(std::string_view file)
{
	auto reader = std::make_unique<FileReader>(file, PrivateOnly());
	if (!reader->valid) {
		return std::nullopt;
	}
	return reader;
}

FileReader::FileReader(std::string_view file, [[maybe_unused]] PrivateOnly key)
        : filename(file),
          valid(DOS_OpenFile(filename.c_str(), (DOS_NOT_INHERIT | OPEN_READ), &handle))
{}

std::optional<uint8_t> FileReader::Read()
{
	std::uint8_t data        = 0;
	std::uint16_t bytes_read = 1;

	bool result = DOS_ReadFile(handle, &data, &bytes_read);
	if (!result || bytes_read == 0) {
		return std::nullopt;
	}

	return data;
}

void FileReader::Reset()
{
	std::uint32_t cursor = 0;
	DOS_SeekFile(handle, &cursor, DOS_SEEK_SET);
}

FileReader::~FileReader()
{
	DOS_CloseFile(handle);
}
