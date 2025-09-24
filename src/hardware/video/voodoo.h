// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_VOODOO_H
#define DOSBOX_VOODOO_H

#include "config/config.h"

void VOODOO_AddConfigSection(const ConfigPtr& conf);

void VOODOO_Init();
void VOODOO_Destroy();

#endif // DOSBOX_VOODOO_H
