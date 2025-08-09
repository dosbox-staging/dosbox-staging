// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TITLEBAR_H
#define DOSBOX_TITLEBAR_H

#include "config/setup.h"

void TITLEBAR_AddMessages();
void TITLEBAR_AddConfig(SectionProp& secprop);
void TITLEBAR_ReadConfig(const SectionProp& secprop);

#endif // DOSBOX_TITLEBAR_H
