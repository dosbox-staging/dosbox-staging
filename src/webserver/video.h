// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_VIDEO_H
#define DOSBOX_WEBSERVER_VIDEO_H

#include "libs/http/http.h"

namespace Webserver {

struct VideoHandlers {
	static void GetFrame(const httplib::Request& req, httplib::Response& res);
	static void GetFrameInfo(const httplib::Request& req, httplib::Response& res);
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_VIDEO_H
