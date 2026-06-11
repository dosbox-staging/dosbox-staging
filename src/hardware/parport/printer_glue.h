// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PARPORT_PRINTER_GLUE_H
#define DOSBOX_PARPORT_PRINTER_GLUE_H

#include "config/config.h"

void PRINTER_AddConfigSection(const ConfigPtr& conf);
void PRINTER_Init();
void PRINTER_Destroy();

#endif
