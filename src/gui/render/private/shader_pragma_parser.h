// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_SHADER_PRAGMA_PARSER_H
#define DOSBOX_PRIVATE_SHADER_PRAGMA_PARSER_H

#include <optional>
#include <string>
#include <vector>

#include "shader_common.h"

namespace ShaderPragma {

struct ParseResult {
	ShaderPreset preset                           = {};
	std::string pass_name                         = {};
	std::vector<std::string> input_ids            = {};
	std::vector<TextureWrapMode> input_wrap_modes = {};
	ShaderOutputSize output_size                  = {};
};

// Parse all `#pragma` directives from the GLSL source of a shader. Returns
// the populated result on success, or an empty optional if any pragma
// failed to parse (all errors are logged via `LOG_ERR`).
std::optional<ParseResult> Parse(const std::string& shader_name,
                                 const std::string& shader_source);

// Apply a single named shader setting (e.g. "linear_filtering", "off") to
// `out_settings`. Used for both `#pragma` parsing and `[settings]` INI
// section parsing. Returns false on unknown setting name or invalid value.
bool SetSetting(const std::string& name, const std::string& value,
                ShaderSettings& out_settings);

} // namespace ShaderPragma

#endif // DOSBOX_PRIVATE_SHADER_PRAGMA_PARSER_H
