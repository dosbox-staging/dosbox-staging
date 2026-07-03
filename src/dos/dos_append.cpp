// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_append.h"
#include "dos.h"
#include "utils/string_utils.h"

namespace DOS_Append {

	static bool exec_mode_enabled = false;

	void SetPaths(const std::string& path_string) {
		dos.append_paths.clear();
		
		std::vector<std::string> items = split_with_empties(path_string, ';');
		remove_empties(items);

		for (auto& item : items) {
			trim(item);
			
			if (!item.empty()) {
				dos.append_paths.push_back(item);
			}
		}
	}

	bool IsActive() {
		return !dos.append_paths.empty();
	}

	std::vector<std::string> GetPaths() {
		return dos.append_paths;
	}

	void SetExecutableMode(bool enabled) {
		exec_mode_enabled = enabled;
	}

	bool IsExecutableMode() {
		return exec_mode_enabled;
	}

}