// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "misc/support.h"
#include "private/messages_po_entry.h"
#include "utils/checks.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

static const std::string Quote  = "\"";
static constexpr char QuoteChar = '"';

static const std::string MarkerExtractedComment  = "#. ";
static const std::string MarkerTranslatorComment = "# ";
static const std::string EmptyCommentLine        = "#";

static const std::string MarkerLocation = "#: ";
static const std::string MarkerFlag     = "#, ";

static const std::string KeywordContext = "msgctxt";
static const std::string KeywordId      = "msgid";
static const std::string KeywordString  = "msgstr";

// A generic key to identify DOSBox Staging metadata
static const std::string MetadataKey = "#METADATA";

// ***************************************************************************
// PO file entry
// ***************************************************************************

const std::string PoEntry::FlagCFormat = "c-format";
const std::string PoEntry::FlagNoWrap  = "no-wrap";
const std::string PoEntry::FlagFuzzy   = "fuzzy";

const std::string PoEntry::MetadataKeyScript = "#SCRIPT";

void PoEntry::ResetEntry()
{
	location = {};
	context  = {};

	flags = {};

	english    = {};
	translated = {};

	help = {};
}

bool PoEntry::IsDosBoxMetadataEntry() const
{
	return GetLocation() == MetadataKey && GetContext() == MetadataKey;
}

bool PoEntry::IsGettextMetadataEntry() const
{
	return GetEnglish().empty() && !GetTranslated().empty();
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

PoReader::PoReader(const std_fs::path& file_path)
        : file_name(file_path.string())
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
	LOG_WARNING(
	        "LOCALE: Translation file '%s' error in entry #%u "
	        "starting from line #%u, %s",
	        file_name.c_str(),
	        check_cast<unsigned int>(entry_counter),
	        check_cast<unsigned int>(entry_start_line),
	        error.c_str());
}

void PoReader::LogWarning(const int line_number, const std::string& error) const
{
	LOG_WARNING("LOCALE: Translation file '%s' error in line #%d, %s",
	            file_name.c_str(),
	            line_number,
	            error.c_str());
}

bool PoReader::ReadEntry()
{
	ResetEntry();

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

	bool found_entry      = false;
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

		// Skip generic comments
		if (line.starts_with(MarkerTranslatorComment) ||
		    line == EmptyCommentLine) {
			continue;
		}

		// If we are here, we have found a new entry
		found_entry = true;
		if (reader_state == State::Initial) {
			reader_state     = State::Normal;
			entry_start_line = line_counter;
			++entry_counter;
		}

		// Check for the end of PO entry
		if (line.empty()) {
			break;
		}

		// Skip extracted entry comments
		if (line.starts_with(MarkerExtractedComment)) {
			reader_state  = State::Normal;
			continue;
		}

		// Read source code location
		if (line.starts_with(MarkerLocation)) {
			if (found_location) {
				LogWarning(line_counter,
				           "string location is allowed "
				           "only once per entry");
			}
			found_location = true;
			reader_state   = State::Normal;
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
				LogWarning(line_counter,
				           "context is allowed only "
				           "once per entry");
			}
			found_context = true;
			reader_state  = State::ReadingContext;
			line          = line.substr(KeywordContext.size());
		} else if (line.starts_with(KeywordId)) {
			if (found_english) {
				LogWarning(line_counter,
				           "English message is allowed "
				           "only once per entry");
			}
			found_english = true;
			reader_state  = State::ReadingEnglish;
			line          = line.substr(KeywordId.size());
		} else if (line.starts_with(KeywordString)) {
			if (found_translated) {
				LogWarning(line_counter,
				           "translated message is allowed "
				           "only once per entry");
			}
			found_translated = true;
			reader_state     = State::ReadingTranslated;
			line             = line.substr(KeywordString.size());
		}
		trim(line);

		// Read the string
		if (line.size() >= 2 && line.starts_with(QuoteChar) &&
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
				LogWarning(line_counter, "unexpected string");
				break;
			}
			continue;
		}

		LogWarning(line_counter, "unrecognized content");
		reader_state = State::Normal;
	}

	// Prevent processing if nothing found and we have reached end of file
	if (IsEndOfFile() && !found_entry) {
		return false;
	}

	// Return I/O status
	return IsFileOk();
}

std::string PoReader::ReadSingleLineString(const std::string& line) const
{
	std::string result = {};

	// Skip the first and last character of the string, these are quotes
	for (size_t idx = 1; idx + 1 < line.size(); ++idx) {
		if (line[idx] != '\\') {
			result += line[idx];
			continue;
		}

		// This is not a regular character - decode the escaping
		++idx;
		switch (line[idx]) {
		case 'n': result += '\n'; break;
		case 'r': result += '\r'; break;
		case 't': result += '\t'; break;
		case '\\': result += '\\'; break;
		case '"': result += '"'; break;
		default:
			LogWarning(line_counter,
			           format_str("unsupported escaping character 0x%02x",
			                      line[idx]));
			result += '\\';
			result += line[idx];
			break;
		}
	}

	return result;
}

void PoReader::ReadLocation(const std::string& line)
{
	auto content = line.substr(MarkerLocation.size());
	trim(content);

	if (content.empty()) {
		LogWarning(line_counter, "string location is empty");
		return;
	}

	SetLocation(content);
}

void PoReader::ReadFlags(const std::string& line)
{
	auto content = line.substr(MarkerFlag.size());
	trim(content);

	if (content.empty()) {
		LogWarning(line_counter, "empty list of flags");
		return;
	}

	for (auto flag : split(content, ",")) {
		trim(flag);
		if (HasFlag(flag)) {
			LogWarning(line_counter,
			           format_str("duplicated flag '%s'", flag.c_str()));
		} else {
			AddFlag(flag);
		}
	}
}

