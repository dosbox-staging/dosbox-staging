// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xcopy.h"

#include "cpu/callback.h"
#include "cpu/registers.h"
#include "misc/support.h"
#include "more_output.h"
#include "utils/ascii.h"

#include <array>
#include <charconv>
#include <ranges>

extern unsigned int result_errorcode;

static std::string path_join(const std::string& base, const std::string& path)
{
	if (base.empty()) {
		return path;
	}

	if (path.empty()) {
		return base;
	}

	return base.ends_with(DosSeparator) || path.starts_with(DosSeparator)
	             ? base + path
	             : base + DosSeparator + path;
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

static bool is_readonly_drive(const char drive)
{
	return Drives[drive_index(drive)]->IsReadOnly();
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

// Stricter than parse_int (disallows +- prefix and whitespace)
static std::optional<uint32_t> parse_date_field(const std::string_view str)
{
	const char* first = str.data();
	const char* last  = first + str.size();
	uint32_t value    = 0;

	// Invalid if errored or if we didn't consume the full string
	const auto [ptr, ec] = std::from_chars(first, last, value);
	if (ec != std::errc() || ptr != last) {
		return {};
	}

	return value;
}

static std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> split_date(
        const std::string& str)
{
	static constexpr std::string_view DateDelimiters = "./-";

	const size_t delim_pos = str.find_first_of(DateDelimiters);
	if (delim_pos == std::string::npos) {
		return {};
	}
	const char delimiter = str[delim_pos];

	const auto fields = split_with_empties(str, std::string_view(&delimiter, 1));
	if (fields.size() != 3) {
		return {};
	}

	const auto maybe_first  = parse_date_field(fields[0]);
	const auto maybe_second = parse_date_field(fields[1]);
	const auto maybe_third  = parse_date_field(fields[2]);

	if (maybe_first && maybe_second && maybe_third) {
		return std::make_tuple(*maybe_first, *maybe_second, *maybe_third);
	}

	return {};
}

static std::optional<uint16_t> parse_date_arg(const std::string& str)
{
	// the first character is always :
	if (str[0] != ':') {
		return {};
	}

	const auto maybe_parts = split_date(str.substr(1));
	if (!maybe_parts) {
		return {};
	}

	uint32_t year  = 0;
	uint32_t month = 0;
	uint32_t day   = 0;

	switch (DOS_GetLocaleDateFormat()) {
	case DosDateFormat::YearMonthDay:
		std::tie(year, month, day) = *maybe_parts;
		break;
	case DosDateFormat::MonthDayYear:
		std::tie(month, day, year) = *maybe_parts;
		break;
	default: // DosDateFormat::DayMonthYear:
		std::tie(day, month, year) = *maybe_parts;
		break;
	}

	// Maps 00-79 to 2000-2079 and 80-99 to 1980-1999. The original
	// implementation of XCOPY refuses dates before 1980 when the year is
	// 2 digits only.
	if (year < 100) {
		year += year < 80 ? 2000 : 1900;
	}

	// Reject invalid dates
	if (!is_date_valid(year, month, day)) {
		return {};
	}

	return DOS_PackDate(year, month, day);
}

// Must be called after CALLBACK_Idle(), which runs the emulated machine
// so that pending key presses reach the BIOS keyboard buffer.
static bool has_ctrlc_been_pressed()
{
	// Stores in reg_al the pressed key
	reg_ah = is_machine_ega_or_better() ? 0x11 : 0x1;
	CALLBACK_RunRealInt(0x16);
	if (GETFLAG(ZF) || reg_al != Ascii::CtrlC) {
		return false;
	}

	// Removes Ctrl+C from the buffer
	reg_ah = is_machine_ega_or_better() ? 0x10 : 0x0;
	CALLBACK_RunRealInt(0x16);
	return true;
}

void XCOPY::AfterRun(const XCopyError error)
{
	if (error == XCopyError::Interrupted) {
		WriteOut("^C\n");
	} else {
		WriteOut(MSG_Get("PROGRAM_XCOPY_FILES_COPIED"), copied_count);
	}

	result_errorcode = enum_val(error);
}

void XCOPY::Run()
{
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_XCOPY_HELP_LONG"));
		output.Display();
		return;
	}

	auto error = Initialize();
	if (error != XCopyError::NoError) {
		AfterRun(error);
		return;
	}

	error = FailOnCyclicCopy();
	if (error != XCopyError::NoError) {
		AfterRun(error);
		return;
	}

	error = BuildSourceSnapshot();
	// FileNotFound does not stop the copy: the original XCOPY
	// implementation creates the destination tree when /e is set.
	if (error != XCopyError::NoError && error != XCopyError::FileNotFound) {
		AfterRun(error);
		return;
	}

	// The original XCOPY implementation does not eagerly check for
	// writeability, but waits for the first failure and DOS then shows the
	// write protect error prompt.
	if (!src_snapshot.empty() && is_readonly_drive(dest_path[0])) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_ACCESS_DENIED"), dest_path.c_str());
		AfterRun(XCopyError::CriticalError);
		return;
	}

	const bool found_nothing = error == XCopyError::FileNotFound;

	error = CopyToDestination();
	if (error != XCopyError::NoError) {
		AfterRun(error);
		return;
	}

	if (found_nothing) {
		// Unlike every other diagnostic, the original XCOPY
		// implementation prints this one on stdout
		WriteOut(MSG_Get("PROGRAM_XCOPY_FILE_NOT_FOUND"),
		         to_string(src_spec).c_str());
		AfterRun(XCopyError::FileNotFound);
		return;
	}

	AfterRun(XCopyError::NoError);
}

