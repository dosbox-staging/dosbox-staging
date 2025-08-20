// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "misc/support.h"
#include "private/po_file_entry.h"
#include "utils/checks.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();


static constexpr std::string Quote = "\"";
static constexpr char QuoteChar    = '"';

static constexpr std::string MarkerExtractedComment  = "#. ";
static constexpr std::string MarkerTranslatorComment = "# ";
static constexpr std::string EmptyCommentLine        = "#";

static constexpr std::string MarkerLocation = "#: ";
static constexpr std::string MarkerFlag     = "#, ";

static constexpr std::string KeywordContext = "msgctxt";
static constexpr std::string KeywordId      = "msgid";
static constexpr std::string KeywordString  = "msgstr";


// ***************************************************************************
// PO file entry
// ***************************************************************************

void PoEntry::ResetEntry()
{
	location = {};
	context  = {};

	flags = {};

	english    = {};
	translated = {};

	help = {};
}

void PoEntry::WriteMultiLineString(std::ofstream& out_file, std::string& value)
{
	// Split the whole text into lines
	auto lines = split_with_empties(value, '\n');

	// Add end-of-line characters
	for (size_t idx = 0; idx + 1 < lines.size(); ++idx) {
		lines[idx] += "\n";
	}
	if (!lines.empty() && lines.back().empty()) {
		lines.pop_back();
	}

	// Only put meaningful data after keyword for single-line strings
	if (lines.empty() || lines.size() > 1) {
		out_file << Quote + Quote + "\n";
	}

	auto escape = [](const std::string& text) {
		std::string result = {};
		for (const auto character : text) {
			switch (character) {
			case '\n': result += "\\n";  break;
			case '\r': result += "\\r";  break;
			case '\t': result += "\\t";  break;
			case '\\': result += "\\\\"; break;
			case '"':  result += "\\\""; break;
			default:   result += character; break;
			}
		}

		return result;	
	};

	for (const auto& line : lines) {
		out_file << Quote + escape(line) + Quote + "\n";
	}
}

bool PoEntry::Write(std::ofstream& out_file)
{
	// Write entry help
	for (const auto& line : help) {
		out_file << MarkerExtractedComment + line + "\n";
	}

	// Write source code location
	if (!location.empty()) {
		out_file << MarkerLocation + location + "\n";
	}

	// Write flags
	const auto has_fuzzy_flag = HasFlag(FlagFuzzy);
	bool first_flag = true;
	for (const auto& flag : flags) {
		if (flag == FlagFuzzy) {
			continue;
		}
		if (first_flag) {
			out_file << MarkerFlag + flag;
			first_flag = false;
		} else {
			out_file << ", " + flag;
		}
	}
	if (!first_flag) {
		out_file << "\n";
	}
	// Always write fuzzy flag separately
	if (has_fuzzy_flag) {
		out_file << MarkerFlag + FlagFuzzy + "\n";		
	}

	// Write context
	if (!context.empty()) {
		out_file << KeywordContext + " ";
		WriteMultiLineString(out_file, context);
	}

	// Write English string
	out_file << KeywordId + " ";
	WriteMultiLineString(out_file, english);

	// Write translated string
	out_file << KeywordString + " ";
	WriteMultiLineString(out_file, translated);

	// Add empty line
	out_file << "\n";

	// Return I/O status
	return !out_file.fail();
}

void PoEntry::SetEnglish(const std::string& value)
{
	english = value;
}

std::string PoEntry::GetEnglish() const
{
	return english;
}

void PoEntry::SetTranslated(const std::string& value)
{
	translated = value;
}

std::string PoEntry::GetTranslated() const
{
	return translated;
}

void PoEntry::SetLocation(const std::string& value)
{
	location = value;
}

std::string PoEntry::GetLocation() const
{
	return location;
}

void PoEntry::SetContext(const std::string& value)
{
	context = value;
}

std::string PoEntry::GetContext() const
{
	return context;
}

void PoEntry::AddFlag(const std::string& flag)
{
	if (!HasFlag(flag)) {
		flags.emplace_back(flag);
	}
}

bool PoEntry::HasFlag(const std::string& flag) const
{
	return contains(flags, flag);
}

void PoEntry::AddHelpLine(const std::string& line)
{
	help.push_back(line);
}

// ***************************************************************************
// PO file reader
// ***************************************************************************

PoReader::PoReader(const std_fs::path& file_path) : PoEntry(file_path.string())
{
	if (file_path.empty()) {
		return;
	}

	if (!path_exists(file_path) || !is_readable(file_path)) {
		return;
	}

	in_file.open(file_path);
	is_file_opened = true;
}

void PoReader::LogWarning(const std::string& error) const
{
	LOG_WARNING("LOCALE: Translation file '%s' error in entry #%u "
		    "starting from line #%u, %s",
	            file_name.c_str(),
	            check_cast<unsigned int>(entry_counter),
	            check_cast<unsigned int>(entry_start_line),
	            error.c_str());	
}

void PoReader::LogWarning(const size_t line_number,
                         const std::string& error) const
{
	LOG_WARNING("LOCALE: Translation file '%s' error in line #%u, %s",
	            file_name.c_str(),
	            check_cast<unsigned int>(line_number),
	            error.c_str());	
}

