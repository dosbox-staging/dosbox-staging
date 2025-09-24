// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_IMFC_H
#define DOSBOX_IMFC_H

#include "config/config.h"

void IMFC_AddConfigSection(const ConfigPtr& conf);
void IMFC_Init();
void IMFC_Destroy();

#endif // DOSBOX_IMFC_H
