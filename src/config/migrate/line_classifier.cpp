// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "line_classifier.h"

#include <cctype>
#include <cstddef>

#include "utils/checks.h"

CHECK_NARROWING();

namespace ConfigMigrate {

namespace {

bool is_ident_char(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string_view strip_eol(std::string_view line, std::string& eol_out)
{
	if (line.ends_with("\r\n")) {
		eol_out = "\r\n";
		line.remove_suffix(2);
	} else if (line.ends_with("\n")) {
		eol_out = "\n";
		line.remove_suffix(1);
	} else if (line.ends_with("\r")) {
		eol_out = "\r";
		line.remove_suffix(1);
	} else {
		eol_out.clear();
	}
	return line;
}

void trim_inplace(std::string_view& sv)
{
	while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) {
		sv.remove_prefix(1);
	}
	while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t')) {
		sv.remove_suffix(1);
	}
}

// Try to match `<identifier> =` starting at scan_start in body. On success,
// fill key_start_out, key_end_out, eq_out and return true.
bool looks_like_setting(std::string_view body, size_t scan_start,
                        size_t& key_start_out, size_t& key_end_out, size_t& eq_out)
{
	auto i = body.find_first_not_of(" \t", scan_start);
	if (i == std::string_view::npos) {
		return false;
	}
	if (!is_ident_char(body[i])) {
		return false;
	}
	key_start_out = i;
	while (i < body.size() && is_ident_char(body[i])) {
		++i;
	}
	key_end_out = i;

	while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) {
		++i;
	}
	if (i >= body.size() || body[i] != '=') {
		return false;
	}
	eq_out = i;
	return true;
}

} // namespace

ClassifiedLine Classify(std::string_view line_with_eol, bool in_autoexec)
{
	ClassifiedLine out = {};

	auto body = strip_eol(line_with_eol, out.eol);
	out.raw   = std::string(body);

	const auto first_nws = body.find_first_not_of(" \t");
	if (first_nws == std::string_view::npos) {
		out.kind = LineKind::Blank;
		return out;
	}

	const char first_char = body[first_nws];

	// Section header
	if (first_char == '[') {
		const auto close = body.find(']', first_nws);
		if (close != std::string_view::npos) {
			out.kind = LineKind::SectionHeader;
			auto name = body.substr(first_nws + 1, close - first_nws - 1);
			trim_inplace(name);
			out.section = std::string(name);
			return out;
		}
		out.kind = LineKind::Unknown;
		return out;
	}

	// Comment line: distinguish plain comment from commented-out setting
	if (first_char == '#' || first_char == ';') {
		size_t key_start = 0;
		size_t key_end   = 0;
		size_t eq        = 0;
		if (looks_like_setting(body, first_nws + 1, key_start, key_end, eq)) {
			out.kind = LineKind::CommentedOutSetting;
			out.key  = std::string(
                                body.substr(key_start, key_end - key_start));
			auto value_view = body.substr(eq + 1);
			trim_inplace(value_view);
			out.value = std::string(value_view);
		} else {
			out.kind = LineKind::Comment;
		}
		return out;
	}

	// Inside [autoexec] / DOS-command context: anything not handled above
	// is a DOS command (the rewriter will tokenise it later).
	if (in_autoexec) {
		out.kind = LineKind::DosCommand;
		return out;
	}

	// INI setting line
	size_t key_start = 0;
	size_t key_end   = 0;
	size_t eq        = 0;
	if (looks_like_setting(body, first_nws, key_start, key_end, eq)) {
		out.kind = LineKind::Setting;
		out.key = std::string(body.substr(key_start, key_end - key_start));

		auto rest = body.substr(eq + 1);

		// Find inline trailing comment (simple version — does not
		// handle quoted '#' inside values; revisit when needed).
		const auto hash        = rest.find('#');
		const auto semi        = rest.find(';');
		const auto comment_pos = std::min(hash, semi);

		std::string_view value_view;
		if (comment_pos != std::string_view::npos) {
			value_view = rest.substr(0, comment_pos);
			out.trailing_comment = std::string(rest.substr(comment_pos));
		} else {
			value_view = rest;
		}
		trim_inplace(value_view);
		out.value = std::string(value_view);
		return out;
	}

	out.kind = LineKind::Unknown;
	return out;
}

} // namespace ConfigMigrate
