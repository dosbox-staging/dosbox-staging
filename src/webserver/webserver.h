// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_H
#define DOSBOX_WEBSERVER_H

#include <charconv>
#include <functional>
#include <limits>
#include <string>

#include <libs/http/http.h>
#include <libs/json/json.h>

#include "config/config.h"

namespace Webserver {

constexpr auto TypeJson = "application/json";
constexpr auto TypeBinary = "application/octet-stream";

enum class Source {
	Param, // get_param_value
	Path,  // get_path_value
	Header // get_header_value
};

template <typename T>
static T num_param(const httplib::Request& req, Source src, const std::string& name,
                   const T min = std::numeric_limits<T>::lowest(),
                   const T max = std::numeric_limits<T>::max())
{
	std::string str;
	if (src == Source::Param) {
		str = req.get_param_value(name);
	} else if (src == Source::Path) {
		str = req.path_params.at(name);
	} else if (src == Source::Header) {
		str = req.get_header_value(name);
	}

	if (str.empty()) {
		throw std::invalid_argument(
		        "Missing or empty required parameter: " + name);
	}

	T value           = 0;
	int base          = 10;
	const char* first = str.data();
	const char* last  = str.data() + str.size();
	if (str.size() >= 2 && str[0] == '0' && std::tolower(str[1]) == 'x') {
		first += 2;
		base = 16;
	}

	auto [ptr, ec] = std::from_chars(first, last, value, base);
	if (ec == std::errc::invalid_argument || ptr != str.data() + str.size()) {
		throw std::invalid_argument("Invalid argument for " + name +
		                            ": " + str);
	} else if (ec == std::errc::result_out_of_range || value < min ||
	           value > max) {
		throw std::invalid_argument("Invalid argument for " + name +
		                            ": " + str + " (out of range)");
	}
	return value;
}

void send_json(httplib::Response& res, const nlohmann::json& j);

} // namespace Webserver

void WEBSERVER_Init();
void WEBSERVER_Destroy();
void WEBSERVER_AddConfigSection(const ConfigPtr& conf);

#endif // DOSBOX_WEBSERVER_H
