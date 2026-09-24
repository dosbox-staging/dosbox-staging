// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_CAPTURE_H
#define DOSBOX_WEBSERVER_CAPTURE_H

#include "webserver/bridge.h"

#include "http/http.h"

#include "capture/capture.h"

namespace Webserver {

class ScreenshotCommand : public Command {
public:
	explicit ScreenshotCommand(const CaptureType type) : type(type) {}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	CaptureType type = {};
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_CAPTURE_H
