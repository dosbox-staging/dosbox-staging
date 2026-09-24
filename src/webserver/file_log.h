// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_FILE_LOG_H
#define DOSBOX_WEBSERVER_FILE_LOG_H

#include <cstdint>
#include <optional>
#include <string>

// Logs the DOS file calls a program makes (open, create, close, read,
// write, seek) as JSON Lines, one call per line, including the file
// position and the memory address each read or write uses. Off unless
// the `webserver_file_log` setting names a log file.
namespace FileLog {

struct Call {
	uint8_t function                 = 0; // INT 21h AH
	uint8_t subfunction              = 0; // AL
	uint16_t handle                  = 0; // BX (DOS handle)
	uint16_t length                  = 0; // CX (bytes requested)
	uint32_t buffer                  = 0; // physical address of DS:DX
	std::string file                 = {};
	std::optional<uint32_t> position = {};
};

void Init(const std::string& path);

// Called by the INT 21h handler before and after it runs. Begin() returns
// nothing unless logging is on and the call is a file call.
std::optional<Call> Begin();
void End(const std::optional<Call>& call);

} // namespace FileLog

#endif // DOSBOX_WEBSERVER_FILE_LOG_H
