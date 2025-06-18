// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_BACKEND_H
#define DOSBOX_RENDER_BACKEND_H

class RenderBackend {

public:
	enum class Type { OpenGL, SdlTexture }

	virtual Init();
	virtual Deinit();

	virtual SetVsyncMode();
	virtual SetShader();
	virtual StartUpdate();

	virtual ~RenderBackend();

	// prevent copying
	RenderBackend(const RenderBackend&) = delete;
	// prevent assignment
	RenderBackend& operator=(const RenderBackend&) = delete;

private:

}

#endif // DOSBOX_RENDER_BACKEND_H
