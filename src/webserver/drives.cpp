// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/drives.h"

#include "bridge.h"
#include "webserver.h"

#include "http/http.h"
#include "json/json.h"

#include "ints/bios_disk.h"
#include "misc/support.h"

using json = nlohmann::json;

namespace Webserver {

void DriveCycleCommand::Execute()
{
	LOG_DEBUG("API: DriveCycleCommand(%c)", 'A' + drive);

	const auto cycle_result = BIOS_CycleDiskImageForDrive(drive);

	if (cycle_result.status == DriveCycleResult::Status::DriveNotMounted) {
		result = Result::NotMounted;
		return;
	}

	result        = Result::Success;
	current_index = cycle_result.current_index;
	total_images  = cycle_result.total_images;
	image_paths   = cycle_result.image_paths;
}

void DriveCycleCommand::Post(const httplib::Request& req, httplib::Response& res)
{
	const auto& letter_param = req.path_params.at("letter");

	if (letter_param.size() != 1 || !std::isalpha(letter_param[0])) {
		json j;
		j["error"] = "Invalid drive letter";
		res.status = httplib::StatusCode::BadRequest_400;
		send_json(res, j);
		return;
	}

	const auto drive = drive_index(letter_param[0]);

	DriveCycleCommand cmd(drive);
	cmd.WaitForCompletion();

	if (cmd.result == Result::NotMounted) {
		json j;
		j["error"] = std::string("Drive ") + drive_letter(drive) +
		             ": has no disk images attached";
		res.status = httplib::StatusCode::UnprocessableContent_422;
		send_json(res, j);
		return;
	}

	json j;
	j["drive"]                    = std::string(1, drive_letter(drive));
	j["current_disk_image_index"] = cmd.current_index;
	j["total_disk_images"]        = cmd.total_images;

	auto paths = json::array();
	for (const auto& path : cmd.image_paths) {
		paths.push_back(path);
	}
	j["disk_image_paths"] = paths;

	send_json(res, j);
}

} // namespace Webserver
