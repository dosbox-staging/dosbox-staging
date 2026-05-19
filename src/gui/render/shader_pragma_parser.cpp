// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/shader_pragma_parser.h"

#include <algorithm>
#include <cassert>
#include <regex>
#include <unordered_map>
#include <utility>

#include "config/setup.h"
#include "misc/logging.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace ShaderPragma {

bool SetSetting(const std::string& name, const std::string& value,
                ShaderSettings& out_settings)
{
	assert(!name.empty());

	const auto maybe_bool = parse_bool_setting(value);
	if (!maybe_bool) {
		return false;
	}
	const auto bool_value = *maybe_bool;

	if (name == "force_single_scan") {
		out_settings.force_single_scan = bool_value;
		return true;
	}

	if (name == "force_no_pixel_doubling") {
		out_settings.force_no_pixel_doubling = bool_value;
		return true;
	}

	if (name == "linear_filtering") {
		out_settings.texture_filter_mode =
		        (bool_value ? TextureFilterMode::Bilinear
		                    : TextureFilterMode::NearestNeighbour);
		return true;
	}

	if (name == "float_output") {
		out_settings.float_output_texture = bool_value;
		return true;
	}

	return false;
}

namespace {

std::optional<std::pair<std::string, float>> parse_parameter_pragma(const std::string& pragma)
{
	assert(!pragma.empty());

	// Parameter format example:
	//
	//   #pragma parameter OUTPUT_GAMMA "OUTPUT GAMMA" 2.2 0.0 5.0 0.1
	//
	const auto parts = split(strip_prefix(pragma, "parameter"), "\"");
	if (parts.size() != 3) {
		return {};
	}
	// parts[0] - param (variable) name (OUTPUT_GAMMA)
	// parts[1] - display name (OUTPUT GAMMA)
	// parts[2] - values (4 space-separated floats)

	auto param_name = parts[0];
	trim(param_name);

	const auto params = split(parts[2]);
	if (params.size() != 4) {
		return {};
	}
	// params[0] - default value (2.2)
	// params[1] - min value     (0.0)
	// params[2] - max value     (5.0)
	// params[3] - value step    (0.1)

	const auto maybe_default_val = parse_float(params[0]);
	if (!maybe_default_val) {
		return {};
	}

	return {
	        {param_name, *maybe_default_val}
        };
}

std::optional<std::pair<std::string, std::string>> parse_setting_pragma(
        const std::string& pragma)
{
	// Shader setting format:
	//
	//   #pragma force_single_scan on
	//
	const auto parts = split(pragma);
	if (parts.size() != 2) {
		return {};
	}
	return {
	        {parts[0], parts[1]}
        };
}

std::optional<std::string> parse_name_pragma(const std::string& pragma)
{
	// Shader pass name format:
	//
	//   #pragma name Main_Pass1
	//
	const auto parts = split(strip_prefix(pragma, "name"));
	if (parts.size() != 1) {
		return {};
	}
	return parts[0];
}

std::optional<ShaderOutputSize> parse_output_size_pragma(const std::string& pragma)
{
	// Output size format:
	//
	//   #pragma output_size VideoMode
	//
	// Valid values are: Previous, Rendered, VideoMode, Viewport
	//
	const auto parts = split(strip_prefix(pragma, "output_size"));
	if (parts.size() != 1) {
		return {};
	}
	const auto& type = parts[0];

	using enum ShaderOutputSize;

	if (type == "Previous") {
		return Previous;

	} else if (type == "Rendered") {
		return Rendered;

	} else if (type == "VideoMode") {
		return VideoMode;

	} else if (type == "Viewport") {
		return Viewport;
	}

	return {};
}

std::optional<std::pair<int, std::string>> parse_input_pragma(const std::string& pragma)
{
	// Shader input format:
	//
	//   #pragma input0 Main_Pass2
	//   #pragma input1 Previous
	//   ...
	//
	const auto parts = split(strip_prefix(pragma, "input"));
	if (parts.size() != 2) {
		return {};
	}

	const auto& index_str = parts[0];

	if (auto maybe_int = parse_int(index_str); maybe_int) {
		const auto& name = parts[1];

		return {
		        {*maybe_int, name}
                };
	}
	return {};
}

} // namespace

