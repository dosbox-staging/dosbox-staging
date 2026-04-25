// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RENDER_SHARED_H
#define DOSBOX_RENDER_SHARED_H

#include "misc/rendered_image.h"

void RENDER_UpdateSharedFrame(const RenderedImage& image);
RenderedImage RENDER_GetSharedFrame();
bool RENDER_HasSharedFrame();

#endif // DOSBOX_RENDER_SHARED_H
