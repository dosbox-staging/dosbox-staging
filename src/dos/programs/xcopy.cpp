// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xcopy.h"

#include "misc/support.h"
#include "more_output.h"
#include "utils/ascii.h"

#include <array>

extern unsigned int result_errorcode;

static std::string path_join(const std::string& path, const std::string& segment)
{
	return path.ends_with(DosSeparator) || segment.starts_with(DosSeparator)
	             ? path + segment
	             : path + DosSeparator + segment;
}

static bool has_invalid_drive_spec(const std::string& path)
{
	if (path.length() < 2 || path[1] != ':') {
		// Path doesn't have a drive spec, so it can't be invalid
		return false;
	}
	const auto drive = path[0];
	return !isalpha(static_cast<unsigned char>(drive)) ||
	       !Drives[drive_index(drive)];
}

static std::vector<DOS_DTA::Result> find_all(const std::string& path,
                                             const DosFilename& spec,
                                             const FatAttributeFlags& attr)
{
	std::vector<DOS_DTA::Result> results = {};

	const auto search = path_join(path, to_string(spec));
	if (!DOS_FindFirst(search.c_str(), attr)) {
		return results;
	}

	const DOS_DTA dta(dos.dta());

	do {
		DOS_DTA::Result result = {};
		dta.GetResult(result);
		results.push_back(std::move(result));
	} while (DOS_FindNext());

	return results;
}

static std::string doserr_msg_get(const uint16_t error_code)
{
	switch (error_code) {
	case DOSERR_FILE_NOT_FOUND:
		return MSG_Get("PROGRAM_XCOPY_FILE_NOT_FOUND");
	case DOSERR_PATH_NOT_FOUND:
		return MSG_Get("PROGRAM_XCOPY_PATH_NOT_FOUND");
	case DOSERR_TOO_MANY_OPEN_FILES:
		return MSG_Get("PROGRAM_XCOPY_TOO_MANY_OPEN_FILES");
	case DOSERR_ACCESS_DENIED:
		return MSG_Get("PROGRAM_XCOPY_ACCESS_DENIED");
	case DOSERR_LOCK_VIOLATION:
		return MSG_Get("PROGRAM_XCOPY_LOCK_VIOLATION");
	default: return MSG_Get("PROGRAM_XCOPY_GENERAL_FAILURE");
	}
}

void XCOPY::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_XCOPY_HELP_LONG"));
		output.Display();
		return;
	}

	if (!Initialize()) {
		WriteOut(MSG_Get("PROGRAM_XCOPY_FILES_COPIED"), 0);
		result_errorcode = enum_val(XcopyExitCode::InitializationError);
		return;
	}

	const auto maybe_snapshot = BuildSourceSnapshot();
	if (!maybe_snapshot) {
		WriteOut(MSG_Get("PROGRAM_XCOPY_FILES_COPIED"), 0);
		result_errorcode = enum_val(XcopyExitCode::FileNotFoundError);
		return;
	}

	const auto [success, copied_count] = CopyToDestination(*maybe_snapshot);
	if (!success) {
		result_errorcode = enum_val(XcopyExitCode::InitializationError);
		// Don't return so we can still show the copied count
	}

	WriteOut(MSG_Get("PROGRAM_XCOPY_FILES_COPIED"), copied_count);
}