XCopyError XCOPY::InitializeSource(const std::string& value)
{
	if (DOS_FindDevice(value.c_str()) != DOS_DEVICES) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_RESERVED_DEVICE_SOURCE"));
		return XCopyError::Failure;
	}

	// The original XCOPY implementation rejects trailing slashes when the
	// source is not a bare drive (eg: C:\). Canonicalization makes
	// this implementation more tolerant.
	const auto canonicalized = DOS_Canonicalize(value.c_str());
	if (canonicalized.empty()) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return XCopyError::Failure;
	}

	FatAttributeFlags attr = {};
	if (canonicalized.find_last_of("?*") == std::string::npos) {
		// If there are no wildcards, source may be a folder and the
		// filespec is omitted. If it is, assume *.*.
		if (DOS_GetFileAttr(canonicalized.c_str(), &attr) && attr.directory) {
			src_path = canonicalized;
			src_spec = AllFiles;
			return XCopyError::NoError;
		}

		// The specified path is a file or does not exist. If the user
		// wants a directory (trailing slash), tell them it's invalid.
		// Using value here because canonicalization may strip slashes.
		if (value.ends_with(DosSeparator)) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
			return XCopyError::Failure;
		}
	}

	const auto last_sep_pos = canonicalized.rfind(DosSeparator);
	assertm(last_sep_pos != std::string::npos,
	        "Canonicalization ensures a slash is always present");
	// If last_sep_pos is 2 it means we have a bare drive (C:\) on the left
	// side of the separator, so we should include it in the path.
	const auto path_len = last_sep_pos == 2 ? 3 : last_sep_pos;

	src_path = canonicalized.substr(0, path_len);
	src_spec = parse_dos_filename(canonicalized.substr(last_sep_pos + 1));
	if (!DOS_GetFileAttr(src_path.c_str(), &attr) || !attr.directory) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return XCopyError::Failure;
	}

	return XCopyError::NoError;
}

