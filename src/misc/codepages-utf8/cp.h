/*
 *  Copyright (C) 2022       The DOSBox Staging Team
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

#include <map>
#include <set>
#include <string>
#include <vector>

typedef std::map<uint8_t, uint16_t> CodePageImport;
typedef std::map<uint16_t, char> CodePageMapping;
typedef std::vector<std::pair<uint16_t, uint16_t>> CodePagePatches;
typedef std::map<uint16_t, uint16_t> CodePageDuplicates;
typedef std::map<uint16_t, std::vector<const CodePageImport *>> CodePageMappingReceipt;

constexpr uint8_t unknown_character = 0x3f; // '?'

extern const CodePageImport cp_437;
extern const CodePageImport cp_737;
extern const CodePageImport cp_775;
extern const CodePageImport cp_850;
extern const CodePageImport cp_852;
extern const CodePageImport cp_855;
extern const CodePageImport cp_856;
extern const CodePageImport cp_857;
extern const CodePageImport cp_860;
extern const CodePageImport cp_861;
extern const CodePageImport cp_862;
extern const CodePageImport cp_863;
extern const CodePageImport cp_864;
extern const CodePageImport cp_865;
extern const CodePageImport cp_866;
extern const CodePageImport cp_869;
extern const CodePageImport cp_874;
extern const CodePageMapping cp_ascii;

extern const CodePagePatches        cp_patches;
extern const CodePageDuplicates     cp_duplicates;
extern const CodePageMappingReceipt cp_receipts;
