// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SDL_RENDERER_H
#define DOSBOX_SDL_RENDERER_H

#include "render_backend.h"

#include <string>

#include "gui/private/common.h"
#include "gui/render/render.h"

#include "dosbox_config.h"
#include "utils/rect.h"

// must be included after dosbox_config.h
#include <SDL3/SDL.h>

class SdlRenderer : public RenderBackend {

public:
	SdlRenderer(const int x, const int y, const int width, const int height,
	            const SDL_WindowFlags sdl_window_flags,
	            const std::string& render_driver,
	            TextureFilterMode texture_filter_mode);

	~SdlRenderer() override;

	SDL_Window* GetWindow() override;

	DosBox::Rect GetCanvasSizeInPixels() override;

	void NotifyViewportSizeChanged(const DosBox::Rect draw_rect_px) override;

	void NotifyRenderSizeChanged(const int new_render_width_px,
	                             const int new_render_height_px) override;

	void NotifyVideoModeChanged(const VideoMode& video_mode) override;

	SetShaderResult SetShader(const std::string& symbolic_shader_descriptor) override;

	void ForceReloadCurrentShader() override;

	ShaderInfo GetCurrentShaderInfo() override;
	ShaderPreset GetCurrentShaderPreset() override;

	std::string GetCurrentSymbolicShaderDescriptor() override;
	ShaderDescriptor GetCurrentShaderDescriptor() override;

	void UploadFrame(const uint32_t* pixels, const int width_px,
	                 const int height_px, const int pitch_bytes,
	                 const int first_row, const int num_rows,
	                 const bool double_width, const bool double_height,
	                 const VideoMode& video_mode) override;

	void PresentFrame() override;

	void SetVsync(const bool is_enabled) override;

	void SetColorSpace(const ColorSpace color_space) override;
	void SetImageAdjustmentSettings(const ImageAdjustmentSettings& settings) override;
	void EnableImageAdjustments(const bool enable) override;
	void SetDeditheringStrength(const float strength) override;

	RenderedImage ReadPixelsPostShader(const DosBox::Rect output_rect_px) override;

	uint32_t MakePixel(const uint8_t red, const uint8_t green,
	                   const uint8_t blue) override;

	// prevent copying
	SdlRenderer(const SdlRenderer&) = delete;
	// prevent assignment
	SdlRenderer& operator=(const SdlRenderer&) = delete;

private:
	bool InitRenderer(const std::string& render_driver);

	void MaybeRecreateTexture(const int width_px, const int height_px);

	SDL_WindowFlags OpenGlDriverCrashWorkaround(const std::string_view render_driver) const;

	SDL_Window* window     = {};
	SDL_Renderer* renderer = {};

	SDL_Texture* texture = {};

	TextureFilterMode texture_filter_mode = TextureFilterMode::Bilinear;
};

#endif // DOSBOX_SDL_RENDERER_H
