// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_PRINTER_H
#define DOSBOX_PRIVATE_PRINTER_H

#include "dosbox_config.h"

#include "hardware/port.h"

// Which printer the user picked.
//
// EpsonDotMatrix24Pin is the 24-pin LQ family (default, ESC/P + ESC/P 2).
// EpsonDotMatrix9Pin is the 9-pin FX/LX family (older ESC/P only -- has
// different line-spacing divisors and lacks several ESC/P 2 commands;
// see Printer::pins branches in printer_dispatch.cpp).
enum class PrinterModel {
	None,
	EpsonDotMatrix9Pin,
	EpsonDotMatrix24Pin,
	PostScript,
	Passthrough,
};

io_val_t PRINTER_ReadData(io_port_t port, io_width_t width);
void PRINTER_WriteData(io_port_t port, io_val_t val, io_width_t width);

io_val_t PRINTER_ReadStatus(io_port_t port, io_width_t width);

void PRINTER_WriteControl(io_port_t port, io_val_t val, io_width_t width);
io_val_t PRINTER_ReadControl(io_port_t port, io_width_t width);

// Set the printer config values that are read at lazy printer
// construction time inside PRINTER_WriteControl. page_width_in /
// page_height_in are inches (double). Dot-matrix-only settings are
// ignored when model is PostScript.
void PRINTER_Configure(PrinterModel model, int dpi,
                       double page_width_in, double page_height_in,
                       int timeout_ms);

// Trigger a form-feed (eject the current page). Called from the mapper
// (Ctrl+F2 by default) and from the inactivity-timeout PIC event.
void PRINTER_FormFeed(bool pressed);

// Release the lazily-created printer instance.
void PRINTER_Reset();

#endif // DOSBOX_PRIVATE_PRINTER_H
