// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONSOLE_H
#define DOSBOX_CONSOLE_H

#include <string>

#include "utils/string_utils.h"

void CONSOLE_Write(const std::string_view output);

void CONSOLE_ResetLastWrittenChar(char c);
void CONSOLE_InjectMissingNewline();

#endif // DOSBOX_CONSOLE_H
