// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/std_filesystem.h"

#include <fstream>
#include <string>
#include <vector>


class PoEntry
{
public:
	PoEntry() {}

	virtual ~PoEntry() = default;

	// XXX 

	PoEntry(const std::string& file_name) : file_name(file_name) {} // XXX delete this

	// Flag indicating C-style format string
	static constexpr std::string FlagCFormat = "c-format";
	// Flag requesting the GUI editors not to wrap lines
	static constexpr std::string FlagNoWrap  = "no-wrap";
	// Flag indicating the translator intervention is necessary
	static constexpr std::string FlagFuzzy   = "fuzzy";

	void ResetEntry();

	bool Write(std::ofstream& out_file);

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

	std::string location = {};
	std::string context  = {};

	std::vector<std::string> flags = {};

	std::string english    = {};
	std::string translated = {};

	std::vector<std::string> help = {};

	// Used for logging only
	const std::string file_name = {}; // XXX move this to reader class

private:
	PoEntry(const PoEntry&)            = delete;
	PoEntry& operator=(const PoEntry&) = delete;

	void WriteMultiLineString(std::ofstream& out_file, std::string& value);
};


class PoReader final : public PoEntry
{
public:
	PoReader(const std_fs::path& file_path);

	bool IsFileOk() const { return is_file_opened && !in_file.bad(); }
	bool IsEndOfFile() const { return !is_file_opened || in_file.eof(); }

	bool IsFirstEntry() const { return entry_counter < 2; }

	void LogWarning(const std::string& error) const;

	bool ReadEntry();

private:
	PoReader() = delete;

	std::string ReadSingleLineString(const std::string& line) const;

	void ReadLocation(const std::string& line);

	void ReadFlags(const std::string& line);

	void LogWarning(const size_t line_number,
	                const std::string& error) const;

	bool is_file_opened  = false;
	size_t line_counter  = -1;
	size_t entry_counter = 0;

	std::ifstream in_file = {};


	// Used for logging only
	size_t entry_start_line = 0;
};


class PoWriter final : public PoEntry
{
public:
	PoWriter(const std_fs::path& file_path);

	// XXX implement, use it

private:
	PoWriter() = delete;
};
