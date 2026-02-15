// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver.h"
#include "bridge.h"
#include "cpu.h"
#include "dos.h"
#include "memory.h"

#include <string>
#include <thread>

#include "libs/http/http.h"
#include "libs/json/json.h"

#include "config/config.h"
#include "dosbox.h"
#include "misc/cross.h"
#include "misc/logging.h"
#include "misc/support.h"

using json = nlohmann::json;

namespace Webserver {

void send_json(httplib::Response& res, const nlohmann::json& j)
{
	res.set_content(j.dump(2), "application/json");
}

static void error_handler(const httplib::Request&, httplib::Response& res,
                          std::exception_ptr ep)
{
	json j;
	std::string msg;
	try {
		if (ep) {
			std::rethrow_exception(ep);
		}
	} catch (const std::exception& e) {
		msg = e.what();
	} catch (...) {
		msg = "Unknown error";
	}
	j["error"] = msg;
	res.status = httplib::StatusCode::InternalServerError_500;
	send_json(res, j);
}

static httplib::Server server;

static void setup_api_handlers()
{
	server.Get("/api/cpu", CpuInfoCommand::Get);
	server.Get("/api/memory/:offset/:len", ReadMemCommand::Get);
	server.Get("/api/memory/:segment/:offset/:len", ReadMemCommand::Get);
	server.Put("/api/memory/:offset", WriteMemCommand::Put);
	server.Put("/api/memory/:segment/:offset", WriteMemCommand::Put);
	server.Post("/api/memory/allocate", AllocMemoryCommand::Post);
	server.Post("/api/memory/free", FreeMemoryCommand::Post);
	server.Get("/api/dos", DosInfoCommand::Get);
}

static void run(std::string addr, int port)
{
	const auto resource_home = get_resource_path("webserver").string();
	const auto config_home = (get_config_dir() / DefaultWebserverDir).string();
	server.set_mount_point("/", config_home);
	server.set_mount_point("/", resource_home);

	setup_api_handlers();
	server.set_exception_handler(error_handler);
	server.Get("/api/info", [=](auto, auto& res) {
		json j;
		j["configHome"]      = get_config_dir();
		j["configWebserver"] = config_home;
		j["version"]         = DOSBOX_GetDetailedVersion();
		send_json(res, j);
	});

	LOG_INFO("WEBSERVER: Starting HTTP REST API on http://%s:%d",
	         addr.c_str(),
	         port);
	LOG_INFO("WEBSERVER: Using document root directory '%s'",
	         config_home.c_str());
	auto ok = server.listen(addr, port);
	if (!ok) {
		LOG_WARNING("WEBSERVER: Failed to bind to %s:%d", addr.c_str(), port);
	}
}

static void init_config_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto enabled = section.AddBool("webserver_enabled", OnlyAtStart, true);
	enabled->SetHelp(
	        "Enable the HTTP REST API that exposes internal state and memory.\n"
	        "Open [color=blue]http://localhost:8080[reset] (or configured port) to show documentation.");

	auto bind_ip = section.AddString("webserver_bind_address",
	                                 OnlyAtStart,
	                                 "127.0.0.1");
	bind_ip->SetHelp(
	        "Bind to the given IP address. This API gives full control over DOSBox, do not\n"
	        "ever expose this to untrusted hosts.\n"
	        "\n"
	        "By default only local connections are allowed.");

	auto bind_port = section.AddInt("webserver_port", OnlyAtStart, 8080);
	bind_port->SetMinMax(1, 0xFFFF);
	bind_port->SetHelp("TCP port to bind to.");
}

} // namespace Webserver

void WEBSERVER_Init()
{
	auto section = get_section("webserver");
	if (section->GetBool("webserver_enabled")) {
		auto addr = section->GetString("webserver_bind_address");
		auto port = section->GetInt("webserver_port");
		std::thread thread(Webserver::run, addr, port);
		thread.detach();
	}
}

void WEBSERVER_Destroy()
{
	Webserver::server.stop();
}

void WEBSERVER_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);
	auto section = conf->AddSection("webserver");
	Webserver::init_config_settings(*section);
}
