/*
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

ShaderManager::ShaderManager() noexcept
{
	static const auto cga_shader_names = {"crt/cga-1080p",
	                                      "crt/cga-1440p",
	                                      "crt/cga-4k"};

	static const auto ega_shader_names = {"crt/ega-1080p",
	                                      "crt/ega-1440p",
	                                      "crt/ega-4k"};

	static const auto vga_shader_names = {"crt/vga-1080p",
	                                      "crt/vga-1440p",
	                                      "crt/vga-4k"};

	static const auto composite_shader_names = {"crt/composite-1080p",
	                                            "crt/composite-1440p",
	                                            "crt/composite-4k"};

	static const auto monochrome_shader_names = {"crt/monochrome"};

	auto build_set = [](const std::vector<const char*> shader_names) {
		std::string source = {};

		std::vector<ShaderInfo> info = {};
		for (const char* name : shader_names) {
			if (!(read_shader_source(name, source))) {
				LOG_ERR("RENDER: Cannot load built-in shader '%s'",
				        name);
			}
			const auto settings = parse_shader_settings(name, source);
			info.push_back({name, settings});
		}

		auto compare = [](const ShaderInfo& a, const ShaderInfo& b) {
			const auto fa = a.settings.min_vertical_scale_factor;
			const auto fb = b.settings.min_vertical_scale_factor;
			return (fa == fb) ? (a.name < b.name) : (fa > fb);
		};
		std::sort(info.begin(), info.end(), compare);

		return info;
	};

	const auto start = high_resolution_clock::now();

	//	shader_set.monochrome = build_set(monochrome_shader_names);
	//	shader_set.composite  = build_set(composite_shader_names);
	//	shader_set.cga        = build_set(cga_shader_names);
	shader_set.ega = build_set(ega_shader_names);
	//	shader_set.vga        = build_set(vga_shader_names);

	const auto stop     = high_resolution_clock::now();
	const auto duration = duration_cast<milliseconds>(stop - start);
	LOG_MSG("RENDER: Building shader sets took %lld ms", duration.count());
}

ShaderManager::~ShaderManager() noexcept {}

void ShaderManager::NotifyGlshaderSetting(const std::string& setting)
{
	if (setting == AutoGraphicsStandardShaderName) {
		mode = ShaderMode::AutoGraphicsStandard;
	} else if (setting == AutoMachineShaderName) {
		mode = ShaderMode::AutoMachine;
	} else {
		mode                   = ShaderMode::Normal;
		this->glshader_setting = setting;
	}
}

void ShaderManager::NotifyRenderParameters(const uint16_t canvas_width,
                                           const uint16_t canvas_height,
                                           const uint16_t draw_width,
                                           const uint16_t draw_height,
                                           const Fraction& render_pixel_aspect_ratio,
                                           const VideoMode& video_mode)
{
	LOG_WARNING("-------------------------------------------------");
	LOG_WARNING("####### canvas_width: %d, height: %d", canvas_width, canvas_height);
	LOG_WARNING("####### draw_width: %d, height: %d", draw_width, draw_height);

	const auto viewport = GFX_CalcViewport(canvas_width,
	                                       canvas_height,
	                                       draw_width,
	                                       draw_height,
	                                       render_pixel_aspect_ratio);

	vertical_scale_factor = static_cast<double>(viewport.h) / draw_height;
	LOG_WARNING("####### vertical_scale_factor: %g", vertical_scale_factor);

	this->video_mode = video_mode;
}

std::vector<ShaderInfo>& ShaderManager::GetShaderSetForGraphicsStandard(
        const VideoMode& video_mode)
{
	if (video_mode.color_depth == ColorDepth::Monochrome) {
		return shader_set.monochrome;
	}
	if (video_mode.color_depth == ColorDepth::Composite) {
		return shader_set.composite;
	}
	switch (video_mode.graphics_standard) {
	case GraphicsStandard::Hercules: return shader_set.monochrome;
	case GraphicsStandard::Cga: return shader_set.cga;
	case GraphicsStandard::Pcjr:
	case GraphicsStandard::Tga:
	case GraphicsStandard::Ega: return shader_set.ega;
	case GraphicsStandard::Vga:
	case GraphicsStandard::Svga:
	case GraphicsStandard::Vesa: return shader_set.vga;
	}
}

std::vector<ShaderInfo>& ShaderManager::GetShaderSetForMachineType(
        const MachineType machine_type, const VideoMode& video_mode)
{
	if (video_mode.color_depth == ColorDepth::Composite) {
		return shader_set.composite;
	}
	switch (machine_type) {
	case MCH_HERC: return shader_set.monochrome;
	case MCH_CGA: return shader_set.cga;
	case MCH_TANDY: return shader_set.ega;
	case MCH_PCJR: return shader_set.ega;
	case MCH_EGA: return shader_set.ega;
	case MCH_VGA: return shader_set.vga;
	};
}

std::string ShaderManager::GetCurrentShaderName()
{
	if (mode == ShaderMode::Normal) {
		return glshader_setting;
	}

	auto get_shader_set = [&]() {
		switch (mode) {
		case ShaderMode::AutoGraphicsStandard:
			return GetShaderSetForGraphicsStandard(video_mode);
		case ShaderMode::AutoMachine:
			return GetShaderSetForMachineType(machine, video_mode);
		case ShaderMode::Normal: assert(false);
		}
	};
	const auto shader_set = get_shader_set();

	auto find_best_shader_name = [&]() -> std::string {
		for (auto shader : shader_set) {
			if (vertical_scale_factor >=
			    shader.settings.min_vertical_scale_factor) {
				return shader.name;
			}
		}
		return "interpolation/sharp";
	};
	const auto shader_name = find_best_shader_name();
	LOG_WARNING(">>>>>>> shader_name: %s", shader_name.c_str());

	return shader_name;
}

