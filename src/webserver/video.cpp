// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video.h"
#include "webserver.h"

#include "gui/render/render_shared.h"
#include "misc/rendered_image.h"
#include "misc/video.h"

#include "libs/json/json.h"

#include <jpeglib.h>
#include <png.h>

#include <cstring>
#include <vector>

using json = nlohmann::json;

namespace Webserver {

static std::vector<uint8_t> convert_to_rgb888(const RenderedImage& image)
{
	const auto w = image.params.width;
	const auto h = image.params.height;
	std::vector<uint8_t> rgb(w * h * 3);

	for (int y = 0; y < h; y++) {
		const auto* src_row = image.image_data + y * image.pitch;
		auto* dst_row       = rgb.data() + y * w * 3;

		switch (image.params.pixel_format) {
		case PixelFormat::Indexed8:
			for (int x = 0; x < w; x++) {
				const auto idx     = src_row[x];
				dst_row[x * 3 + 0] = image.palette[idx].red;
				dst_row[x * 3 + 1] = image.palette[idx].green;
				dst_row[x * 3 + 2] = image.palette[idx].blue;
			}
			break;
		case PixelFormat::BGRX32_ByteArray:
			for (int x = 0; x < w; x++) {
				dst_row[x * 3 + 0] = src_row[x * 4 + 2]; // R
				dst_row[x * 3 + 1] = src_row[x * 4 + 1]; // G
				dst_row[x * 3 + 2] = src_row[x * 4 + 0]; // B
			}
			break;
		case PixelFormat::BGR24_ByteArray:
			for (int x = 0; x < w; x++) {
				dst_row[x * 3 + 0] = src_row[x * 3 + 2]; // R
				dst_row[x * 3 + 1] = src_row[x * 3 + 1]; // G
				dst_row[x * 3 + 2] = src_row[x * 3 + 0]; // B
			}
			break;
		case PixelFormat::RGB555_Packed16:
			for (int x = 0; x < w; x++) {
				const auto px = reinterpret_cast<const uint16_t*>(
				        src_row)[x];
				dst_row[x * 3 + 0] = static_cast<uint8_t>(
				        ((px >> 10) & 0x1F) * 255 / 31);
				dst_row[x * 3 + 1] = static_cast<uint8_t>(
				        ((px >> 5) & 0x1F) * 255 / 31);
				dst_row[x * 3 + 2] = static_cast<uint8_t>(
				        (px & 0x1F) * 255 / 31);
			}
			break;
		case PixelFormat::RGB565_Packed16:
			for (int x = 0; x < w; x++) {
				const auto px = reinterpret_cast<const uint16_t*>(
				        src_row)[x];
				dst_row[x * 3 + 0] = static_cast<uint8_t>(
				        ((px >> 11) & 0x1F) * 255 / 31);
				dst_row[x * 3 + 1] = static_cast<uint8_t>(
				        ((px >> 5) & 0x3F) * 255 / 63);
				dst_row[x * 3 + 2] = static_cast<uint8_t>(
				        (px & 0x1F) * 255 / 31);
			}
			break;
		default: std::memset(dst_row, 0, w * 3); break;
		}
	}
	return rgb;
}

static std::string encode_jpeg(const RenderedImage& image, int quality = 80)
{
	auto rgb     = convert_to_rgb888(image);
	const auto w = image.params.width;
	const auto h = image.params.height;

	struct jpeg_compress_struct cinfo = {};
	struct jpeg_error_mgr jerr        = {};
	cinfo.err                         = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	unsigned char* buf     = nullptr;
	unsigned long buf_size = 0;
	jpeg_mem_dest(&cinfo, &buf, &buf_size);

	cinfo.image_width      = w;
	cinfo.image_height     = h;
	cinfo.input_components = 3;
	cinfo.in_color_space   = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);

