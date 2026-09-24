// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_TIMESTAMP_H
#define DOSBOX_WEBSERVER_TIMESTAMP_H

#include <chrono>
#include <string>

namespace Webserver {

// UTC, e.g. 2026-09-24T15:41:33.125Z. Built by hand: std::format's chrono
// support needs a newer macOS than the deployment target.
inline std::string UtcTimestamp()
{
	using namespace std::chrono;
	const auto now  = floor<milliseconds>(system_clock::now());
	const auto day  = floor<days>(now);
	const auto date = year_month_day{day};
	const hh_mm_ss time{now - day};

	const auto pad = [](const int value, const size_t width) {
		auto text = std::to_string(value);
		return std::string(width > text.size() ? width - text.size() : 0,
		                   '0') +
		       text;
	};
	return pad(static_cast<int>(date.year()), 4) + "-" +
	       pad(static_cast<int>(static_cast<unsigned>(date.month())), 2) +
	       "-" + pad(static_cast<int>(static_cast<unsigned>(date.day())), 2) +
	       "T" + pad(static_cast<int>(time.hours().count()), 2) + ":" +
	       pad(static_cast<int>(time.minutes().count()), 2) + ":" +
	       pad(static_cast<int>(time.seconds().count()), 2) + "." +
	       pad(static_cast<int>(time.subseconds().count()), 3) + "Z";
}

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_TIMESTAMP_H