XCopyError XCOPY::InitializeDestination(const std::string& value)
{
	const auto canonicalized = DOS_Canonicalize(value.c_str());
	if (canonicalized.empty()) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return XCopyError::Failure;
	}

	// If it has a trailing slash, assume it is a directory. Using
	// value here because canonicalization may strip slashes.
	if (value.ends_with(DosSeparator)) {
		if (canonicalized.find_last_of("?*") != std::string::npos) {
			// Canonicalization does not reject directories with
			// wildcards in their name, but they're still invalid.
			WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
			return XCopyError::Failure;
		}

		dest_path = canonicalized;
		dest_spec = AllFiles;
		return XCopyError::NoError;
	}

	// Check if canonicalized exists and is a directory. Note that
	// we could in theory check if canonicalized exists and is a
	// file, but the original XCOPY doesn't do that: it always
	// prompts when this check fails.
	FatAttributeFlags attr = {};
	if (DOS_GetFileAttr(canonicalized.c_str(), &attr) && attr.directory) {
		dest_path = canonicalized;
		dest_spec = AllFiles;
		return XCopyError::NoError;
	}

	if (canonicalized.find_last_of("?*") == std::string::npos) {
		// The directory doesn't exist, the user didn't mark the
		// destination with a trailing slash and there are no wildcards,
		// we have no idea what the user wants.
		WriteOut(MSG_Get("PROGRAM_XCOPY_FD_PROMPT"),
		         get_basename(canonicalized).c_str());
		const auto maybe_letter = PromptForLetter("fd", false);

		if (maybe_letter == Ascii::CtrlC) {
			return XCopyError::Interrupted;
		}

		if (!maybe_letter) {
			return XCopyError::Failure;
		}

		// User wants a directory.
		if (maybe_letter == 'd') {
			dest_path = canonicalized;
			dest_spec = AllFiles;
			return XCopyError::NoError;
		}
	}

	const auto last_sep_pos = canonicalized.rfind(DosSeparator);
	assertm(last_sep_pos != std::string::npos,
	        "Canonicalization ensures a slash is always present");

	// If last_sep_pos is 2 it means we have a bare drive (C:\) on the left
	// side of the separator, so we should include it in the path.
	const auto path_len = last_sep_pos == 2 ? 3 : last_sep_pos;

	dest_path = canonicalized.substr(0, path_len);
	dest_spec = parse_dos_filename(canonicalized.substr(last_sep_pos + 1));
	if (dest_path.find_last_of("?*") != std::string::npos) {
		// Canonicalization does not reject directories with wildcards
		// in their name, but they're still invalid.
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PATH"));
		return XCopyError::Failure;
	}

	return XCopyError::NoError;
}

XCopyError XCOPY::Initialize()
{
	std::string found = {};

	if (cmd->FindExist("/s", true)) {
		deep_copy = true;
	}

	if (cmd->FindExist("/e", true)) {
		// The /e switch implies /s as well
		deep_copy       = true;
		copy_empty_dirs = true;
	}

	if (cmd->FindStringBegin("/d", found, true)) {
		const auto maybe_value = parse_date_arg(found);
		if (!maybe_value) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_DATE"));
			return XCopyError::Failure;
		}
		maybe_min_date = maybe_value;
	}

	// /y and /-y are exclusive: this block only removes one, so that
	// the fallback can catch the other one and produce an error.
	if (cmd->FindExist("/y", true)) {
		display_overwrite_prompt = false;
	} else if (cmd->FindExist("/-y", true)) {
		display_overwrite_prompt = true;
	} else {
		display_overwrite_prompt = GetDefaultDisplayOverwritePrompt();
	}

	if (cmd->FindExist("/a", true)) {
		copy_archived_files_only = true;
	}

	if (cmd->FindExist("/m", true)) {
		// The /m switch implies /a as well
		copy_archived_files_only = true;
		clear_archive_attribute  = true;
	}

	// Discard the /v flag, DOSBox does not implement write with verify
	cmd->FindExist("/v", true);

	display_copy_prompt = cmd->FindExist("/p", true);

	const auto display_wait_prompt = cmd->FindExist("/w", true);

	if (cmd->FindStringBegin("/", found)) {
		// found does not contain the leading /, the message key does
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_SWITCH"), found.c_str());
		return XCopyError::Failure;
	}

	const auto args = cmd->GetArguments();
	if (args.size() == 0 || args.size() > 2) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_PARAM_NUM"));
		return XCopyError::Failure;
	}

	const auto src  = args[0];
	const auto dest = args.size() == 2 ? args[1] : CurrentDirectory;

	// The original XCOPY implementation checks for drive validity before
	// the wait prompt
	if (has_invalid_drive_spec(src) || has_invalid_drive_spec(dest)) {
		WriteErr(MSG_Get("PROGRAM_XCOPY_INVALID_DRIVE"));
		return XCopyError::Failure;
	}

	// This prompt has the purpose of letting the user switch disks when
	// XCOPY is running from a floppy, so it must be run before src checks.
	if (display_wait_prompt) {
		WriteOut(MSG_Get("PROGRAM_XCOPY_WAIT_PROMPT"));

		uint8_t answer = 0;
		uint16_t count = 1;

		if (!DOS_ReadFile(STDIN, &answer, &count) || count == 0) {
			LOG_WARNING("XCOPY: broken or empty stdin");
			is_stdin_broken = true;
		}

		if (DOSBOX_IsShutdownRequested() || answer == Ascii::CtrlC) {
			return XCopyError::Interrupted;
		}

		WriteOut("\n");
	}

	const auto error = InitializeSource(src);
	return error == XCopyError::NoError ? InitializeDestination(dest) : error;
}

