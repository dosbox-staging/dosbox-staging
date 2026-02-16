// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/std_filesystem.h"

#include <fstream>
#include <string>
#include <vector>


class PoEntry {
public:
	// Flag indicating C-style format string
	static const std::string FlagCFormat;
	// Flag requesting the GUI editors not to wrap lines
	static const std::string FlagNoWrap;
	// Flag indicating the translator intervention is necessary
	static const std::string FlagFuzzy;

	// Metadata key for writing script
	static const std::string MetadataKeyScript;

	void ResetEntry();

	bool IsDosBoxMetadataEntry() const;
	bool IsGettextMetadataEntry() const;

	void SetEnglish(const std::string& value);
	std::string GetEnglish() const;

	void SetTranslated(const std::string& value);
	std::string GetTranslated() const;

	void SetLocation(const std::string& value);
	std::string GetLocation() const;

	void SetContext(const std::string& value);
	std::string GetContext() const;

	void AddFlag(const std::string& flag);
	bool HasFlag(const std::string& flag) const;

	void AddHelpLine(const std::string& line);

protected:
	PoEntry() = default;
	virtual ~PoEntry() = default;

	std::string location = {};
	std::string context  = {};

	std::vector<std::string> flags = {};

	std::string english    = {};
	std::string translated = {};

	std::vector<std::string> help = {};

private:
	PoEntry(const PoEntry&)            = delete;
	PoEntry& operator=(const PoEntry&) = delete;
};


class PoReader final : public PoEntry {
public:
	PoReader(const std_fs::path& file_path);

	bool IsFileOk() const
	{
		return is_file_opened && !in_file.bad();
	}
	bool IsEndOfFile() const
	{
		return !is_file_opened || in_file.eof();
	}

	bool IsFirstEntry() const
	{
		return entry_counter == 1;
	}

	bool ValidateGettextMetadata() const;

	std::string GetLanguageFromMetadata() const;

	void LogWarning(const std::string& error) const;

	bool ReadEntry();

private:
	PoReader() = delete;

	std::string ReadSingleLineString(const std::string& line) const;

	void ReadLocation(const std::string& line);

	void ReadFlags(const std::string& line);

	void LogWarning(const int line_number, const std::string& error) const;

	bool is_file_opened = false;
	int line_counter    = 0;
	int entry_counter   = 0;

	std::ifstream in_file = {};

	// Used for logging only
	const std::string file_name = {};
	int entry_start_line        = 0;
};


class PoWriter final : public PoEntry {
public:
	PoWriter(const std_fs::path& file_path);

	bool IsFileOk() const
	{
		return !out_file.bad();
	}

	bool WriteEntry();

	bool WriteEmptyLine();
	bool WriteCommentLine(const std::string& comment = {});

	bool WriteHeader(const std::string& language);

	bool WriteDosBoxMetadata(const std::string& key, const std::string& value,
	                         const std::vector<std::string>& help,
	                         const bool is_fuzzy = false);

private:
	PoWriter() = delete;

	void WriteMultiLineString(std::string& value);

	std::ofstream out_file = {};
};
