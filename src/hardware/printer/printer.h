// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Virtual printer module. Public entry point for the rest of the codebase
// (see src/dosbox.cpp). The module emulates Epson dot-matrix printers
// (24-pin LQ family and 9-pin FX/LX family) by interpreting the ESC/P 2
// byte stream into per-page bitmaps written to disk as PNGs, and also
// offers passthrough modes that save raw PostScript or arbitrary
// printer-language bytes verbatim. Internals live under private/.
//
// Primary reference (cited as "escp2ref.pdf <section>" throughout the
// printer sources):
//
//     Epson America, "EPSON ESC/P Reference Manual", revised April
//     1997. https://files.support.epson.com/pdf/general/escp2ref.pdf

#ifndef DOSBOX_PRINTER_H
#define DOSBOX_PRINTER_H

#include "config/config.h"

void PRINTER_AddConfigSection(const ConfigPtr& conf);
void PRINTER_Init();
void PRINTER_Destroy();

#endif // DOSBOX_PRINTER_H