bool XCOPY::GetDefaultDisplayOverwritePrompt() const
{
	auto copycmd = psp->GetEnvironmentValue("COPYCMD").value_or("");
	trim(copycmd);

	if (iequals(copycmd, "/y")) {
		return false;
	}
	if (iequals(copycmd, "/-y")) {
		return true;
	}

	return IsInvokedInteractively();
}

XCopyError XCOPY::FailOnCyclicCopy()
{
	if (deep_copy) {
		// Ensuring that src_path has a separator at the end makes
		// the prefix check robust against situations where dest_path
		// starts with src_path but is a different directory (eg:
		// C:\FOOBAR with src_path is C:\FOO). Works because both paths
		// are canonicalized.
		const auto src_prefix = src_path.ends_with(DosSeparator)
		                              ? src_path
		                              : src_path + DosSeparator;
		if (dest_path.starts_with(src_prefix) || src_path == dest_path) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_CYCLIC_COPY"));
			return XCopyError::Failure;
		}
	}

	return XCopyError::NoError;
}

XCopyError XCOPY::BuildSourceSnapshot()
{
	static constexpr auto FileAttributes = FatAttributeFlags::Archive |
	                                       FatAttributeFlags::ReadOnly;

	const auto original_dta_ptr = dos.dta();
	dos.dta(dos.tables.tempdta);

	bool read_msg_displayed        = false;
	bool found_something           = false;
	std::vector<std::string> paths = {};
	paths.push_back("");

	while (!paths.empty()) {
		if (CALLBACK_Idle() || has_ctrlc_been_pressed()) {
			dos.dta(original_dta_ptr);
			return XCopyError::Interrupted;
		}

		const auto path = paths.back();
		paths.pop_back();
		const auto from_base = path_join(src_path, path);

		// First pass: look for files that match the filespec
		const auto files = find_all(from_base, src_spec, FileAttributes);

		found_something |= !files.empty();

		// Filter the files by date and archive flag, if specified
		std::vector<DOS_DTA::Result> filtered = {};
		for (const auto& file : files) {
			if (CALLBACK_Idle() || has_ctrlc_been_pressed()) {
				dos.dta(original_dta_ptr);
				return XCopyError::Interrupted;
			}

			if (maybe_min_date && file.date < *maybe_min_date) {
				continue;
			}

			if (copy_archived_files_only && !file.attr.archive) {
				continue;
			}

			if (display_copy_prompt) {
				const auto from_path = path_join(from_base,
				                                 file.name);
				WriteOut(MSG_Get("PROGRAM_XCOPY_COPY_PROMPT"),
				         from_path.c_str());

				const auto maybe_letter = PromptForLetter("yn", false);

				if (maybe_letter == Ascii::CtrlC) {
					dos.dta(original_dta_ptr);
					return XCopyError::Interrupted;
				}

				if (!maybe_letter || maybe_letter == 'n') {
					continue;
				}
			}

			if (!read_msg_displayed) {
				WriteOut(MSG_Get("PROGRAM_XCOPY_READING_SOURCE"));
				read_msg_displayed = true;
			}

			filtered.push_back(file);
		}

		// We create each directory that is a key in files. Normally,
		// a directory gets created if it has some files in it, but
		// if copy_empty_dirs is set, we need to create empty
		// directories as well.
		if (!filtered.empty() || copy_empty_dirs) {
			src_snapshot.push_back({path, std::move(filtered)});
		}

		if (!deep_copy) {
			break;
		}

		// Second pass: look for directories if deep_copy is true.
		// attr not exclusive: this will return directories AND files.
		const auto results = find_all(from_base,
		                              AllFiles,
		                              FatAttributeFlags::Directory);

		// Reverse iteration ensures depth-first snapshot construction
		for (const auto& result : results | std::views::reverse) {
			// We need to exclude . and .. to prevent endless loops
			if (result.attr.directory && !result.name.starts_with('.')) {
				paths.push_back(path_join(path, result.name));
			}
		}
	}

	dos.dta(original_dta_ptr);

	return found_something ? XCopyError::NoError : XCopyError::FileNotFound;
}

XCopyError XCOPY::MakeDirectoryTree(const std::string& path)
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
				return XCopyError::Failure;
			}
		} else if (!DOS_MakeDir(partial_path.c_str())) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_UNABLE_CREATE_DIR"));
			return XCopyError::Failure;
		}
	} while (find_pos != std::string::npos);

	return XCopyError::NoError;
}

