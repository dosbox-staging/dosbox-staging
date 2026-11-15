// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_PRIVATE_MAPPER_H
#define DOSBOX_GUI_PRIVATE_MAPPER_H

#include <utility>

// Scales an analog stick position from the circle to the square, so a full
// diagonal reaches full deflection on both axes. The direction stays the same;
// the result is clamped to the int16_t range of SDL's axis values.
std::pair<int, int> CircleToSquare(int x, int y);

#endif // DOSBOX_GUI_PRIVATE_MAPPER_H