std::optional<ParseResult> Parse(const std::string& shader_name,
                                 const std::string& shader_source)
{
	// We'll try to parse all pragmas and report all errors
	bool has_errors = false;

	ParseResult result = {};

	// The default preset has no name
	result.preset.name.clear();

	std::unordered_map<int, std::string> input_ids = {};
	auto highest_input_index                       = -1;

	try {
		const std::regex re("\\s*#pragma\\s+(.+)");

		std::sregex_iterator next(shader_source.begin(),
		                          shader_source.end(),
		                          re);
		const std::sregex_iterator end;

		while (next != end) {
			std::smatch match = *next;

			auto pragma = match[1].str();

			if (pragma.starts_with("parameter")) {
				if (const auto maybe_result = parse_parameter_pragma(
				            pragma);
				    maybe_result) {

					const auto& [param_name,
					             default_value] = *maybe_result;

					result.preset.params[param_name] = default_value;
				} else {
					LOG_ERR("RENDER: Invalid shader parameter pragma: '%s'",
					        pragma.c_str());
					has_errors = true;
				}

			} else if (pragma.starts_with("name")) {
				if (const auto maybe_name = parse_name_pragma(pragma);
				    maybe_name) {

					result.pass_name = *maybe_name;
				} else {
					LOG_ERR("RENDER: Invalid shader pass name pragma: '%s'",
					        pragma.c_str());
					has_errors = true;
				}

			} else if (pragma.starts_with("input")) {
				if (const auto maybe_result = parse_input_pragma(pragma);
				    maybe_result) {

					const auto& [input_index,
					             input_name] = *maybe_result;

					input_ids[input_index] = input_name;

					highest_input_index = std::max(
					        input_index, highest_input_index);
				} else {
					LOG_ERR("RENDER: Invalid input name pragma: '%s'",
					        pragma.c_str());
					has_errors = true;
				}

			} else if (pragma.starts_with("output_size")) {
				if (const auto maybe_output_size =
				            parse_output_size_pragma(pragma);
				    maybe_output_size) {

					result.output_size = *maybe_output_size;
				} else {
					LOG_ERR("RENDER: Invalid output size pragma: '%s'",
					        pragma.c_str());
					has_errors = true;
				}

			} else {
				bool parse_ok = false;

				if (const auto maybe_setting = parse_setting_pragma(
				            pragma);
				    maybe_setting) {

					const auto& [name, value] = *maybe_setting;

					parse_ok = SetSetting(name,
					                      value,
					                      result.preset.settings);
				}
				if (!parse_ok) {
					LOG_ERR("RENDER: Invalid shader setting pragma: '%s'",
					        pragma.c_str());
					has_errors = true;
				}
			}

			++next;
		}
	} catch (std::regex_error& e) {
		LOG_ERR("RENDER: Regex error while parsing shader '%s' for pragmas: %d",
		        shader_name.c_str(),
		        e.code());

		has_errors = true;
	}

	// Validate and store texture input IDs
	const auto num_inputs = check_cast<int>(input_ids.size());

	if (num_inputs != (highest_input_index + 1)) {
		LOG_ERR("RENDER: Shader input indices are invalid "
		        "(%d inputs but highest input index is %d)",
		        num_inputs,
		        highest_input_index);

		has_errors = true;
	}

	if (num_inputs == 0) {
		result.input_ids = {"Previous"};
	} else {
		for (auto i = 0; i <= highest_input_index; ++i) {
			result.input_ids.emplace_back(input_ids[i]);
		}
	}

	if (has_errors) {
		return {};
	}

	return result;
}

} // namespace ShaderPragma
