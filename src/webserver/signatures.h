// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_SIGNATURES_H
#define DOSBOX_WEBSERVER_SIGNATURES_H

#include <string>

// Watches emulated memory for signatures read from JSON files in a
// directory (the `webserver_signature_dir` setting), and logs each hit as
// a JSON line with a window of the memory around it. Two kinds:
//
// - "pattern": bytes (hex with ?? wildcards) or text appear somewhere new.
// - "watch": a known address (or a table: address + stride * n) changes.
//
// A hit can also request a screenshot or pause the emulator, so the
// moment can be captured whole. The first scan after loading is a silent
// baseline: only changes after it are reported.
namespace Signatures {

void Init(const std::string& dir, int interval_ms);

// Called from the emulation loop (also while paused); scans when the interval
// has passed.
void Tick();

// Re-reads the signature files and takes a new baseline. Returns how many
// signatures were loaded. Emulation thread only.
int Reload();

} // namespace Signatures

#endif // DOSBOX_WEBSERVER_SIGNATURES_H
