
#include "webserver.h"
#include "bridge.h"
#include "cpu.h"
#include "dos.h"
#include "memory.h"

#include <format>
#include <string>
#include <thread>

#include "libs/http/httplib.h"
#include "libs/json/json.h"

#include "config/config.h"
#include "dosbox.h"
#include "misc/cross.h"
#include "misc/logging.h"

using json = nlohmann::json;

namespace Webserver {

static void error_handler(const httplib::Request&, httplib::Response& res,
                          std::exception_ptr ep)
{
	constexpr std::string_view fmt = "<h1>Internal server error (500)</h1><p>{}</p>";
	std::string body;
	try {
		if (ep) {
			std::rethrow_exception(ep);
		}
	} catch (const std::exception& e) {
		body = std::format(fmt, e.what());
	} catch (...) {
		body = std::format(fmt, "Unknown error");
	}
	res.set_content(body, "text/html");
	res.status = httplib::StatusCode::InternalServerError_500;
}

static httplib::Server server;

static void run(std::string addr, int port)
{
	const auto& file = get_config_dir() / DefaultWebserverDir;
	server.set_mount_point("/", file.string());
	server.Get("/api/cpu", CpuInfoCommand::Get);
	server.Get("/api/memory/:offset/:len", ReadMemCommand::Get);
	server.Get("/api/memory/:segment/:offset/:len", ReadMemCommand::Get);
	server.Put("/api/memory/:offset", WriteMemCommand::Put);
	server.Put("/api/memory/:segment/:offset", WriteMemCommand::Put);
	server.Post("/api/memory/allocate", AllocMemoryCommand::Post);
	server.Post("/api/memory/free", FreeMemoryCommand::Post);
	server.Get("/api/pointers", DosInfoCommand::Get);
	server.set_exception_handler(error_handler);
	LOG_INFO("WEBSERVER: Starting HTTP REST API on http://%s:%d",
	         addr.c_str(),
	         port);
	auto ok = server.listen(addr, port);
	if (!ok) {
		LOG_WARNING("WEBSERVER: Failed to bind to %s:%d", addr.c_str(), port);
	}
}

static void init_config_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;
	auto enabled = section.AddBool("enabled", OnlyAtStart, true);
	enabled->SetHelp(
	        "Enable an HTTP REST API that exposes internal state and memory.\n"
	        "See " +
	        (get_config_dir() / DefaultWebserverDir / "README.md").string() +
	        " for details.\n"
	        "Note: This requires core = normal at the moment.");
	auto bind_ip = section.AddString("bind", OnlyAtStart, "127.0.0.1");
	bind_ip->SetHelp(
	        "Bind to the given IP address. This API gives full control over "
	        "DOSBox, do not ever expose this to untrusted hosts. By default "
	        "only local connections are allowed.");
	auto bind_port = section.AddInt("port", OnlyAtStart, 8080);
	bind_port->SetMinMax(1, 0xFFFF);
	bind_port->SetHelp("TCP port to bind to.");
}

} // namespace Webserver

void WEBSERVER_Init()
{
	auto section = get_section("webserver");
	if (section->GetBool("enabled")) {
		auto addr = section->GetString("bind");
		auto port = section->GetInt("port");
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
