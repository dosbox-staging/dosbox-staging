/*
 *  Copyright (C) 2019-2020  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_DOS_MSCDEX_H
#define DOSBOX_DOS_MSCDEX_H

#include "dosbox.h"
#include "cdrom.h"

int   MSCDEX_AddDrive(char driveLetter, const char *physicalPath, Bit8u &subUnit);
int   MSCDEX_RemoveDrive(char driveLetter);
bool  MSCDEX_HasDrive(char driveLetter);
void  MSCDEX_ReplaceDrive(CDROM_Interface *cdrom, Bit8u subUnit);
Bit8u MSCDEX_GetSubUnit(char driveLetter);
bool  MSCDEX_GetVolumeName(Bit8u subUnit, char *name);
bool  MSCDEX_HasMediaChanged(Bit8u subUnit);

#endif // DOSBOX_DOS_MSCDEX_H