bool XCOPY::InitializeSource(const std::string& value)
{
	if (has_invalid_drive_spec(value)) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_DRIVE"));
		return false;
	}

	if (DOS_FindDevice(value.c_str()) != DOS_DEVICES) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_RESERVED_DEVICE_SOURCE"));
		return false;
	}

	// The original XCOPY implementation rejects trailing slashes when the
	// source is not a bare drive (eg: C:\). Canonicalization makes
	// this implementation more tolerant.
	const auto canonicalized = DOS_Canonicalize(value.c_str());
	if (canonicalized.empty()) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return false;
	}

	FatAttributeFlags attr = {};
	if (canonicalized.find_last_of("?*") == std::string::npos) {
		// If there are no wildcards, source may be a folder and the
		// filespec is omitted. If it is, assume *.*.
		if (DOS_GetFileAttr(canonicalized.c_str(), &attr) && attr.directory) {
			src_path = canonicalized;
			src_spec = AllFiles;
			return true;
		}

		// The specified path is a file or does not exist. If the user
		// wants a directory (trailing slash), tell them it's invalid.
		// Using value here because canonicalization may strip slashes.
		if (value.ends_with(DosSeparator)) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
			return false;
		}
	}

	const auto last_sep_pos = canonicalized.rfind(DosSeparator);
	assertm(last_sep_pos != std::string::npos,
	        "Canonicalization ensures a slash is always present");

	src_path = canonicalized.substr(0, last_sep_pos);
	src_spec = parse_dos_filename(canonicalized.substr(last_sep_pos + 1));
	if (!DOS_GetFileAttr(src_path.c_str(), &attr) || !attr.directory) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return false;
	}

	return true;
}

bool XCOPY::InitializeDestination(const std::string& value)
{
	if (has_invalid_drive_spec(value)) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_DRIVE"));
		return false;
	}

	const auto canonicalized = DOS_Canonicalize(value.c_str());
	if (canonicalized.empty()) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return false;
	}

	// If it has a trailing slash, assume it is a directory. Using
	// value here because canonicalization may strip slashes.
	if (value.ends_with(DosSeparator)) {
		if (canonicalized.find_last_of("?*") != std::string::npos) {
			// Canonicalization does not reject directories with
			// wildcards in their name, but they're still invalid.
			WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
			return false;
		}

		dest_path = canonicalized;
		dest_spec = AllFiles;
		return true;
	}

	// Check if canonicalized exists and is a directory. Note that
	// we could in theory check if canonicalized exists and is a
	// file, but the original XCOPY doesn't do that: it always
	// prompts when this check fails.
	FatAttributeFlags attr = {};
	if (DOS_GetFileAttr(canonicalized.c_str(), &attr) && attr.directory) {
		dest_path = canonicalized;
		dest_spec = AllFiles;
		return true;
	}

	if (canonicalized.find_last_of("?*") == std::string::npos) {
		// The directory doesn't exists, the user didn't mark the
		// destination with a trailing slash and there are no wildcards,
		// we have no idea what the user wants.
		WriteOut(MSG_Get("PROGRAM_XCOPY_FD_PROMPT"),
		         get_basename(canonicalized).c_str());
		const auto maybe_letter = PromptForLetter("fd", false);
		if (!maybe_letter) {
			return false;
		}

		// User wants a directory.
		if (maybe_letter == 'd') {
			dest_path = canonicalized;
			dest_spec = AllFiles;
			return true;
		}
	}

	const auto last_sep_pos = canonicalized.rfind(DosSeparator);
	assertm(last_sep_pos != std::string::npos,
	        "Canonicalization ensures a slash is always present");

	dest_path = canonicalized.substr(0, last_sep_pos);
	dest_spec = parse_dos_filename(canonicalized.substr(last_sep_pos + 1));
	if (dest_path.find_last_of("?*") != std::string::npos) {
		// Canonicalization does not reject directories with wildcards
		// in their name, but they're still invalid.
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return false;
	}

	return true;
}

bool XCOPY::Initialize()
{
	std::string found = {};

	// TODO: support all switches
	if (cmd->FindStringBegin("/", found)) {
		// found does not contain the leading /, the message key does
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_SWITCH"), found.c_str());
		return false;
	}

	const auto args = cmd->GetArguments();
	if (args.size() == 0 || args.size() > 2) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PARAM_NUM"));
		return false;
	}

	return InitializeSource(args[0]) &&
	       InitializeDestination(args.size() == 2 ? args[1] : CurrentDirectory);
}