bool PoReader::ValidateGettextMetadata() const
{
	assert(IsGettextMetadataEntry());

	if (!IsFirstEntry()) {
		LogWarning(
		        "only the first entry is expected to contain "
		        "gettext metadata");
		return true;
	}

	bool found_non_utf8 = false;
	bool found_charset  = false;

	// Check that content is UTF-8 encoded
	for (const auto& entry : split(GetTranslated(), "\n\r")) {
		const auto separator_position = entry.find(':');
		if (separator_position == 0 ||
		    separator_position == std::string::npos) {
			LogWarning("syntax error in gettext metadata");
			continue;
		}

		if (entry.substr(0, separator_position) != "Content-Type") {
			continue;
		}

		const auto values = entry.substr(separator_position + 1,
		                                 std::string::npos);
		for (auto value : split(values, ";")) {
			trim(value);
			if (!value.starts_with("charset=")) {
				continue;
			}

			if (found_charset) {
				LogWarning("gettext metadata already specified the charset");
			}
			found_charset = true;

			if (value != "charset=UTF-8" && !found_non_utf8) {
				found_non_utf8 = true;
				LogWarning(
				        "gettext metadata indicates "
				        "incompatible charset");
			}
		}
	}

	if (!found_charset) {
		LogWarning("gettext metadata does not specify charset");
	}

	return !found_non_utf8;
}

std::string PoReader::GetLanguageFromMetadata() const
{
	assert(IsGettextMetadataEntry());

	for (const auto& entry : split(GetTranslated(), "\n\r")) {
		auto tokens = split(entry, ":");
		if (tokens.size() != 2) {
			continue;
		}

		trim(tokens[0]);
		if (tokens[0] != "Language") {
			continue;
		}

		trim(tokens[1]);
		return tokens[1];
	}

	return {};
}

// ***************************************************************************
// PO file writer
// ***************************************************************************

PoWriter::PoWriter(const std_fs::path& file_path)
{
	out_file.open(file_path);
}

void PoWriter::WriteMultiLineString(std::string& value)
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
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			case '\\': result += "\\\\"; break;
			case '"': result += "\\\""; break;
			default: result += character; break;
			}
		}

		return result;
	};

	for (const auto& line : lines) {
		out_file << Quote + escape(line) + Quote + "\n";
	}
}

bool PoWriter::WriteEmptyLine()
{
	out_file << "\n";

	return IsFileOk();
}

bool PoWriter::WriteCommentLine(const std::string& comment)
{
	if (comment.empty()) {
		out_file << "#\n";
	} else {
		out_file << "# " << comment << "\n";
	}

	return IsFileOk();
}

bool PoWriter::WriteHeader(const std::string& language)
{
	// Write DOSBox-specific comments
	WriteCommentLine(DOSBOX_NAME " language file");
	WriteCommentLine();
	WriteCommentLine("Before editing read the translation manual:");
	WriteCommentLine(DOSBOX_MANUAL_TRANSLATION);
	WriteCommentLine();
	WriteEmptyLine();

	// Prepare standard file header metadata
	std::string header_data = {};
	auto add = [&header_data](const std::string& key, const std::string& value) {
		header_data += key + ": " + value + "\n";
	};

	add("Project-Id-Version", DOSBOX_PROJECT_NAME);
	add("Report-Msgid-Bugs-To", DOSBOX_BUGS_TO);
	add("Language", language);
	add("Content-Type", "text/plain; charset=UTF-8");
	add("Content-Transfer-Encoding", "8bit");
	add("MIME-Version", "1.0");
	add("X-Generator", std::string(DOSBOX_NAME " ") + DOSBOX_GetVersion());

	// Set the gettext metadata
	SetTranslated(header_data);

	// Write, return I/O status
	return WriteEntry();
}

bool PoWriter::WriteDosBoxMetadata(const std::string& key, const std::string& value,
                                   const std::vector<std::string>& help,
                                   const bool is_fuzzy)
{
	SetLocation(MetadataKey);
	SetContext(MetadataKey);

	if (is_fuzzy) {
		AddFlag(FlagFuzzy);
	}

	AddHelpLine("Do not translate this, set it according to the instruction!");

	auto english = key + "\n\n";
	assertm(!help.empty(), "Metadata help is mandatory");
	for (const auto& line : help) {
		english += line + "\n";
	}

	// Strip the trailing newline characters to prevent Poedit from
	// reporting an error
	while (english.ends_with('\n')) {
		english.pop_back();
	}

	SetEnglish(english);
	SetTranslated(value);

	// Write, return I/O status
	return WriteEntry();
}

bool PoWriter::WriteEntry()
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
	bool first_flag = true;
	if (HasFlag(FlagFuzzy)) {
		// Always write the fuzzy flag first
		out_file << MarkerFlag + FlagFuzzy;
		first_flag = false;		
	}
	for (const auto& flag : flags) {
		if (flag == FlagFuzzy) {
			// Fuzzy flag already written
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

	// Write context
	if (!context.empty()) {
		out_file << KeywordContext + " ";
		WriteMultiLineString(context);
	}

	// Write English string
	out_file << KeywordId + " ";
	WriteMultiLineString(english);

	// Write translated string
	out_file << KeywordString + " ";
	WriteMultiLineString(translated);

	// Add empty line
	out_file << "\n";

	// Clear stored entry data
	ResetEntry();

	// Return I/O status
	return IsFileOk();
}
