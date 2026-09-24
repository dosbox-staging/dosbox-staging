// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "signatures.h"
#include "private/timestamp.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "json/json.h"

#include "capture/capture.h"
#include "dosbox.h"
#include "hardware/memory.h"
#include "misc/logging.h"
#include "misc/std_filesystem.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace Signatures {

namespace {

using json = nlohmann::json;

constexpr int Wildcard           = -1;
constexpr size_t MaxHitsPerScan  = 64;
constexpr uint32_t MaxWindow     = 1024;
constexpr uint32_t MaxWatchSize  = 256;
constexpr uint32_t MaxWatchCount = 4096;

struct Signature {
	std::string name = {};
	std::string kind = {}; // "pattern" or "watch"
	std::string file = {};

	std::vector<int> pattern    = {}; // byte values, or Wildcard
	uint32_t start              = 0;
	std::optional<uint32_t> end = {};

	uint32_t address = 0;
	uint32_t size    = 0;
	uint32_t stride  = 0;
	uint32_t count   = 1;

	uint32_t window = 32;
	bool screenshot = false;
	bool pause      = false;

	// Scan state; the first scan only records a baseline.
	bool has_baseline                        = false;
	std::set<uint32_t> present               = {};
	std::vector<std::vector<uint8_t>> values = {};
};

bool enabled                       = false;
std_fs::path directory             = {};
FILE_unique_ptr hit_log            = {};
std::vector<Signature> signatures  = {};
std::chrono::milliseconds interval = std::chrono::milliseconds(500);
std::chrono::steady_clock::time_point next_scan = {};
uint64_t sequence                               = 0;

// A hit that pauses and wants a screenshot gets it once the pause has taken
// hold, from the last frame drawn: that frame is the moment of the hit.
bool screenshot_when_paused = false;

uint32_t to_number(const json& value)
{
	if (value.is_number()) {
		return value.get<uint32_t>();
	}
	return static_cast<uint32_t>(
	        std::stoul(value.get<std::string>(), nullptr, 0));
}

std::vector<int> parse_hex(const std::string& text)
{
	std::vector<int> bytes = {};
	std::string token      = {};
	const auto flush       = [&] {
		if (token.empty()) {
			return;
		}
		bytes.push_back(token == "??" ? Wildcard
		                              : std::stoi(token, nullptr, 16));
		token.clear();
	};
	for (const auto c : text) {
		if (c == ' ' || c == '\t' || c == ',') {
			flush();
		} else {
			token += c;
		}
	}
	flush();
	return bytes;
}

Signature parse(const json& j, const std::string& file)
{
	Signature s = {};
	s.name      = j.at("name").get<std::string>();
	s.kind      = j.value("kind", std::string("pattern"));
	s.file      = file;
	s.window = std::min(j.contains("window") ? to_number(j["window"]) : 32u,
	                    MaxWindow);

	for (const auto& action : j.value("actions", json::array())) {
		const auto name = action.get<std::string>();
		s.screenshot |= (name == "screenshot");
		s.pause |= (name == "pause");
	}

	if (s.kind == "pattern") {
		if (j.contains("hex")) {
			s.pattern = parse_hex(j["hex"].get<std::string>());
		} else {
			for (const auto c : j.at("text").get<std::string>()) {
				s.pattern.push_back(static_cast<uint8_t>(c));
			}
		}
		const auto has_byte = std::any_of(s.pattern.begin(),
		                                  s.pattern.end(),
		                                  [](const int b) {
			                                  return b != Wildcard;
		                                  });
		if (!has_byte) {
			throw std::invalid_argument(
			        "a pattern needs at least one fixed byte");
		}
		if (j.contains("start")) {
			s.start = to_number(j["start"]);
		}
		if (j.contains("end")) {
			s.end = to_number(j["end"]);
		}
	} else if (s.kind == "watch") {
		s.address = to_number(j.at("address"));
		s.size    = to_number(j.at("size"));
		s.stride  = j.contains("stride") ? to_number(j["stride"]) : 0;
		s.count   = j.contains("count") ? to_number(j["count"]) : 1;
		if (s.size == 0 || s.size > MaxWatchSize || s.count == 0 ||
		    s.count > MaxWatchCount) {
			throw std::invalid_argument("watch size or count out of range");
		}
	} else {
		throw std::invalid_argument("kind must be pattern or watch");
	}
	return s;
}

std::string to_hex(const uint8_t* data, const size_t length)
{
	constexpr auto Digits = "0123456789ABCDEF";
	std::string text      = {};
	for (size_t i = 0; i < length; ++i) {
		if (i) {
			text += ' ';
		}
		text += Digits[data[i] >> 4];
		text += Digits[data[i] & 0xf];
	}
	return text;
}

uint32_t little_endian(const uint8_t* data, const uint32_t size)
{
	uint32_t value = 0;
	for (uint32_t i = std::min(size, 4u); i-- > 0;) {
		value = (value << 8) | data[i];
	}
	return value;
}

bool matches(const uint8_t* at, const std::vector<int>& pattern)
{
	for (size_t i = 0; i < pattern.size(); ++i) {
		if (pattern[i] != Wildcard && at[i] != pattern[i]) {
			return false;
		}
	}
	return true;
}

std::set<uint32_t> find_all(const Signature& s, const uint8_t* mem,
                            const uint32_t mem_size)
{
	std::set<uint32_t> hits = {};
	const auto end          = std::min(s.end.value_or(mem_size), mem_size);
	if (s.pattern.size() > end || s.start > end - s.pattern.size()) {
		return hits;
	}
	// Search on the first fixed byte, then check the whole pattern.
	const auto anchor      = static_cast<size_t>(std::distance(
	        s.pattern.begin(),
	        std::find_if(s.pattern.begin(), s.pattern.end(), [](const int b) {
		        return b != Wildcard;
	        })));
	const auto anchor_byte = static_cast<uint8_t>(s.pattern[anchor]);
	const auto last        = mem + end - s.pattern.size();
	for (auto p = mem + s.start + anchor; p <= last + anchor;) {
		p = std::find(p, last + anchor + 1, anchor_byte);
		if (p > last + anchor) {
			break;
		}
		const auto candidate = p - anchor;
		if (matches(candidate, s.pattern)) {
			hits.insert(static_cast<uint32_t>(candidate - mem));
		}
		++p;
	}
	return hits;
}

void log_hit(const Signature& s, json j, const uint32_t address,
             const uint8_t* mem, const uint32_t mem_size)
{
	const auto half  = s.window / 2;
	const auto first = address > half ? address - half : 0;
	const auto last  = std::min(mem_size, first + s.window);

	j["seq"]          = sequence++;
	j["t"]            = Webserver::UtcTimestamp();
	j["signature"]    = s.name;
	j["kind"]         = s.kind;
	j["address"]      = address;
	j["window_start"] = first;
	j["window"]       = to_hex(mem + first, last - first);

	const auto line = j.dump() + "\n";
	fwrite(line.data(), 1, line.size(), hit_log.get());
	fflush(hit_log.get());
	LOG_MSG("SIGNATURES: '%s' at 0x%06x", s.name.c_str(), address);
}

void scan()
{
	const auto mem      = GetMemBase();
	const auto mem_size = MEM_TotalPages() * MemPageSize;

	bool want_screenshot = false;
	bool want_pause      = false;
	const auto hit       = [&](const Signature& s) {
		want_screenshot |= s.screenshot;
		want_pause |= s.pause;
	};

	for (auto& s : signatures) {
		if (s.kind == "pattern") {
			auto now_present = find_all(s, mem, mem_size);
			if (s.has_baseline) {
				size_t logged = 0;
				for (const auto address : now_present) {
					if (!s.present.contains(address) &&
					    logged++ < MaxHitsPerScan) {
						log_hit(s,
						        json::object(),
						        address,
						        mem,
						        mem_size);
						hit(s);
					}
				}
			}
			s.present = std::move(now_present);
		} else {
			s.values.resize(s.count);
			for (uint32_t i = 0; i < s.count; ++i) {
				const auto address = s.address + i * s.stride;
				if (address + s.size > mem_size) {
					break;
				}
				const std::vector<uint8_t> now(mem + address,
				                               mem + address + s.size);
				auto& before = s.values[i];
				if (s.has_baseline && before != now) {
					json j     = json::object();
					j["index"] = i;
					j["old"]   = to_hex(before.data(),
					                    before.size());
					j["new"] = to_hex(now.data(), now.size());
					j["old_value"] = little_endian(before.data(),
					                               s.size);
					j["new_value"] = little_endian(now.data(),
					                               s.size);
					log_hit(s, j, address, mem, mem_size);
					hit(s);
				}
				before = now;
			}
		}
		s.has_baseline = true;
	}

	if (want_pause) {
		screenshot_when_paused = want_screenshot;
		DOSBOX_RequestUserPause();
	} else if (want_screenshot) {
		CAPTURE_RequestImage(CaptureType::RawImage);
	}
}

} // namespace

