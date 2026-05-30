// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_DRIVES_H
#define DOSBOX_WEBSERVER_DRIVES_H

#include "webserver/bridge.h"

#include "http/http.h"

#include <string>
#include <vector>

namespace Webserver {

class DriveCycleCommand : public Command {
public:
	explicit DriveCycleCommand(const int drive) : drive(drive) {}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	int drive = 0;

	enum class Result { Success, NotMounted };
	Result result                        = Result::NotMounted;
	int current_index                    = 0;
	int total_images                     = 0;
	std::vector<std::string> image_paths = {};
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_DRIVES_H
