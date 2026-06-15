// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRINTER_IF
#define PRINTER_IF

#include "misc/types.h"

#include <cstdint>

uint64_t PRINTER_ReadData(uint64_t port, uint64_t iolen);
void PRINTER_WriteData(uint64_t port, uint64_t val, uint64_t iolen);
uint64_t PRINTER_ReadStatus(uint64_t port, uint64_t iolen);
void PRINTER_WriteControl(uint64_t port, uint64_t val, uint64_t iolen);
uint64_t PRINTER_ReadControl(uint64_t port, uint64_t iolen);

// Set the printer config values that are read at lazy Printer
// construction time inside PRINTER_WriteControl. The strings must outlive
// the printer instance (i.e. the caller owns the buffers).
void PRINTER_Configure(uint16_t dpi, uint16_t width, uint16_t height,
                       const char* docpath, const char* output_format,
                       bool multipage, int timeout_ms);

// Trigger a form-feed (eject the current page). Called from the mapper
// (Ctrl+F2 by default) and from the inactivity-timeout PIC event.
void PRINTER_FormFeed(bool pressed);

// Release the lazily-created Printer instance (closes any open multipage
// document, flushes the final page).
void PRINTER_Reset();
#endif
