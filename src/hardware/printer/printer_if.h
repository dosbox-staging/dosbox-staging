// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRINTER_IF
#define PRINTER_IF

#include "misc/types.h"

#include <cstdint>
#include <string>

// Which printer the user picked. Mirrors VirtualPrinter::PrinterModel but
// kept in plain enum form so the C-style cross-module API doesn't depend
// on the dot-matrix internals header.
enum class PrinterModelKind {
	None,
	EpsonDotMatrix,
	PostScript,
};

uint64_t PRINTER_ReadData(uint64_t port, uint64_t iolen);
void PRINTER_WriteData(uint64_t port, uint64_t val, uint64_t iolen);
uint64_t PRINTER_ReadStatus(uint64_t port, uint64_t iolen);
void PRINTER_WriteControl(uint64_t port, uint64_t val, uint64_t iolen);
uint64_t PRINTER_ReadControl(uint64_t port, uint64_t iolen);

// Set the printer config values that are read at lazy printer
// construction time inside PRINTER_WriteControl. page_width_in /
// page_height_in are inches (Real64). Dot-matrix-only settings are
// ignored when model == PostScript.
void PRINTER_Configure(PrinterModelKind model, uint16_t dpi,
                       double page_width_in, double page_height_in,
                       const std::string& output_dir, int timeout_ms);

// Trigger a form-feed (eject the current page). Called from the mapper
// (Ctrl+F2 by default) and from the inactivity-timeout PIC event.
void PRINTER_FormFeed(bool pressed);

// Release the lazily-created printer instance.
void PRINTER_Reset();
#endif
