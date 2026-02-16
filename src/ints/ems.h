// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_EMS_H
#define DOSBOX_EMS_H

#include "config/setup.h"

constexpr uint16_t EmsPageSize = 16 * 1024;

extern const std::string EmsDeviceName;

void EMS_Init(SectionProp& section);
void EMS_Destroy();

#endif // DOSBOX_EMS_H