XCopyError XCOPY::CopyFile(const uint16_t source_handle,
                           const uint16_t dest_handle, const char* filename)
{
	static constexpr uint16_t CopyBufferSize = 0x8000;
	std::array<uint8_t, CopyBufferSize> buf  = {};
	bool eof                                 = false;

	while (!eof) {
		if (DOSBOX_IsShutdownRequested()) {
			return XCopyError::Interrupted;
		}

		uint16_t bytes_read = CopyBufferSize;
		if (!DOS_ReadFile(source_handle, buf.data(), &bytes_read)) {
			WriteErr(doserr_msg_get(dos.errorcode), filename);
			return XCopyError::Failure;
		}

		if (bytes_read != CopyBufferSize) {
			// Last loop
			eof = true;
		}

		uint16_t bytes_written = bytes_read;
		if (!DOS_WriteFile(dest_handle, buf.data(), &bytes_written)) {
			WriteErr(doserr_msg_get(dos.errorcode), filename);
			return XCopyError::Failure;
		}

		if (bytes_read != bytes_written) {
			WriteErr(MSG_Get("PROGRAM_XCOPY_INSUFFICIENT_DISK_SPACE"),
			         filename);
			return XCopyError::Failure;
		}
	}

	return XCopyError::NoError;
}

std::optional<char> XCOPY::PromptForLetter(const std::string& allowed,
                                           const bool cr_confirms)
{
	char selected         = '\0';
	bool requires_confirm = cr_confirms;

	if (is_stdin_broken) {
		WriteOut("\n");
		return {};
	}

	do {
		uint8_t answer = 0;
		uint16_t count = 1;

		if (DOSBOX_IsShutdownRequested()) {
			return {};
		}

		if (!DOS_ReadFile(STDIN, &answer, &count) || count == 0) {
			LOG_WARNING("XCOPY: broken or empty stdin");
			is_stdin_broken = true;
			WriteOut("\n");
			return {};
		}

		if (answer == Ascii::CtrlC) {
			return Ascii::CtrlC;
		}

		// cr_confirms is used to request the user to confirm their
		// selection with a CR (produced by the Enter key). If false the
		// loop ends immediately on any valid char.
		requires_confirm = cr_confirms && answer != Ascii::CarriageReturn;

		const char letter = tolower(answer);
		if (allowed.contains(letter)) {
			WriteOut(selected ? "\b%c" : "%c", answer);
			selected = letter;
		}
	} while (!selected || requires_confirm);

	WriteOut("\n");

	return selected;
}

XCopyError XCOPY::CopyToDestination()
{
	bool overwrite_all = !display_overwrite_prompt;

	for (const auto& [path, files] : src_snapshot) {
		const auto from_base = path_join(src_path, path);
		const auto to_base   = path_join(dest_path, path);

		// Make sure the destination path exists
		if (!files.empty() || copy_empty_dirs) {
			const auto error = MakeDirectoryTree(to_base);
			if (error != XCopyError::NoError) {
				return error;
			}
		}

		for (const auto& file : files) {
			if (CALLBACK_Idle() || has_ctrlc_been_pressed()) {
				return XCopyError::Interrupted;
			}

			const auto to_file = apply_pattern(file.name, dest_spec);
			if (DOS_FindDevice(to_file.c_str()) != DOS_DEVICES) {
				// The original XCOPY implementation doesn't
				// produce an error in this case, it just stops.
				WriteErr(MSG_Get("PROGRAM_XCOPY_RESERVED_DEVICE_DEST"));
				return XCopyError::NoError;
			}

			const auto from_path = path_join(from_base, file.name);
			const auto to_path   = path_join(to_base, to_file);
			if (from_path == to_path) {
				// The original XCOPY implementation doesn't
				// produce an error in this case, it just stops.
				WriteErr(MSG_Get("PROGRAM_XCOPY_COPY_ONTO_ITSELF"));
				return XCopyError::NoError;
			}

			uint16_t from_handle = {};
			if (!DOS_OpenFile(from_path.c_str(), OPEN_READ, &from_handle)) {
				WriteErr(doserr_msg_get(dos.errorcode),
				         file.name.c_str());
				return XCopyError::Failure;
			}

			if (!overwrite_all && DOS_FileExists(to_path.c_str())) {
				// Ask what to do if the file already exists
				WriteErr(MSG_Get("PROGRAM_XCOPY_OVERWRITE_PROMPT"),
				         to_path.c_str());

				// TODO: make PromptForLetter use WriteErr here
				const auto maybe_letter = PromptForLetter("yna", true);

				if (maybe_letter == Ascii::CtrlC) {
					DOS_CloseFile(from_handle);
					return XCopyError::Interrupted;
				}

				if (!maybe_letter || maybe_letter == 'n') {
					DOS_CloseFile(from_handle);
					continue;
				}

				overwrite_all = maybe_letter == 'a';
			} else {
				WriteOut("%s\n", file.name.c_str());
			}

			uint16_t to_handle = {};
			if (!DOS_CreateFile(to_path.c_str(),
			                    FatAttributeFlags::Archive,
			                    &to_handle)) {
				// Without this ternary it'll print "File not
				// found", which would be strange for a file we
				// tried to create.
				const auto msg = dos.errorcode == DOSERR_FILE_NOT_FOUND
				                       ? MSG_Get("PROGRAM_XCOPY_FILE_CREATION_ERROR")
				                       : doserr_msg_get(dos.errorcode);
				WriteErr(msg, file.name.c_str());
				DOS_CloseFile(from_handle);
				return XCopyError::Failure;
			}

			const auto error = CopyFile(from_handle,
			                            to_handle,
			                            file.name.c_str());

			if (error == XCopyError::NoError) {
				DOS_SetFileDate(to_handle, file.time, file.date);
			}

			DOS_CloseFile(from_handle);
			DOS_CloseFile(to_handle);

			if (error != XCopyError::NoError) {
				// Try deleting partial destination
				DOS_UnlinkFile(to_path.c_str());
				return error;
			}

			copied_count++;

			if (clear_archive_attribute) {
				FatAttributeFlags attr = file.attr;
				attr.archive           = false;
				if (!DOS_SetFileAttr(from_path.c_str(), attr)) {
					LOG_WARNING("XCOPY: Could not clear archive bit of '%s'",
					            from_path.c_str());
				}
			}
		}
	}

	return XCopyError::NoError;
}

