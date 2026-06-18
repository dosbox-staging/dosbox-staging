// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "file_walker.h"

#include <fstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

const std::unordered_set<std::string_view>& known_section_names()
{
	static const std::unordered_set<std::string_view> names = {
	        "sdl",        "dosbox",      "render", "composite", "voodoo",
	        "capture",    "reelmagic",   "cpu",    "mixer",     "midi",
	        "fluidsynth", "soundcanvas", "mt32",   "sblaster",  "gus",
	        "innovation", "speaker",     "imfc",   "joystick",  "serial",
	        "mouse",      "dos",         "ipx",    "ethernet",  "webserver",
	        "disknoise",  "log",         "debug",  "autoexec",
	};
	return names;
}

bool should_skip_dir(const std_fs::path& p)
{
	const auto name = p.filename().string();
	if (name.empty()) {
		return false;
	}
	return name.front() == '.';
}

void consider(const std_fs::path& p, std::vector<std_fs::path>& out)
{
	if (p.extension() != ".conf") {
		return;
	}
	if (!LooksLikeDosboxConfig(p)) {
		return;
	}
	out.push_back(p);
}

} // namespace

bool LooksLikeDosboxConfig(const std_fs::path& path)
{
	std::ifstream in(path);
	if (!in) {
		return false;
	}
	const auto& sections = known_section_names();

	std::string line;
	int lines_read              = 0;
	constexpr int MaxSniffLines = 100;
	while (lines_read < MaxSniffLines && std::getline(in, line)) {
		++lines_read;

		const auto first = line.find_first_not_of(" \t\r");
		if (first == std::string::npos) {
			continue;
		}
		if (line[first] != '[') {
			continue;
		}
		const auto close = line.find(']', first);
		if (close == std::string::npos) {
			continue;
		}
		std::string_view name(line.data() + first + 1, close - first - 1);
		while (!name.empty() &&
		       (name.front() == ' ' || name.front() == '\t')) {
			name.remove_prefix(1);
		}
		while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
			name.remove_suffix(1);
		}
		if (sections.count(name)) {
			return true;
		}
	}
	return false;
}

std::vector<std_fs::path> FindConfigFiles(const std_fs::path& root)
{
	std::vector<std_fs::path> out;
	std::error_code ec;

	if (!std_fs::exists(root, ec)) {
		return out;
	}

	if (std_fs::is_regular_file(root, ec)) {
		consider(root, out);
		return out;
	}
	if (!std_fs::is_directory(root, ec)) {
		return out;
	}

	using It        = std_fs::recursive_directory_iterator;
	const auto opts = std_fs::directory_options::skip_permission_denied;

	It it(root, opts, ec);
	const It end;
	while (!ec && it != end) {
		const auto& entry = *it;
		const auto& p     = entry.path();

		if (entry.is_symlink(ec)) {
			it.disable_recursion_pending();
			it.increment(ec);
			continue;
		}
		if (entry.is_directory(ec) && should_skip_dir(p)) {
			it.disable_recursion_pending();
			it.increment(ec);
			continue;
		}
		if (entry.is_regular_file(ec)) {
			consider(p, out);
		}
		it.increment(ec);
	}
	return out;
}

} // namespace ConfigMigrate
