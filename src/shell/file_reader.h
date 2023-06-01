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

#ifndef DOSBOX_FILE_READER_H
#define DOSBOX_FILE_READER_H

#include <optional>
#include <string>

#include "shell.h"

class FileReader final : public ByteReader {
private:
	// Exists to effectively make the FileReader constructor private
	// It still needs to be public for internal use with std::make_unique
	struct PrivateOnly {
		explicit PrivateOnly() = default;
	};

public:
	[[nodiscard]] static std::optional<std::unique_ptr<FileReader>> GetFileReader(
	        std::string_view file);

	void Reset() final;
	[[nodiscard]] std::optional<char> Read() final;

	FileReader(std::string_view filename, PrivateOnly key);
	~FileReader() final;

	FileReader(const FileReader&)            = delete;
	FileReader& operator=(const FileReader&) = delete;
	FileReader(FileReader&&)                 = delete;
	FileReader& operator=(FileReader&&)      = delete;

private:
	std::string filename = {};
	uint16_t handle      = 0;
	bool valid;
};

#endif
