// SPDX-FileCopyrightText:  2019-2020 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_MSCDEX_H
#define DOSBOX_DOS_MSCDEX_H

#include "dosbox.h"
#include "cdrom.h"

int   MSCDEX_AddDrive(char driveLetter, const char *physicalPath, uint8_t &subUnit);
int   MSCDEX_RemoveDrive(char driveLetter);
bool  MSCDEX_HasDrive(char driveLetter);
void  MSCDEX_ReplaceDrive(std::unique_ptr<CDROM_Interface> cdrom, uint8_t subUnit);
uint8_t MSCDEX_GetSubUnit(char driveLetter);
bool  MSCDEX_GetVolumeName(uint8_t subUnit, char *name);
bool  MSCDEX_HasMediaChanged(uint8_t subUnit);

#endif // DOSBOX_DOS_MSCDEX_H

