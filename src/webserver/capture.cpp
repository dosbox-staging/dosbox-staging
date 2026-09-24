// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/capture.h"
#include "webserver.h"

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <thread>

#include "http/http.h"
#include "json/json.h"

#include "capture/capture.h"
#include "misc/std_filesystem.h"
#include "misc/support.h"

using json = nlohmann::json;
using httplib::Request, httplib::Response;

namespace Webserver {

void ScreenshotCommand::Execute()
{
	CAPTURE_RequestImage(type);
}

static CaptureType str_to_capture_type(const std::string& str)
{
	if (str.empty() || str == "raw") {
		return CaptureType::RawImage;
	}
	if (str == "upscaled") {
		return CaptureType::UpscaledImage;
	}
	if (str == "rendered") {
		return CaptureType::RenderedImage;
	}
	throw std::invalid_argument("type must be raw, upscaled or rendered");
}

// Captured images are named e.g. `image0001-raw.png`, `image0001.png`
// (upscaled) and `image0001-rendered.png`.
static bool is_image_of_type(const std::string& name, const CaptureType type)
{
	if (!name.starts_with("image") || !name.ends_with(".png")) {
		return false;
	}
	const auto is_raw      = name.ends_with("-raw.png");
	const auto is_rendered = name.ends_with("-rendered.png");

	switch (type) {
	case CaptureType::RawImage: return is_raw;
	case CaptureType::RenderedImage: return is_rendered;
	default: return !is_raw && !is_rendered;
	}
}

static std::set<std::string> list_images(const std_fs::path& dir,
                                         const CaptureType type)
{
	std::set<std::string> names = {};
	std::error_code ec          = {};

	for (const auto& entry : std_fs::directory_iterator(dir, ec)) {
		const auto name = entry.path().filename().string();
		if (is_image_of_type(name, type)) {
			names.insert(name);
		}
	}
	return names;
}

// The image saver threads write the PNG in place, so a file only counts as
// complete once it ends with the 12-byte IEND chunk.
static std::optional<std::string> read_complete_png(const std_fs::path& path)
{
	const auto file = make_fopen(path.string().c_str(), "rb");
	if (!file) {
		return {};
	}

	std::string data = {};
	char buf[64 * 1024];
	size_t num_read = 0;
	while ((num_read = fread(buf, 1, sizeof(buf), file.get())) > 0) {
		data.append(buf, num_read);
	}

	constexpr auto IendChunkSize = 12;
	if (data.size() < IendChunkSize ||
	    data.compare(data.size() - 8, 4, "IEND") != 0) {
		return {};
	}
	return data;
}

void ScreenshotCommand::Post(const Request& req, Response& res)
{
	const auto type = str_to_capture_type(req.get_param_value("type"));
	const auto dir  = generate_capture_filename(type, 0).parent_path();

	const auto existing = list_images(dir, type);

	ScreenshotCommand cmd(type);
	cmd.WaitForCompletion();

	using namespace std::chrono_literals;
	constexpr auto Timeout = 5s;

	const auto deadline = std::chrono::steady_clock::now() + Timeout;
	while (std::chrono::steady_clock::now() < deadline) {
		for (const auto& name : list_images(dir, type)) {
			if (existing.contains(name)) {
				continue;
			}
			const auto path = dir / name;
			const auto png  = read_complete_png(path);
			if (!png) {
				continue;
			}
			if (req.get_param_value("inline") == "1") {
				res.set_content(*png, "image/png");
			} else {
				json j;
				j["path"] = path.string();
				send_json(res, j);
			}
			return;
		}
		std::this_thread::sleep_for(20ms);
	}

	throw std::runtime_error("Timed out waiting for the screenshot");
}

} // namespace Webserver