void XCOPY::AddMessages()
{
	MSG_Add("PROGRAM_XCOPY_INVALID_PARAM_NUM", "Invalid number of parameters\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_SWITCH", "Invalid switch - /%s\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_PATH", "Invalid path\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_DRIVE", "Invalid drive specification\n");

	MSG_Add("PROGRAM_XCOPY_INVALID_DATE", "Invalid date\n");

	MSG_Add("PROGRAM_XCOPY_FILE_NOT_FOUND", "File not found - %s\n");

	MSG_Add("PROGRAM_XCOPY_PATH_NOT_FOUND", "Path not found - %s\n");

	MSG_Add("PROGRAM_XCOPY_FILE_CREATION_ERROR", "File creation error\n");

	MSG_Add("PROGRAM_XCOPY_UNABLE_CREATE_DIR", "Unable to create directory\n");

	MSG_Add("PROGRAM_XCOPY_TOO_MANY_OPEN_FILES", "Too many open files\n");

	MSG_Add("PROGRAM_XCOPY_LOCK_VIOLATION", "Lock violation - %s\n");

	MSG_Add("PROGRAM_XCOPY_ACCESS_DENIED", "Access denied - %s\n");

	MSG_Add("PROGRAM_XCOPY_GENERAL_FAILURE", "General failure - %s\n");

	MSG_Add("PROGRAM_XCOPY_INSUFFICIENT_DISK_SPACE", "Insufficient disk space\n");

	MSG_Add("PROGRAM_XCOPY_CYCLIC_COPY", "Cannot perform a cyclic copy\n");

	MSG_Add("PROGRAM_XCOPY_RESERVED_DEVICE_SOURCE",
	        "Cannot copy from a reserved device\n");

	MSG_Add("PROGRAM_XCOPY_RESERVED_DEVICE_DEST",
	        "Cannot copy to a reserved device\n");

	MSG_Add("PROGRAM_XCOPY_COPY_ONTO_ITSELF",
	        "File cannot be copied onto itself\n");

	MSG_Add("PROGRAM_XCOPY_READING_SOURCE", "Reading source file(s)...\n");

	MSG_Add("PROGRAM_XCOPY_OVERWRITE_PROMPT", "Overwrite %s (Yes/No/All)?");

	MSG_Add("PROGRAM_XCOPY_WAIT_PROMPT", "Press any key to begin copying file(s)");

	MSG_Add("PROGRAM_XCOPY_COPY_PROMPT", "%s (Y/N)? ");

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