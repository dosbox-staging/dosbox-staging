/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_CAPTURE_IMAGE_H
#define DOSBOX_CAPTURE_IMAGE_H

#include <atomic>
#include <thread>

#include "image_scaler.h"
#include "render.h"
#include "rwqueue.h"

#if (C_SSHOT)
#include <png.h>
#include <zlib.h>

void capture_image(const RenderedImage_t image);

class ImageCapturer {
public:
	ImageCapturer() = default;
	~ImageCapturer();

	void Open();
	void Close();

	void CaptureImage(const RenderedImage_t image);

private:
	static constexpr auto MaxQueuedImages = 5;

	void SaveQueuedImages();
	void SavePng(const RenderedImage_t image);
	void SetPngCompressionsParams();
	void WritePngInfo(const uint16_t width, const uint16_t height,
	                  const bool is_paletted, const uint8_t* palette_data);

	ImageScaler image_scaler = {};

	RWQueue<RenderedImage_t> image_fifo{MaxQueuedImages};
	std::thread renderer = {};

	bool is_open = false;

	png_structp png_ptr    = nullptr;
	png_infop png_info_ptr = nullptr;
};

#endif

#endif