std::optional<std::vector<DOS_DTA::Result>> XCOPY::BuildSourceSnapshot()
{
	static constexpr auto FileAttributes = FatAttributeFlags::Archive |
	                                       FatAttributeFlags::ReadOnly;

	const auto original_dta_ptr = dos.dta();
	dos.dta(dos.tables.tempdta);

	const auto found = find_all(src_path, src_spec, FileAttributes);

	dos.dta(original_dta_ptr);

	if (found.empty()) {
		WriteOut(MSG_Get("PROGRAM_XCOPY_FILE_NOT_FOUND"),
		         to_string(src_spec).c_str());
		return {};
	}

	// Inform the user we found something
	WriteOut(MSG_Get("PROGRAM_XCOPY_READING_SOURCE"));
	return found;
}

bool XCOPY::MakeDirectoryTree(const std::string& path)
{
	FatAttributeFlags attr = {};

	// skips the drive spec X:\, always present for canonicalized paths
	size_t find_pos = path.find(DosSeparator, 2);

	do {
		find_pos                = path.find(DosSeparator, find_pos + 1);
		const auto partial_path = path.substr(0, find_pos);

		if (DOS_GetFileAttr(partial_path.c_str(), &attr)) {
			if (!attr.directory) {
				WriteErr(MSG_Get("PROGRAM_XCOPY_UNABLE_CREATE_DIR"));
				return false;
			}
		} else if (!DOS_MakeDir(partial_path.c_str())) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_UNABLE_CREATE_DIR"));
			return false;
		}
	} while (find_pos != std::string::npos);

	return true;
}

bool XCOPY::CopyFile(const uint16_t source_handle, const uint16_t dest_handle,
                     const char* filename)
{
	static constexpr uint16_t CopyBufferSize = 0x8000;
	std::array<uint8_t, CopyBufferSize> buf  = {};
	bool eof                                 = false;

	while (!eof) {
		// Interrupt the cycle safely when shutting down
		if (DOSBOX_IsShutdownRequested()) {
			LOG_WARNING("XCOPY: shutdown requested, aborting copy loop");
			return false;
		}

		uint16_t bytes_read = CopyBufferSize;
		if (!DOS_ReadFile(source_handle, buf.data(), &bytes_read)) {
			WriteErr(doserr_msg_get(dos.errorcode), filename);
			return false;
		}

		if (bytes_read != CopyBufferSize) {
			// Last loop
			eof = true;
		}

		uint16_t bytes_written = bytes_read;
		if (!DOS_WriteFile(dest_handle, buf.data(), &bytes_written)) {
			WriteErr(doserr_msg_get(dos.errorcode), filename);
			return false;
		}

		if (bytes_read != bytes_written) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_INSUFFICIENT_DISK_SPACE"),
			         filename);
			return false;
		}
	}

	return true;
}

std::optional<char> XCOPY::PromptForLetter(const std::string& allowed,
                                           const bool cr_confirms)
{
	char selected         = '\0';
	bool requires_confirm = cr_confirms;

	do {
		uint8_t answer = 0;
		uint16_t count = 1;

		if (DOSBOX_IsShutdownRequested()) {
			LOG_INFO("XCOPY: shutdown requested, aborting user prompt loop");
			return {};
		}

		if (!DOS_ReadFile(STDIN, &answer, &count) || count == 0) {
			LOG_ERR("XCOPY: broken stdin, aborting user prompt loop");
			return {};
		}

		// cr_confirms is used to request the user to confirm their
		// selection with a CR (produced by the Enter key). If false the
		// loop ends immediately on any valid char.
		requires_confirm = cr_confirms &&
		                   answer != Ascii::ControlCode::CarriageReturn;

		const char letter = tolower(answer);
		if (allowed.contains(letter)) {
			WriteOut(selected ? "\b%c" : "%c", answer);
			selected = letter;
		}
	} while (!selected || requires_confirm);

	WriteOut("\n");

	return selected;
}