int Reload()
{
	signatures.clear();
	std::error_code ec              = {};
	std::vector<std_fs::path> files = {};
	for (const auto& entry : std_fs::directory_iterator(directory, ec)) {
		if (entry.path().extension() == ".json") {
			files.push_back(entry.path());
		}
	}
	std::sort(files.begin(), files.end());

	for (const auto& path : files) {
		try {
			const auto file = make_fopen(path.string().c_str(), "rb");
			std::string text = {};
			char buf[4096];
			size_t num_read = 0;
			while (file &&
			       (num_read = fread(buf, 1, sizeof(buf), file.get())) >
			               0) {
				text.append(buf, num_read);
			}
			const auto j    = json::parse(text);
			const auto name = path.filename().string();
			if (j.is_array()) {
				for (const auto& item : j) {
					signatures.push_back(parse(item, name));
				}
			} else {
				signatures.push_back(parse(j, name));
			}
		} catch (const std::exception& e) {
			LOG_WARNING("SIGNATURES: Skipping '%s': %s",
			            path.string().c_str(),
			            e.what());
		}
	}
	LOG_MSG("SIGNATURES: Loaded %d signatures from '%s'",
	        static_cast<int>(signatures.size()),
	        directory.string().c_str());
	return static_cast<int>(signatures.size());
}

void Init(const std::string& dir, const int interval_ms)
{
	if (dir.empty()) {
		return;
	}
	directory = dir;
	hit_log = make_fopen((directory / "hits.jsonl").string().c_str(), "a");
	if (!hit_log) {
		LOG_WARNING("SIGNATURES: Can't open '%s'",
		            (directory / "hits.jsonl").string().c_str());
		return;
	}
	interval = std::chrono::milliseconds(interval_ms);
	enabled  = true;
	Reload();
}

void Tick()
{
	if (!enabled) {
		return;
	}
	if (DOSBOX_IsPaused()) {
		if (screenshot_when_paused) {
			screenshot_when_paused = false;
			CAPTURE_RequestImage(CaptureType::RawImage);
		}
		return;
	}
	const auto now = std::chrono::steady_clock::now();
	if (now < next_scan) {
		return;
	}
	next_scan = now + interval;
	scan();
}

} // namespace Signatures
