// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_IF
#define DOSBOX_PRINTER_IF

#include "misc/types.h"

#include <cstdint>

// Which printer the user picked. Mirrors VirtualPrinter::PrinterModel but
// kept in plain enum form so the C-style cross-module API doesn't depend
// on the dot-matrix internals header.
//
// EpsonDotMatrix24Pin is the 24-pin LQ family (default, ESC/P + ESC/P 2).
// EpsonDotMatrix9Pin is the 9-pin FX/LX family (older ESC/P only -- has
// different line-spacing divisors and lacks several ESC/P 2 commands;
// see Printer::pins branches in printer_dispatch.cpp).
enum class PrinterModelKind {
	None,
	EpsonDotMatrix9Pin,
	EpsonDotMatrix24Pin,
	PostScript,
	Passthrough,
};

uint64_t PRINTER_ReadData(const uint64_t port, const uint64_t iolen);
void PRINTER_WriteData(const uint64_t port, const uint64_t val, const uint64_t iolen);

uint64_t PRINTER_ReadStatus(const uint64_t port, const uint64_t iolen);

void PRINTER_WriteControl(const uint64_t port, const uint64_t val,
                          const uint64_t iolen);
uint64_t PRINTER_ReadControl(const uint64_t port, const uint64_t iolen);

// Set the printer config values that are read at lazy printer
// construction time inside PRINTER_WriteControl. page_width_in /
// page_height_in are inches (double). Dot-matrix-only settings are
// ignored when model is PostScript.
void PRINTER_Configure(const PrinterModelKind model, const int dpi,
                       const double page_width_in, const double page_height_in,
                       const int timeout_ms);

// Trigger a form-feed (eject the current page). Called from the mapper
// (Ctrl+F2 by default) and from the inactivity-timeout PIC event.
void PRINTER_FormFeed(bool pressed);

// Release the lazily-created printer instance.
void PRINTER_Reset();

#endif // DOSBOX_PRINTER_IF