std::pair<bool, int> XCOPY::CopyToDestination(const std::vector<DOS_DTA::Result>& files)
{
	int copied_count = 0;

	// Create destination directory if it doesn't exist
	// TODO: with /e this must be done even if the source matches nothing
	if (!MakeDirectoryTree(dest_path)) {
		return std::make_pair(false, copied_count);
	}

	// We always overwrite if we are ran from a batch file
	bool overwrite_all = !IsInvokedInteractively();

	for (const DOS_DTA::Result& file : files) {
		const auto to_file = apply_pattern(file.name, dest_spec);
		if (DOS_FindDevice(to_file.c_str()) != DOS_DEVICES) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_RESERVED_DEVICE_DEST"));
			return std::make_pair(true, copied_count);
		}

		const auto from_path = path_join(src_path, file.name);
		const auto to_path = path_join(dest_path, to_file);
		if (from_path == to_path) {
			// The original XCOPY implementation doesn't produce an
			// error in this case, it just stops.
			WriteErr(MSG_Get("PROGRAM_XCOPY_COPY_ONTO_ITSELF"));
			return std::make_pair(true, copied_count);
		}

		uint16_t from_handle = {};
		if (!DOS_OpenFile(from_path.c_str(), OPEN_READ, &from_handle)) {
			WriteErr(doserr_msg_get(dos.errorcode), file.name.c_str());
			return std::make_pair(false, copied_count);
		}

		if (!overwrite_all && DOS_FileExists(to_path.c_str())) {
			// Ask the user what to do if the file already exists
			WriteErr(MSG_Get("PROGRAM_XCOPY_OVERWRITE_PROMPT"),
			         to_path.c_str());

			const auto maybe_letter = PromptForLetter("yna", true);
			if (!maybe_letter) {
				// Prompt aborted
				DOS_CloseFile(from_handle);
				return {};
			}

			overwrite_all = maybe_letter == 'a';
			if (maybe_letter == 'n') {
				DOS_CloseFile(from_handle);
				continue;
			}
		} else {
			WriteOut("%s\n", file.name.c_str());
		}

		uint16_t to_handle = {};
		if (!DOS_CreateFile(to_path.c_str(),
		                    FatAttributeFlags::Archive,
		                    &to_handle)) {
			// Without this exception it'll print "File not found",
			// which would be strange for a file we tried to create.
			const auto msg = dos.errorcode == DOSERR_FILE_NOT_FOUND
			                       ? MSG_Get("PROGRAM_XCOPY_FILE_CREATION_ERROR")
			                       : doserr_msg_get(dos.errorcode);
			WriteErr(msg, file.name.c_str());
			DOS_CloseFile(from_handle);
			return std::make_pair(false, copied_count);
		}

		const bool success = CopyFile(from_handle,
		                              to_handle,
		                              file.name.c_str());

		if (success) {
			DOS_SetFileDate(to_handle, file.time, file.date);
		}

		DOS_CloseFile(from_handle);
		DOS_CloseFile(to_handle);

		if (!success) {
			// Try deleting partial destination
			DOS_UnlinkFile(to_path.c_str());
			return std::make_pair(false, copied_count);
		}

		copied_count++;
	}

	return std::make_pair(true, copied_count);
}

