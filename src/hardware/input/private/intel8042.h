// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_INTEL_8042_H
#define DOSBOX_INTEL_8042_H

#include <cstdint>
#include <vector>

void I8042_Init();

// Functions for keyboard and mouse emulation code, to provide
// command responses ('AddByte') and key scancodes / mouse updates
// ('AddFrame')

void I8042_AddAuxByte(const uint8_t byte);
void I8042_AddAuxFrame(const std::vector<uint8_t>& bytes);
void I8042_AddKbdByte(const uint8_t byte);
void I8042_AddKbdFrame(const std::vector<uint8_t>& bytes);

bool I8042_IsReadyForAuxFrame();
bool I8042_IsReadyForKbdFrame();

void I8042_TriggerAuxInterrupt();

// The following routines needs to be implemented by keyboard/mouse
// emulation code:

void KEYBOARD_PortWrite(const uint8_t byte);
bool MOUSEPS2_PortWrite(const uint8_t byte); // false if mouse not connected

void KEYBOARD_NotifyReadyForFrame();
void MOUSEPS2_NotifyReadyForFrame();

#endif // DOSBOX_INTEL_8042_H
