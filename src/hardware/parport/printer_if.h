// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2013 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef PRINTER_IF
#define PRINTER_IF

#include "misc/types.h"

Bitu PRINTER_readdata(Bitu port,Bitu iolen);
void PRINTER_writedata(Bitu port,Bitu val,Bitu iolen);
Bitu PRINTER_readstatus(Bitu port,Bitu iolen);
void PRINTER_writecontrol(Bitu port,Bitu val, Bitu iolen);
Bitu PRINTER_readcontrol(Bitu port,Bitu iolen);

bool PRINTER_isInited();
#endif