void XCOPY::AddMessages()
{
	MSG_Add("PROGRAM_XCOPY_INVALID_PARAM_NUM", "Invalid number of parameters\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_SWITCH", "Invalid switch - /%s\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_PATH", "Invalid path\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_DRIVE", "Invalid drive specification\n");

	MSG_Add("PROGRAM_XCOPY_FILE_NOT_FOUND", "File not found - %s\n");

	MSG_Add("PROGRAM_XCOPY_PATH_NOT_FOUND", "Path not found - %s\n");

	MSG_Add("PROGRAM_XCOPY_FILE_CREATION_ERROR", "File creation error\n");

	MSG_Add("PROGRAM_XCOPY_UNABLE_CREATE_DIR", "Unable to create directory\n");

	MSG_Add("PROGRAM_XCOPY_TOO_MANY_OPEN_FILES", "Too many open files\n");

	MSG_Add("PROGRAM_XCOPY_LOCK_VIOLATION", "Lock violation - %s\n");

	MSG_Add("PROGRAM_XCOPY_ACCESS_DENIED", "Access denied - %s\n");

	MSG_Add("PROGRAM_XCOPY_GENERAL_FAILURE", "General failure - %s\n");

	MSG_Add("PROGRAM_XCOPY_INSUFFICIENT_DISK_SPACE", "Insufficient disk space\n");

	MSG_Add("PROGRAM_XCOPY_RESERVED_DEVICE_SOURCE",
	        "Cannot copy from a reserved device\n");

	MSG_Add("PROGRAM_XCOPY_RESERVED_DEVICE_DEST",
	        "Cannot copy to a reserved device\n");

	MSG_Add("PROGRAM_XCOPY_COPY_ONTO_ITSELF",
	        "File cannot be copied onto itself\n");

	MSG_Add("PROGRAM_XCOPY_READING_SOURCE", "Reading source file(s)...\n");

	MSG_Add("PROGRAM_XCOPY_OVERWRITE_PROMPT", "Overwrite %s (Yes/No/All)?");

	MSG_Add("PROGRAM_XCOPY_FD_PROMPT",
	        "Does %s specify a file name\n"
	        "or directory name on the target\n"
	        "(F = file, D = directory)?");

	MSG_Add("PROGRAM_XCOPY_FILES_COPIED", "%9d File(s) copied\n");

	MSG_Add("PROGRAM_XCOPY_HELP", "Copy files and directory trees.\n");

	MSG_Add("PROGRAM_XCOPY_HELP_LONG",
	        "Copies files (except hidden and system files) and directory trees.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]xcopy[reset] [color=white]SOURCE[reset] [[color=light-cyan]DESTINATION[reset]] [/a|/m] [/d:[color=light-cyan]DATE[reset]] [/p] [/s|/e] [/v] [/w] [/y|/-y]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]SOURCE[reset]       the file or files to copy\n"
	        "  [color=light-cyan]DESTINATION[reset]  the location and/or name of the new files; current working \n"
	        "               directory if omitted\n"
	        "  /a           copy only files with the archive attribute set\n"
	        "  /m           same as /a, but clear the archive attribute in the source files\n"
	        "  /d:[color=light-cyan]DATE[reset]      copy only files changed on or after [color=light-cyan]DATE[reset]\n"
	        "  /p           prompt before creating each destination file\n"
	        "  /s           copy entire source tree, except empty directories\n"
	        "  /e           copy entire source tree, including empty directories\n"
	        "  /v           verify each new file as it is written\n"
	        "  /w           prompt to press a key before copying starts\n"
	        "  /y           overwrite existing destination files without prompting for\n"
	        "               confirmation\n"
	        "  /-y          prompt for confirmation before overwriting an existing\n"
	        "               destination file\n"
	        "\n"
	        "Notes:\n"
	        "  - The COPYCMD environment variable can be preset to /y or /-y. Either can\n"
	        "    be overridden by giving the opposite switch on the command line.\n"
	        "\n"
	        "  - [color=light-cyan]DATE[reset] takes a two- or four-digit year, with the day and month in the\n"
	        "    order your country setting uses. Two-digit years 80 to 99 mean 1980 to\n"
	        "    1999 and 00 to 79 mean 2000 to 2079; allowed four-digit years are in the\n"
	        "    range from 1980 to 2099.\n"
	        "\n"
	        "  - Copies are always created with the archive attribute set. The read-only,\n"
	        "    hidden and system attributes are not carried over.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]xcopy[reset] [color=white]a:[reset] [color=light-cyan]b:[reset] /e\n"
	        "  [color=light-green]xcopy[reset] [color=white]..\\*.*[reset] /d:[color=light-cyan]01-01-93[reset]");
}