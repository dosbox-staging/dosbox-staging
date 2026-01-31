// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_MANAGER_H
#define DOSBOX_SHADER_MANAGER_H

#include <optional>
#include <string>
#include <utility>

#include "gui/private/common.h"
#include "shader_pass.h"

/*
 * Shader manager for loading shader and shader presets.
 *
 * Main duties:
 *
 * - loading shader sources, compiling them, and caching the results
 * - loading & parsing shader presets and caching the results
 * - generating an inventry of all available shaders
 */
class ShaderManager {
public:
	static ShaderManager& GetInstance()
	{
		static ShaderManager instance;
		return instance;
	}

	static void AddMessages();

	/*
	 * Generate a human-readable shader inventory message (one list element
	 * per line).
	 */
	std::deque<std::string> GenerateShaderInventoryMessage() const;

	std::optional<std::pair<ShaderInfo, std::string>> LoadShader(
	        const std::string& mapped_name);

	std::optional<std::pair<ShaderInfo, std::string>> LoadShader(
	        const std::string& shader_name, const std::string& extension);

	std::optional<ShaderPreset> LoadShaderPreset(
	        const ShaderDescriptor& descriptor,
	        const ShaderPreset& default_preset) const;

private:
	ShaderManager()  = default;
	~ShaderManager() = default;

	// prevent copying
	ShaderManager(const ShaderManager&) = delete;
	// prevent assignment
	ShaderManager& operator=(const ShaderManager&) = delete;


	std::optional<std::string> FindShaderAndReadSource(const std::string& shader_name);

	ShaderPreset ParseDefaultShaderPreset(const std::string& shader_name,
	                                      const std::string& shader_source) const;

	void SetShaderSetting(const std::string& name, const std::string& value,
	                      ShaderSettings& settings) const;

	std::optional<std::pair<std::string, float>> ParseParameterPragma(
	        const std::string& pragma_value) const;


};

#endif // DOSBOX_SHADER_MANAGER_H