bool PoReader::ReadEntry()
{
	ResetEntry();

	auto Warn = [&](const std::string& error) {
		LogWarning(line_counter, error);
	};

	enum class State {
		// Entry not found yet - reading empty lines, comments, etc.
		Initial,
		// Normal parser state, where a PO entry has been found
		Normal,
		// Reading a possibly multi-line string of given type
		ReadingContext,
		ReadingEnglish,
		ReadingTranslated
	};

	auto reader_state = State::Initial;

	bool found_location   = false;
	bool found_context    = false;
	bool found_english    = false;
	bool found_translated = false;

	std::string line = {};
	while (std::getline(in_file, line)) {
		++line_counter;
		trim(line);

		// Skip initial empty lines
		if (reader_state == State::Initial && line.empty()) {
			continue;
		}

		// Skip comments
		if (line.starts_with(MarkerTranslatorComment) ||
		    line.starts_with(MarkerExtractedComment) ||
		    line == EmptyCommentLine) {
			continue;
		}

		if (reader_state == State::Initial) {
			reader_state     = State::Normal;
			entry_start_line = line_counter;
			++entry_counter;
		}

		// Check for the end of PO entry
		if (line.empty()) {
			break;
		}

		// Read source code location
		if (line.starts_with(MarkerLocation)) {
			if (found_location) {
				Warn("string location is allowed "
				     "only once per entry");
			}
			found_location = true;
			reader_state = State::Normal;
			ReadLocation(line);
			continue;
		}

		// Read flags
		if (line.starts_with(MarkerFlag)) {
			reader_state = State::Normal;
			ReadFlags(line);
			continue;
		}

		// Read start of context / English / translated string
		if (line.starts_with(KeywordContext)) {
			if (found_context) {
				Warn("context is allowed only once per entry");
			}
			found_context = true;
			reader_state = State::ReadingContext;
			line = line.substr(KeywordContext.size());
		} else if (line.starts_with(KeywordId)) {
			if (found_english) {
				Warn("English message is allowed "
				     "only once per entry");
			}
			found_english = true;
			reader_state = State::ReadingEnglish;
			line = line.substr(KeywordId.size());
		} else if (line.starts_with(KeywordString)) {
			if (found_translated) {
				Warn("translated message is allowed "
				     "only once per entry");
			}
			found_translated = true;
			reader_state = State::ReadingTranslated;
			line = line.substr(KeywordString.size());
		}
		trim(line);

		// Read the string
		if (line.size() >= 2 &&
		    line.starts_with(QuoteChar) &&
		    line.ends_with(QuoteChar)) {
		    	switch (reader_state) {
		    	case State::ReadingContext:
		    		context += ReadSingleLineString(line);
		    		break;
		    	case State::ReadingEnglish:
		    		english += ReadSingleLineString(line);
		    		break;
		    	case State::ReadingTranslated:
		    		translated += ReadSingleLineString(line);
		    		break;
		    	default:
		    		Warn("unexpected string");
		    		break;
		    	}
		    	continue;
		}

		Warn("unrecognized content");
		reader_state = State::Normal;
	}

	// Return I/O status
	return !in_file.fail();
}

std::string PoReader::ReadSingleLineString(const std::string& line) const
{
	std::string result = {};

	auto Warn = [&](const std::string& error) {
		LogWarning(line_counter, error);
	};

	// Skip the first and last character of the string, these are quotes
	for (size_t idx = 1; idx + 1 < line.size(); ++idx) {
		if (line[idx] != '\\') {
			result += line[idx];
			continue;
		}

		// This is not a regular character - decode the escaping
		++idx;
		switch (line[idx]) {
		case 'n':  result += '\n'; break;	
		case 'r':  result += '\r'; break;
		case 't':  result += '\t'; break;
		case '\\': result += '\\'; break;
		case '"':  result += '"';  break;
		default:
			Warn(format_str("unsupported escaping character 0x%02x", line[idx]));
			result += '\\';
			result += line[idx];
			break;
		}
	}

	return result;
}

void PoReader::ReadLocation(const std::string& line)
{
	auto Warn = [&](const std::string& error) {
		LogWarning(line_counter, error);
	};

	auto content = line.substr(MarkerLocation.size());
	trim(content);

	if (content.empty()) {
		Warn("string location is empty");
		return;
	}

	SetLocation(content);
}

void PoReader::ReadFlags(const std::string& line)
{
	auto Warn = [&](const std::string& error) {
		LogWarning(line_counter, error);
	};

	auto content = line.substr(MarkerFlag.size());
	trim(content);

	if (content.empty()) {
		Warn("empty list of flags");
		return;
	}

	for (auto flag : split(content, ",")) {
		trim(flag);
		if (HasFlag(flag)) {
			Warn(format_str("duplicated flag '%s'", flag.c_str()));
		} else {
			AddFlag(flag);
		}
	}
}

// ***************************************************************************
// PO file writer
// ***************************************************************************

PoWriter::PoWriter(const std_fs::path& file_path) : PoEntry(file_path.string())
{
	// XXX
}
