// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHADER_MANAGER_H
#define DOSBOX_SHADER_MANAGER_H

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "gui/private/common.h"
#include "shader.h"
#include "shader_common.h"

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

	std::optional<Shader> LoadShader(const std::string& shader_name);
	std::optional<Shader> ForceReloadShader(const std::string& shader_name);

	ShaderPreset LoadShaderPresetOrDefault(const ShaderDescriptor& descriptor);

	/*
	 * Generate a human-readable shader inventory message (one list element
	 * per line).
	 */
	std::deque<std::string> GenerateShaderInventoryMessage() const;

private:
	ShaderManager();
	~ShaderManager();

	// prevent copying
	ShaderManager(const ShaderManager&) = delete;
	// prevent assignment
	ShaderManager& operator=(const ShaderManager&) = delete;

	std::optional<Shader> LoadAndBuildShader(const std::string& shader_name);

	std::optional<std::string> FindShaderAndReadSource(const std::string& shader_name);

	std::optional<ShaderPreset> LoadShaderPreset(
	        const ShaderDescriptor& descriptor,
	        const ShaderPreset& default_preset) const;

	ShaderPreset ParseDefaultShaderPreset(const std::string& shader_name,
	                                      const std::string& shader_source) const;

	void SetShaderSetting(const std::string& name, const std::string& value,
	                      ShaderSettings& settings) const;

	std::optional<std::pair<std::string, float>> ParseParameterPragma(
	        const std::string& pragma_value) const;

	// Keys are the shader names including the path part but without the
	// .glsl file extension
	std::unordered_map<std::string, Shader> shader_cache = {};

	// Keys are the shader names including the path part but without the
	// .glsl file extension
	std::unordered_map<std::string, ShaderPreset> shader_preset_cache = {};
};

#endif // DOSBOX_SHADER_MANAGER_H
