// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_MESSAGES_H
#define DOSBOX_PRIVATE_MESSAGES_H

#include <string>

void adjust_newlines(const std::string& current,
                     std::string& previous,
                     std::string& translated);

#endif // DOSBOX_PRIVATE_MESSAGES_H
