// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TITLEBAR_H
#define DOSBOX_TITLEBAR_H

#include "config/setup.h"

void TITLEBAR_AddMessages();
void TITLEBAR_AddConfigSettings();

void TITLEBAR_ReadConfig();

void TITLEBAR_RefreshTitle();
void TITLEBAR_RefreshAnimatedTitle();

void TITLEBAR_NotifyBooting();
void TITLEBAR_NotifyAudioCaptureStatus(const bool is_capturing);
void TITLEBAR_NotifyVideoCaptureStatus(const bool is_capturing);
void TITLEBAR_NotifyAudioMutedStatus(const bool is_muted);

void TITLEBAR_NotifyProgramName(const std::string& segment_name,
                                const std::string& canonical_name);

void TITLEBAR_NotifyCyclesChanged();

#endif // DOSBOX_TITLEBAR_H
