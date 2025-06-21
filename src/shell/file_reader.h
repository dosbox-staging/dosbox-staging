// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FILE_READER_H
#define DOSBOX_FILE_READER_H

#include <optional>
#include <string>

#include "shell.h"

class FileReader final : public LineReader {
public:
	static std::unique_ptr<FileReader> GetFileReader(const std::string& file);

	void Reset() final;
	std::optional<std::string> Read() final;

	FileReader(const FileReader&)            = delete;
	FileReader& operator=(const FileReader&) = delete;
	FileReader(FileReader&&)                 = default;
	FileReader& operator=(FileReader&&)      = default;
	~FileReader() final                      = default;

private:
	explicit FileReader(std::string filename);

	std::string filename;
	uint32_t cursor;
};

#endif
