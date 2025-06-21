// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLIPBOARD_H
#define DOSBOX_CLIPBOARD_H

#include <cstdint>
#include <string>

// Text clipboard support

bool CLIPBOARD_HasText();

void CLIPBOARD_CopyText(const std::string& content);
void CLIPBOARD_CopyText(const std::string& content, const uint16_t code_page);

std::string CLIPBOARD_PasteText();
std::string CLIPBOARD_PasteText(const uint16_t code_page);

#endif // DOSBOX_CLIPBOARD_H