	while (cinfo.next_scanline < cinfo.image_height) {
		auto* row = rgb.data() + cinfo.next_scanline * w * 3;
		jpeg_write_scanlines(&cinfo, &row, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	std::string result(reinterpret_cast<char*>(buf), buf_size);
	free(buf);
	return result;
}

static void write_png_to_buffer(png_structp png_ptr, png_bytep data, png_size_t length)
{
	auto* out = static_cast<std::string*>(png_get_io_ptr(png_ptr));
	out->append(reinterpret_cast<char*>(data), length);
}

static std::string encode_png(const RenderedImage& image)
{
	auto rgb     = convert_to_rgb888(image);
	const auto w = image.params.width;
	const auto h = image.params.height;

	std::string result;

	auto* png_ptr  = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                nullptr,
                                                nullptr,
                                                nullptr);
	auto* info_ptr = png_create_info_struct(png_ptr);

	png_set_write_fn(png_ptr, &result, write_png_to_buffer, nullptr);

	png_set_IHDR(png_ptr,
	             info_ptr,
	             w,
	             h,
	             8,
	             PNG_COLOR_TYPE_RGB,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

	png_set_compression_level(png_ptr, 1);
	png_write_info(png_ptr, info_ptr);

	for (int y = 0; y < h; y++) {
		auto* row = rgb.data() + y * w * 3;
		png_write_row(png_ptr, row);
	}

	png_write_end(png_ptr, nullptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	return result;
}

static std::string encode_raw(const RenderedImage& image)
{
	const auto w      = static_cast<uint32_t>(image.params.width);
	const auto h      = static_cast<uint32_t>(image.params.height);
	const auto pitch  = static_cast<int32_t>(image.pitch);
	const auto pf     = static_cast<uint8_t>(image.params.pixel_format);
	const auto is_pal = image.is_paletted();
	const uint16_t pal_count = is_pal ? 256 : 0;

	const auto data_size   = static_cast<size_t>(h * std::abs(pitch));
	const auto header_size = sizeof(w) + sizeof(h) + sizeof(pitch) +
	                         sizeof(pf) + sizeof(pal_count);
	const auto palette_size = static_cast<size_t>(pal_count * 3);

	std::string result;
	result.resize(header_size + palette_size + data_size);
	auto* p = result.data();

	std::memcpy(p, &w, sizeof(w));
	p += sizeof(w);
	std::memcpy(p, &h, sizeof(h));
	p += sizeof(h);
	std::memcpy(p, &pitch, sizeof(pitch));
	p += sizeof(pitch);
	std::memcpy(p, &pf, sizeof(pf));
	p += sizeof(pf);
	std::memcpy(p, &pal_count, sizeof(pal_count));
	p += sizeof(pal_count);

	if (is_pal) {
		for (int i = 0; i < pal_count; i++) {
			*p++ = image.palette[i].red;
			*p++ = image.palette[i].green;
			*p++ = image.palette[i].blue;
		}
	}

	std::memcpy(p, image.image_data, data_size);

	return result;
}

static const char* pixel_format_name(PixelFormat pf)
{
	switch (pf) {
	case PixelFormat::Indexed8: return "Indexed8";
	case PixelFormat::RGB555_Packed16: return "RGB555_Packed16";
	case PixelFormat::RGB565_Packed16: return "RGB565_Packed16";
	case PixelFormat::BGR24_ByteArray: return "BGR24_ByteArray";
	case PixelFormat::BGRX32_ByteArray: return "BGRX32_ByteArray";
	default: return "Unknown";
	}
}

void VideoHandlers::GetFrame(const httplib::Request& req, httplib::Response& res)
{
	if (!RENDER_HasSharedFrame()) {
		res.status = 503;
		json err;
		err["error"] = "No frame available yet";
		send_json(res, err);
		return;
	}

	auto frame        = RENDER_GetSharedFrame();
	const auto format = req.get_param_value("format");

	if (format == "png") {
		auto data = encode_png(frame);
		res.set_content(std::move(data), "image/png");
	} else if (format == "raw") {
		auto data = encode_raw(frame);
		res.set_content(std::move(data), "application/octet-stream");
	} else {
		auto data = encode_jpeg(frame);
		res.set_content(std::move(data), "image/jpeg");
	}

	frame.free();
}

void VideoHandlers::GetFrameInfo(const httplib::Request&, httplib::Response& res)
{
	if (!RENDER_HasSharedFrame()) {
		res.status = 503;
		json err;
		err["error"] = "No frame available yet";
		send_json(res, err);
		return;
	}

	auto frame = RENDER_GetSharedFrame();

	json j;
	j["width"]        = frame.params.width;
	j["height"]       = frame.params.height;
	j["pixel_format"] = pixel_format_name(frame.params.pixel_format);
	j["pitch"]        = frame.pitch;
	j["is_paletted"]  = frame.is_paletted();

	frame.free();

	send_json(res, j);
}

} // namespace Webserver
