// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOS_APPEND_H
#define DOS_APPEND_H

#include <string>
#include <vector>

namespace DOS_Append {

	// Parses and stores the active APPEND directories.
	// Splits the semicolon-delimited input string into distinct paths.
	void SetPaths(const std::string& path_string);

	// Checks if the APPEND service is currently running.
	bool IsActive();

	// Retrieves the active APPEND directories.
	std::vector<std::string> GetPaths();

	// Toggles whether the APPEND service intercepts executable lookups.
	// This represents the MS-DOS /X (or /X:ON / /X:OFF) switch behavior.
	void SetExecutableMode(bool enabled);

	// Checks if executable interception is currently enabled.
	bool IsExecutableMode();

	// The VFS Failure Hook template.
	// Iterates through the appended paths, securely concatenating the filename,
	// and invokes the provided native VFS callback (e.g. FileOpen, FindFirst).
	// Stops iteration and returns true immediately if the callback succeeds.
	template <typename Func>
	bool Resolve(const char* name, Func&& attempt_op) {
		if (!IsActive()) return false;
		for (const auto& dir : GetPaths()) {
			std::string try_path = dir;
			if (!try_path.empty() && try_path.back() != '\\') {
				try_path += '\\';
			}
			try_path += name;
			if (attempt_op(try_path)) {
				return true;
			}
		}
		return false;
	}

}

#endif