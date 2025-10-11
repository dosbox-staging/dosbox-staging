// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_GUI_COMMON_H
#define DOSBOX_GUI_COMMON_H

#include "private/common.h"

#include "hardware/input/mouse.h"
#include "utils/rect.h"

// TODO We should improve this interface over time (e.g., remove the GFX_
// prefix, move the functions to more appropriate places, like the renderer,
// etc.)

void GFX_AddConfigSection();

void GFX_Init();
void GFX_Destroy();

void GFX_RequestExit(const bool pressed);

void GFX_Quit();

void GFX_LosingFocus();

void GFX_CenterMouse();

void GFX_SetMouseHint(const MouseHint requested_hint_id);
void GFX_SetMouseCapture(const bool requested_capture);
void GFX_SetMouseVisibility(const bool requested_visible);
void GFX_SetMouseRawInput(const bool requested_raw_input);

// Detects the presence of a desktop environment or window manager
bool GFX_HaveDesktopEnvironment();

DosBox::Rect GFX_GetCanvasSizeInPixels();
DosBox::Rect GFX_GetViewportSizeInPixels();

double GFX_GetHostRefreshRate();

PresentationMode GFX_GetPresentationMode();

void GFX_MaybePresentFrame();

bool GFX_PollAndHandleEvents();

#endif // DOSBOX_GUI_COMMON_H
