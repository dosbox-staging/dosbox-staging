// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRINTER_IF
#define DOSBOX_PRINTER_IF

#include "misc/types.h"

#include <cstdint>

uint64_t PRINTER_readdata(uint64_t port, uint64_t iolen);
void PRINTER_writedata(uint64_t port, uint64_t val, uint64_t iolen);
uint64_t PRINTER_readstatus(uint64_t port, uint64_t iolen);
void PRINTER_writecontrol(uint64_t port, uint64_t val, uint64_t iolen);
uint64_t PRINTER_readcontrol(uint64_t port, uint64_t iolen);

bool PRINTER_isInited();

// Set the printer config values that are read at lazy CPrinter
// construction time inside PRINTER_writecontrol. The strings must outlive
// the printer instance (i.e. the caller owns the buffers).
void PRINTER_Configure(uint16_t dpi, uint16_t width, uint16_t height,
                       const char* docpath, const char* output_format,
                       bool multipage, int timeout_ms);

// Trigger a form-feed (eject the current page). Called from the mapper
// (Ctrl+F2 by default) and from the inactivity-timeout PIC event.
void PRINTER_FormFeed(bool pressed);

// Release the lazily-created CPrinter instance (closes any open multipage
// document, flushes the final page).
void PRINTER_Reset();

#endif // DOSBOX_PRINTER_IF
