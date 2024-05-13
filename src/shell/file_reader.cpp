/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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

std::unique_ptr<FileReader> FileReader::GetFileReader(const std::string& filename)
{
	auto fullname = DOS_Canonicalize(filename.c_str());

	std::uint16_t handle = {};
	if (!DOS_OpenFile(fullname.c_str(), (DOS_NOT_INHERIT | OPEN_READ), &handle)) {
		return {};
	}
	DOS_CloseFile(handle);
	return std::unique_ptr<FileReader>(new FileReader(std::move(fullname)));
}

FileReader::FileReader(std::string filename)
        : filename(std::move(filename)),
          cursor(0)
{}

std::optional<std::string> FileReader::Read()
{
	uint16_t entry = {};
	if (!DOS_OpenFile(filename.c_str(), (DOS_NOT_INHERIT | OPEN_READ), &entry)) {
		return {};
	}
	DOS_SeekFile(entry, &cursor, DOS_SEEK_SET);

	uint8_t data           = 0;
	uint16_t bytes_to_read = 1;
	std::string line       = {};

	while (data != '\n') {
		bool result = DOS_ReadFile(entry, &data, &bytes_to_read);
		if (!result || bytes_to_read == 0) {
			break;
		}
		line += static_cast<char>(data);
	}

	cursor = 0;
	DOS_SeekFile(entry, &cursor, DOS_SEEK_CUR);
	DOS_CloseFile(entry);

	if (line.empty()) {
		return {};
	}

	return line;
}

void FileReader::Reset()
{
	cursor = 0;
}
