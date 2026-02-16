// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
		// We should also stop at EOF (0x1a) character
		if (!DOS_ReadFile(entry, &data, &bytes_to_read) ||
		    bytes_to_read == 0 || data == 0x1a) {
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
