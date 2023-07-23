/*
 *  Copyright (C) 2019-2023  The DOSBox Staging Team
 *  Copyright (C) 2002-2015  The DOSBox Team
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

#include "dos_keyboard_layout.h"

#include <map>
#include <memory>
#include <string_view>
using sv = std::string_view;

#include "../ints/int10.h"
#include "autoexec.h"
#include "bios.h"
#include "bios_disk.h"
#include "callback.h"
#include "dos_inc.h"
#include "drives.h"
#include "mapper.h"
#include "math_utils.h"
#include "regs.h"
#include "setup.h"
#include "string_utils.h"

// The default codepage for DOS
constexpr uint16_t default_cp_437 = 437;
constexpr auto default_country    = Country::United_States;

// A common pattern in the keyboard layout file is to try opening the requested
// file first within DOS, then from the local path, and finally from builtin
// resources. This function performs those in order and returns the first hit.
static FILE_unique_ptr open_layout_file(const char *name, const char *resource_dir = nullptr)
{
	constexpr auto file_perms = "rb";

	// Try opening from DOS first (this can throw, so we catch/ignore)
	uint8_t drive = 0;
	char fullname[DOS_PATHLENGTH] = {};
	if (DOS_MakeName(name, fullname, &drive)) try {
		// try to open file on mounted drive first
		const auto ldp = dynamic_cast<localDrive*>(Drives[drive]);
		if (ldp) {
			if (const auto fp = ldp->GetSystemFilePtr(fullname, file_perms); fp) {
				return FILE_unique_ptr(fp);
			}
		}
	} catch (...) {}

	// Then try from the local filesystem
	if (const auto fp = fopen(name, file_perms); fp)
		return FILE_unique_ptr(fp);

	// Finally try from a built-in resources
	if (resource_dir)
		if (const auto rp = GetResourcePath(resource_dir, name); !rp.empty())
			return make_fopen(rp.string().c_str(), file_perms);

	return nullptr;
}

class KeyboardLayout {
public:
	KeyboardLayout()
	{
		Reset();
		sprintf(current_keyboard_file_name, "none");
	}

	KeyboardLayout(const KeyboardLayout &) = delete; // prevent copying
	KeyboardLayout &operator=(const KeyboardLayout &) = delete; // prevent
	                                                            // assignment

	// read in a codepage from a .cpi-file
	KeyboardErrorCode ReadCodePageFile(const char *codepage_file_name,
	                                   const int32_t codepage_id);

	uint16_t ExtractCodePage(const char *keyboard_file_name);

	// read in a keyboard layout from a .kl-file
	KeyboardErrorCode ReadKeyboardFile(const char *keyboard_file_name,
	                                   const int32_t req_cp);

	// call SetLayoutKey to apply the current language layout
	bool SetLayoutKey(const uint8_t key, const uint8_t flags1,
	                  const uint8_t flags2, const uint8_t flags3);

	KeyboardErrorCode SwitchKeyboardLayout(const char *new_layout,
	                                       KeyboardLayout *&created_layout,
	                                       int32_t &tried_cp);
	void SwitchForeignLayout();
	const char *GetLayoutName();
	const char *GetMainLanguageCode();

private:
	static constexpr uint8_t layout_pages = 12;

	uint16_t current_layout[(MAX_SCAN_CODE + 1) * layout_pages] = {};
	struct {
		uint16_t required_flags      = 0;
		uint16_t forbidden_flags     = 0;
		uint16_t required_userflags  = 0;
		uint16_t forbidden_userflags = 0;
	} current_layout_planes[layout_pages - 4] = {};

	uint8_t additional_planes   = 0;
	uint8_t used_lock_modifiers = 0;

	// diacritics table
	uint8_t diacritics[2048] = {};

	uint16_t diacritics_entries   = 0;
	uint16_t diacritics_character = 0;
	uint16_t user_keys            = 0;

	char current_keyboard_file_name[256] = {};

	bool use_foreign_layout = false;

	// the list of language codes supported by this layout. Used when
	// switching layouts.
	std::list<std::string> language_codes = {};

	bool HasLanguageCode(const char *requested_code);
	void ResetLanguageCodes();

	void Reset();
	void ReadKeyboardFile(const int32_t specific_layout);

	KeyboardErrorCode ReadKeyboardFile(const char *keyboard_file_name,
	                                   const int32_t specific_layout,
	                                   const int32_t requested_codepage);

	bool SetMapKey(const uint8_t key, const uint16_t layouted_key,
	               const bool is_command, const bool is_keypair);
};

void KeyboardLayout::Reset()
{
	for (uint32_t i = 0; i < (MAX_SCAN_CODE + 1) * layout_pages; i++)
		current_layout[i] = 0;
	for (uint32_t i=0; i<layout_pages-4; i++) {
		current_layout_planes[i].required_flags=0;
		current_layout_planes[i].forbidden_flags=0xffff;
		current_layout_planes[i].required_userflags=0;
		current_layout_planes[i].forbidden_userflags=0xffff;
	}
	used_lock_modifiers=0x0f;
	diacritics_entries=0;		// no diacritics loaded
	diacritics_character=0;
	user_keys=0;				// all userkeys off
	language_codes.clear();
}

KeyboardErrorCode KeyboardLayout::ReadKeyboardFile(const char *keyboard_file_name,
                                                   const int32_t req_cp)
{
	return ReadKeyboardFile(keyboard_file_name, -1, req_cp);
}

// switch to a different layout
void KeyboardLayout::ReadKeyboardFile(const int32_t specific_layout)
{
	if (strcmp(current_keyboard_file_name, "none"))
		ReadKeyboardFile(current_keyboard_file_name,
		                 specific_layout,
		                 dos.loaded_codepage);
}

static uint32_t read_kcl_file(const FILE_unique_ptr &kcl_file, const char *layout_id, bool first_id_only)
{
	assert(kcl_file);
	assert(layout_id);
	static uint8_t rbuf[8192];

	// check ID-bytes of file
	auto dr = fread(rbuf, sizeof(uint8_t), 7, kcl_file.get());
	if ((dr < 7) || (rbuf[0] != 0x4b) || (rbuf[1] != 0x43) || (rbuf[2] != 0x46)) {
		return 0;
	}

	const auto seek_pos = 7 + rbuf[6];
	if (fseek(kcl_file.get(), seek_pos, SEEK_SET) != 0) {
		LOG_WARNING("LAYOUT: could not seek to byte %d in keyboard layout file: %s", seek_pos, strerror(errno));
		return 0;
	}

	for (;;) {
		const auto cur_pos = ftell(kcl_file.get());
		dr = fread(rbuf, sizeof(uint8_t), 5, kcl_file.get());
		if (dr < 5)
			break;
		uint16_t len=host_readw(&rbuf[0]);

		uint8_t data_len=rbuf[2];
		assert (data_len < UINT8_MAX);
		static_assert(UINT8_MAX < sizeof(rbuf), "rbuf too small");

		char lng_codes[258];

		constexpr int8_t lang_codes_offset = -2;
		if (fseek(kcl_file.get(), lang_codes_offset, SEEK_CUR) != 0) {
			LOG_ERR("LAYOUT: could not seek to the language codes "
			        "at byte %d in keyboard layout: %s",
			        check_cast<int>(cur_pos) + lang_codes_offset,
			        strerror(errno));
			return 0;
		}

		// get all language codes for this layout
		for (auto i = 0; i < data_len;) {
			if (fread(rbuf, sizeof(uint8_t), 2, kcl_file.get()) != 2) {
				break;
			}
			uint16_t lcnum = host_readw(&rbuf[0]);
			i+=2;
			uint16_t lng_pos = 0;
			for (;i<data_len;) {
				if (fread(rbuf, sizeof(uint8_t), 1, kcl_file.get()) != 1) {
					break;
				}
				i++;
				if (((char)rbuf[0])==',') break;
				lng_codes[lng_pos++] = (char)rbuf[0];
			}
			lng_codes[lng_pos] = 0;
			if (strcasecmp(lng_codes, layout_id)==0) {
				// language ID found in file, return file position
				return check_cast<uint32_t>(cur_pos);
			}
			if (first_id_only) break;
			if (lcnum) {
				sprintf(&lng_codes[lng_pos], "%d", lcnum);
				if (strcasecmp(lng_codes, layout_id)==0) {
					// language ID found in file, return file position
					return check_cast<uint32_t>(cur_pos);
				}
			}
		}
		if (fseek(kcl_file.get(), cur_pos + 3 + len, SEEK_SET) != 0) {
			LOG_ERR("LAYOUT: could not seek to byte %d in keyboard layout file: %s",
			        check_cast<int>(cur_pos) + 3 + len,
			        strerror(errno));
			return 0;
		}
	}
	return 0;
}

// Scans the builtin keyboard files for the given layout.
// If found, populates the kcl_file and starting position.
static bool load_builtin_keyboard_layouts(const char *layout_id, FILE_unique_ptr &kcl_file, uint32_t &kcl_start_pos)
{
	auto find_layout_id = [&](const char *builtin_filename, const bool first_only) {
		// Could we open the file?
		constexpr auto resource_dir = "freedos-keyboard";
		auto fp = open_layout_file(builtin_filename, resource_dir);
		if (!fp)
			return false;

		// Could we read it and find the start of the layout?
		const auto pos = read_kcl_file(fp, layout_id, first_only);
		if (pos == 0)
			return false;

		// Layout was found at the given position
		kcl_file = std::move(fp);
		kcl_start_pos = pos;
		return true;
	};

	for (const auto first_only : {true, false})
		for (const auto builtin_filename : {"KEYBOARD.SYS", "KEYBRD2.SYS", "KEYBRD3.SYS", "KEYBRD4.SYS"})
			if (find_layout_id(builtin_filename, first_only))
				return true;

	return false;
}

KeyboardErrorCode KeyboardLayout::ReadKeyboardFile(const char *keyboard_file_name,
                                                   const int32_t specific_layout,
                                                   const int32_t requested_codepage)
{
	Reset();

	if (specific_layout == -1)
		safe_strcpy(current_keyboard_file_name, keyboard_file_name);
	if (!strcmp(keyboard_file_name,"none")) return KEYB_NOERROR;

	static uint8_t read_buf[65535];
	uint32_t read_buf_size, read_buf_pos, bytes_read;
	uint32_t start_pos=5;

	char nbuf[512];
	read_buf_size = 0;
	sprintf(nbuf, "%s.kl", keyboard_file_name);
	auto tempfile = open_layout_file(nbuf);

	if (!tempfile) {
		if (!load_builtin_keyboard_layouts(keyboard_file_name, tempfile, start_pos)) {
			LOG(LOG_BIOS, LOG_ERROR)
			("Keyboard layout file %s not found", keyboard_file_name);
			return KEYB_FILENOTFOUND;
		}
		if (tempfile) {
			const auto seek_pos = start_pos + 2;
			if (fseek(tempfile.get(), seek_pos, SEEK_SET) != 0) {
				LOG_WARNING("LAYOUT: could not seek to byte %d in keyboard layout file '%s': %s",
				            seek_pos, keyboard_file_name, strerror(errno));
				return KEYB_INVALIDFILE;
			}
			read_buf_size = (uint32_t)fread(read_buf, sizeof(uint8_t),
			                              65535, tempfile.get());
		}
		start_pos=0;
	} else {
		// check ID-bytes of file
		uint32_t dr = (uint32_t)fread(read_buf, sizeof(uint8_t), 4,
		                          tempfile.get());
		if ((dr<4) || (read_buf[0]!=0x4b) || (read_buf[1]!=0x4c) || (read_buf[2]!=0x46)) {
			LOG(LOG_BIOS, LOG_ERROR)
			("Invalid keyboard layout file %s", keyboard_file_name);
			return KEYB_INVALIDFILE;
		}

		fseek(tempfile.get(), 0, SEEK_SET);
		read_buf_size = (uint32_t)fread(read_buf, sizeof(uint8_t), 65535,
		                              tempfile.get());
	}

	const auto data_len = read_buf[start_pos++];
	assert(data_len < UINT8_MAX);
	static_assert(UINT8_MAX < sizeof(read_buf), "read_buf too small");

	language_codes.clear();
	// get all language codes for this layout
	for (uint16_t i = 0; i < data_len;) {
		i+=2;
		std::string language_code = {};
		for (;i<data_len;) {
			assert(start_pos + i < sizeof(read_buf));
			char lcode=char(read_buf[start_pos+i]);
			i++;
			if (lcode==',') break;
			language_code.push_back(lcode);
		}
		if (!language_code.empty()) {
			language_codes.emplace_back(std::move(language_code));
		}
	}

	start_pos+=data_len;		// start_pos==absolute position of KeybCB block

	assert(start_pos < sizeof(read_buf));
	const auto submappings = read_buf[start_pos];

	assert(start_pos + 1 < sizeof(read_buf));
	additional_planes = read_buf[start_pos + 1];

	// four pages always occupied by normal,shift,flags,commandbits
	if (additional_planes>(layout_pages-4)) additional_planes=(layout_pages-4);

	// seek to plane descriptor
	read_buf_pos = start_pos + 0x14 + submappings * 8;

	assert(read_buf_pos < sizeof(read_buf));

	for (uint16_t i = 0; i < additional_planes; ++i) {
		auto &layout = current_layout_planes[i];
		for (auto p_flags : {
		             &layout.required_flags,
		             &layout.forbidden_flags,
		             &layout.required_userflags,
		             &layout.forbidden_userflags,
		     }) {
			assert(read_buf_pos < sizeof(read_buf));
			*p_flags = host_readw(&read_buf[read_buf_pos]);
			read_buf_pos += 2;
			if (p_flags == &layout.required_flags) {
				used_lock_modifiers |= ((*p_flags) & 0x70);
			}
		}
	}

	bool found_matching_layout=false;
	
	// check all submappings and use them if general submapping or same codepage submapping
	for (uint16_t sub_map=0; (sub_map<submappings) && (!found_matching_layout); sub_map++) {
		uint16_t submap_cp, table_offset;

		if ((sub_map!=0) && (specific_layout!=-1)) sub_map=(uint16_t)(specific_layout&0xffff);

		// read codepage of submapping
		submap_cp=host_readw(&read_buf[start_pos+0x14+sub_map*8]);
		if ((submap_cp!=0) && (submap_cp!=requested_codepage) && (specific_layout==-1))
			continue;		// skip nonfitting submappings

		if (submap_cp==requested_codepage) found_matching_layout=true;

		// get table offset
		table_offset=host_readw(&read_buf[start_pos+0x18+sub_map*8]);
		diacritics_entries=0;
		if (table_offset!=0) {
			// process table
			uint16_t i,j;
			for (i=0; i<2048;) {
				if (read_buf[start_pos+table_offset+i]==0) break;	// end of table
				diacritics_entries++;
				i+=read_buf[start_pos+table_offset+i+1]*2+2;
			}
			// copy diacritics table
			for (j=0; j<=i; j++) diacritics[j]=read_buf[start_pos+table_offset+j];
		}


		// get table offset
		table_offset=host_readw(&read_buf[start_pos+0x16+sub_map*8]);
		if (table_offset==0) continue;	// non-present table

		read_buf_pos=start_pos+table_offset;

		bytes_read=read_buf_size-read_buf_pos;

		// process submapping table
		for (uint32_t i=0; i<bytes_read;) {
			uint8_t scan=read_buf[read_buf_pos++];
			if (scan==0) break;
			uint8_t scan_length=(read_buf[read_buf_pos]&7)+1;		// length of data struct
			read_buf_pos+=2;
			i+=3;

			assert(scan_length > 0);
			if ((scan & 0x7f) <= MAX_SCAN_CODE) {
				// add all available mappings
				for (uint16_t addmap=0; addmap<scan_length; addmap++) {
					if (addmap>additional_planes+2) break;
					const auto pos =
					        read_buf_pos +
					        addmap * ((read_buf[read_buf_pos - 2] & 0x80)
					                          ? 2
					                          : 1);
					const auto charptr = check_cast<uint16_t>(pos);
					uint16_t kchar = read_buf[charptr];

					if (kchar!=0) {		// key remapped
						if (read_buf[read_buf_pos-2]&0x80) kchar|=read_buf[charptr+1]<<8;	// scancode/char pair
						// overwrite mapping
						current_layout[scan*layout_pages+addmap]=kchar;
						// clear command bit
						current_layout[scan*layout_pages+layout_pages-2]&=~(1<<addmap);
						// add command bit
						current_layout[scan*layout_pages+layout_pages-2]|=(read_buf[read_buf_pos-1] & (1<<addmap));
					}
				}

				// calculate max length of entries, taking into account old number of entries
				uint8_t new_flags=current_layout[scan*layout_pages+layout_pages-1]&0x7;
				if ((read_buf[read_buf_pos-2]&0x7) > new_flags) new_flags = read_buf[read_buf_pos-2]&0x7;

				// merge flag bits in as well
				new_flags |= (read_buf[read_buf_pos-2] | current_layout[scan*layout_pages+layout_pages-1]) & 0xf0;

				current_layout[scan*layout_pages+layout_pages-1]=new_flags;
				if (read_buf[read_buf_pos-2]&0x80) scan_length*=2;		// granularity flag (S)
			}
			i+=scan_length;		// advance pointer
			read_buf_pos+=scan_length;
		}
		if (specific_layout==sub_map) break;
	}

	if (found_matching_layout) {
		if (specific_layout==-1) LOG(LOG_BIOS,LOG_NORMAL)("Keyboard layout %s successfully loaded",keyboard_file_name);
		else LOG(LOG_BIOS,LOG_NORMAL)("Keyboard layout %s (%i) successfully loaded",keyboard_file_name,specific_layout);
		use_foreign_layout = true;
		return KEYB_NOERROR;
	}

	LOG(LOG_BIOS,LOG_ERROR)("No matching keyboard layout found in %s",keyboard_file_name);

	// Reset layout data (might have been changed by general layout)
	Reset();

	return KEYB_LAYOUTNOTFOUND;
}

bool KeyboardLayout::SetLayoutKey(const uint8_t key, const uint8_t flags1,
                                  const uint8_t flags2, const uint8_t flags3)
{
	if (key > MAX_SCAN_CODE)
		return false;
	if (!use_foreign_layout)
		return false;

	bool is_special_pair=(current_layout[key*layout_pages+layout_pages-1] & 0x80)==0x80;

	if ((((flags1&used_lock_modifiers)&0x7c)==0) && ((flags3&2)==0)) {
		// check if shift/caps is active:
		// (left_shift OR right_shift) XOR (key_affected_by_caps AND caps_locked)
		if ((((flags1&2)>>1) | (flags1&1)) ^ (((current_layout[key*layout_pages+layout_pages-1] & 0x40) & (flags1 & 0x40))>>6)) {
			// shift plane
			if (current_layout[key*layout_pages+1]!=0) {
				// check if command-bit is set for shift plane
				bool is_command=(current_layout[key*layout_pages+layout_pages-2]&2)!=0;
				if (SetMapKey(key,
				              current_layout[key * layout_pages + 1],
				              is_command,
				              is_special_pair))
					return true;
			}
		} else {
			// normal plane
			if (current_layout[key*layout_pages]!=0) {
				// check if command-bit is set for normal plane
				bool is_command=(current_layout[key*layout_pages+layout_pages-2]&1)!=0;
				if (SetMapKey(key,
				              current_layout[key * layout_pages],
				              is_command,
				              is_special_pair))
					return true;
			}
		}
	}

	// calculate current flags
	auto current_flags = (flags1 & 0x7f) |
	                     (((flags2 & 3) | (flags3 & 0xc)) << 8);
	if (flags1&3) current_flags|=0x4000;	// either shift key active
	if (flags3&2) current_flags|=0x1000;	// e0 prefixed

	// check all planes if flags fit
	for (uint16_t cplane = 0; cplane < additional_planes; cplane++) {
		uint16_t req_flags     = current_layout_planes[cplane].required_flags;
		uint16_t req_userflags = current_layout_planes[cplane].required_userflags;
		// test flags
		if (((current_flags & req_flags) == req_flags) &&
		    ((user_keys & req_userflags) == req_userflags) &&
		    ((current_flags & current_layout_planes[cplane].forbidden_flags) == 0) &&
		    ((user_keys & current_layout_planes[cplane].forbidden_userflags) == 0)) {
			// remap key
			if (current_layout[key * layout_pages + 2 + cplane] != 0) {
				// check if command-bit is set for this plane
				bool is_command = ((current_layout[key * layout_pages + layout_pages - 2] >> (cplane + 2)) & 1) != 0;
				if (SetMapKey(key, current_layout[key * layout_pages + 2 + cplane], is_command, is_special_pair))
					return true;
			} else {
				break; // abort plane checking
			}
		}
	}

	if (diacritics_character>0) {
		// ignore state-changing keys
		switch(key) {
			case 0x1d:			/* Ctrl Pressed */
			case 0x2a:			/* Left Shift Pressed */
			case 0x36:			/* Right Shift Pressed */
			case 0x38:			/* Alt Pressed */
			case 0x3a:			/* Caps Lock */
			case 0x45:			/* Num Lock */
			case 0x46:			/* Scroll Lock */
				break;
			default:
			        if (diacritics_character >= diacritics_entries + 200) {
				        diacritics_character=0;
					return true;
			        }
			        uint16_t diacritics_start=0;
				// search start of subtable
				for (uint16_t i=0; i<diacritics_character-200; i++)
					diacritics_start+=diacritics[diacritics_start+1]*2+2;

				BIOS_AddKeyToBuffer((uint16_t)(key<<8) | diacritics[diacritics_start]);
				diacritics_character=0;
		}
	}

	return false;
}

bool KeyboardLayout::SetMapKey(const uint8_t key, const uint16_t layouted_key,
                               const bool is_command, const bool is_keypair)
{
	if (is_command) {
		uint8_t key_command=(uint8_t)(layouted_key&0xff);
		// check if diacritics-command
		if ((key_command>=200) && (key_command<235)) {
			// diacritics command
			diacritics_character=key_command;
			if (diacritics_character >= diacritics_entries + 200)
				diacritics_character = 0;
			return true;
		} else if ((key_command>=120) && (key_command<140)) {
			// switch layout command
			ReadKeyboardFile(key_command - 119);
			return true;
		} else if ((key_command>=180) && (key_command<188)) {
			// switch user key off
			user_keys&=~(1<<(key_command-180));
			return true;
		} else if ((key_command>=188) && (key_command<196)) {
			// switch user key on
			user_keys|=(1<<(key_command-188));
			return true;
		} else if (key_command==160) return true;	// nop command
	} else {
		// non-command
		if (diacritics_character>0) {
			if (diacritics_character-200>=diacritics_entries) diacritics_character = 0;
			else {
				uint16_t diacritics_start=0;
				// search start of subtable
				for (uint16_t i=0; i<diacritics_character-200; i++)
					diacritics_start+=diacritics[diacritics_start+1]*2+2;

				uint8_t diacritics_length=diacritics[diacritics_start+1];
				diacritics_start+=2;
				diacritics_character=0;	// reset

				// search scancode
				for (uint16_t i=0; i<diacritics_length; i++) {
					if (diacritics[diacritics_start+i*2]==(layouted_key&0xff)) {
						// add diacritics to keybuf
						BIOS_AddKeyToBuffer((uint16_t)(key<<8) | diacritics[diacritics_start+i*2+1]);
						return true;
					}
				}
				// add standard-diacritics to keybuf
				BIOS_AddKeyToBuffer((uint16_t)(key<<8) | diacritics[diacritics_start-2]);
			}
		}

		// add remapped key to keybuf
		if (is_keypair) BIOS_AddKeyToBuffer(layouted_key);
		else BIOS_AddKeyToBuffer((uint16_t)(key<<8) | (layouted_key&0xff));

		return true;
	}
	return false;
}

uint16_t KeyboardLayout::ExtractCodePage(const char *keyboard_file_name)
{
	if (!strcmp(keyboard_file_name, "none"))
		return default_cp_437;

	size_t read_buf_size = 0;
	static uint8_t read_buf[65535];
	uint32_t start_pos=5;

	char nbuf[512];
	sprintf(nbuf, "%s.kl", keyboard_file_name);
	auto tempfile = open_layout_file(nbuf);

	if (!tempfile) {
		if (!load_builtin_keyboard_layouts(keyboard_file_name, tempfile, start_pos)) {
			LOG(LOG_BIOS, LOG_ERROR)
			("Keyboard layout file %s not found", keyboard_file_name);
			return default_cp_437;
		}
		if (tempfile) {
			fseek(tempfile.get(), start_pos + 2, SEEK_SET);
			read_buf_size = fread(read_buf, sizeof(uint8_t), 65535, tempfile.get());
		}
		start_pos=0;
	} else {
		// check ID-bytes of file
		uint32_t dr = (uint32_t)fread(read_buf, sizeof(uint8_t), 4,
		                          tempfile.get());
		if ((dr<4) || (read_buf[0]!=0x4b) || (read_buf[1]!=0x4c) || (read_buf[2]!=0x46)) {
			LOG(LOG_BIOS, LOG_ERROR)
			("Invalid keyboard layout file %s", keyboard_file_name);
			return default_cp_437;
		}

		fseek(tempfile.get(), 0, SEEK_SET);
		read_buf_size = fread(read_buf, sizeof(uint8_t), 65535, tempfile.get());
	}
	if (read_buf_size == 0) {
		LOG_WARNING("CODEPAGE: Could not read data from layout file %s", keyboard_file_name);
		return default_cp_437;
	}

	auto data_len = read_buf[start_pos++];

	start_pos+=data_len;		// start_pos==absolute position of KeybCB block

	assert(start_pos < sizeof(read_buf));
	auto submappings = read_buf[start_pos];

	// Make sure the submappings value won't let us read beyond the end of
	// the buffer
	if (submappings >= ceil_udivide(sizeof(read_buf) - start_pos - 0x14, 8u)) {
		LOG(LOG_BIOS, LOG_ERROR)
		("Keyboard layout file %s is corrupt", keyboard_file_name);
		return default_cp_437;
	}

	// check all submappings and use them if general submapping or same
	// codepage submapping
	for (uint16_t sub_map=0; (sub_map<submappings); sub_map++) {
		uint16_t submap_cp;

		// read codepage of submapping
		submap_cp=host_readw(&read_buf[start_pos+0x14+sub_map*8]);

		if (submap_cp!=0) return submap_cp;
	}
	return default_cp_437;
}

const char *get_builtin_cp_filename(const int codepage_id)
{
	// reference:
	// https://gitlab.com/FreeDOS/base/cpidos/-/blob/master/DOC/CPIDOS/CODEPAGE.TXT
	switch (codepage_id) {
	case 437:
	case 850:
	case 852:
	case 853:
	case 857:
	case 858: return "EGA.CPX";
	case 775:
	case 859:
	case 1116:
	case 1117:
	case 1118:
	case 1119: return "EGA2.CPX";
	case 771:
	case 772:
	case 808:
	case 855:
	case 866:
	case 872: return "EGA3.CPX";
	case 848:
	case 849:
	case 1125:
	case 1131:
	case 3012:
	case 30010: return "EGA4.CPX";
	case 113:
	case 737:
	case 851:
	case 869: return "EGA5.CPX";
	case 899:
	case 30008:
	case 58210:
	case 59829:
	case 60258:
	case 60853: return "EGA6.CPX";
	case 30011:
	case 30013:
	case 30014:
	case 30017:
	case 30018:
	case 30019: return "EGA7.CPX";
	case 770:
	case 773:
	case 774:
	case 777:
	case 778: return "EGA8.CPX";
	case 860:
	case 861:
	case 863:
	case 865:
	case 867: return "EGA9.CPX";
	case 667:
	case 668:
	case 790:
	case 991:
	case 3845: return "EGA10.CPX";
	case 30000:
	case 30001:
	case 30004:
	case 30007:
	case 30009: return "EGA11.CPX";
	case 30003:
	case 30029:
	case 30030:
	case 58335: return "EGA12.CPX";
	case 895:
	case 30002:
	case 58152:
	case 59234:
	case 62306: return "EGA13.CPX";
	case 30006:
	case 30012:
	case 30015:
	case 30016:
	case 30020:
	case 30021: return "EGA14.CPX";
	case 30023:
	case 30024:
	case 30025:
	case 30026:
	case 30027:
	case 30028: return "EGA15.CPX";
	case 3021:
	case 30005:
	case 30022:
	case 30031:
	case 30032: return "EGA16.CPX";
	case 862:
	case 864:
	case 30034:
	case 30033:
	case 30039:
	case 30040: return "EGA17.CPX";
	case 856:
	case 3846:
	case 3848: return "EGA18.CPI";
	default: return ""; // none
	}
}

KeyboardErrorCode KeyboardLayout::ReadCodePageFile(const char *requested_cp_filename, const int32_t codepage_id)
{
	assert(requested_cp_filename);
	std::string cp_filename = requested_cp_filename;

	if (cp_filename.empty() || cp_filename == "none" || codepage_id == dos.loaded_codepage)
		return KEYB_NOERROR;

	if (cp_filename == "auto") {
		cp_filename = get_builtin_cp_filename(codepage_id);
		if (cp_filename.empty()) {
			LOG_WARNING("CODEPAGE: Could not find a file for codepage ID %d", codepage_id);
			return KEYB_INVALIDCPFILE;
		}
	}
	// At this point, we expect to have a filename
	assert(!cp_filename.empty());

	auto tempfile = open_layout_file(cp_filename.c_str(), "freedos-cpi");
	if (!tempfile) {
		LOG_WARNING("CODEPAGE: Could not open file %s in DOS or from host resources", cp_filename.c_str());
		return KEYB_INVALIDCPFILE;
	}
	// At this point, we expect to have a file
	assert(tempfile);

	std::array<uint8_t, UINT16_MAX + 1> cpi_buf;
	constexpr size_t cpi_unit_size = sizeof(cpi_buf[0]);

	size_t cpi_buf_size = 0;
	size_t size_of_cpxdata = 0;
	bool upxfound = false;
	size_t found_at_pos = 5;

	constexpr auto bytes_to_detect_upx = 5;

	const auto dr = fread(cpi_buf.data(), cpi_unit_size, bytes_to_detect_upx, tempfile.get());
	// check if file is valid
	if (dr < 5) {
		LOG(LOG_BIOS, LOG_ERROR)
		("Codepage file %s invalid", cp_filename.c_str());
		return KEYB_INVALIDCPFILE;
	}

	// Helper used by both the compressed and uncompressed code paths to read the entire file
	auto read_entire_cp_file = [&](size_t &bytes_read) {
		if (fseek(tempfile.get(), 0, SEEK_SET) != 0) {
			LOG_ERR("CODEPAGE: could not seek to start of compressed file %s: %s: ",
			        cp_filename.c_str(), strerror(errno));
			return false;
		}
		bytes_read = fread(cpi_buf.data(),
		                   cpi_unit_size,
		                   cpi_buf.size(),
		                   tempfile.get());
		return (bytes_read > 0);
	};

	// check if non-compressed cpi file
	if ((cpi_buf[0] != 0xff) || (cpi_buf[1] != 0x46) || (cpi_buf[2] != 0x4f) || (cpi_buf[3] != 0x4e) ||
	    (cpi_buf[4] != 0x54)) {
		// check if dr-dos custom cpi file
		if ((cpi_buf[0] == 0x7f) && (cpi_buf[1] != 0x44) && (cpi_buf[2] != 0x52) && (cpi_buf[3] != 0x46) &&
		    (cpi_buf[4] != 0x5f)) {
			LOG(LOG_BIOS, LOG_ERROR)
			("Codepage file %s has unsupported DR-DOS format", cp_filename.c_str());
			return KEYB_INVALIDCPFILE;
		}

		// Read enough data to scan for UPX's identifier and version
		const auto scan_size = 100;
		assert(scan_size <= cpi_buf.size());
		if (fread(cpi_buf.data(), cpi_unit_size, scan_size, tempfile.get()) != scan_size) {
			LOG_WARNING("CODEPAGE: File %s is too small, could not read initial %d bytes",
			            cp_filename.c_str(),
			            scan_size + ds);
			return KEYB_INVALIDCPFILE;
		}
		// Scan for the UPX identifier
		const auto upx_id     = sv{"UPX!"};
		const auto scan_buf   = sv{reinterpret_cast<char *>(cpi_buf.data()), scan_size};
		const auto upx_id_pos = scan_buf.find(upx_id);

		// did we find the UPX identifier?
		upxfound = upx_id_pos != scan_buf.npos;
		if (!upxfound) {
			LOG_WARNING("CODEPAGE: File %s is invalid, could not find the UPX identifier", cp_filename.c_str());
			return KEYB_INVALIDCPFILE;
		}
		// The IPX version byte comes after the identifier pattern
		const auto upx_ver_pos = upx_id_pos + upx_id.length();
		const auto upx_ver     = cpi_buf[upx_ver_pos];

		// Can we handle this version?
		constexpr uint8_t upx_min_ver = 10;
		if (upx_ver < upx_min_ver) {
			LOG_WARNING("CODEPAGE: File %s is packed with UPX version %u, but %u+ is needed",
			            cp_filename.c_str(),
			            upx_ver,
			            upx_min_ver);
			return KEYB_INVALIDCPFILE;
		}
		// The next data comes after the version (used for decompression below)
		found_at_pos += upx_ver_pos + sizeof(upx_ver);

		// Read the entire compressed CPX file
		if (!read_entire_cp_file(size_of_cpxdata))
			return KEYB_INVALIDCPFILE;

	} else {
		// read the entire uncompressed CPI file
		if (!read_entire_cp_file(cpi_buf_size))
			return KEYB_INVALIDCPFILE;
	}

	if (upxfound) {
		if (size_of_cpxdata>0xfe00) E_Exit("Size of cpx-compressed data too big");

		found_at_pos+=19;
		// prepare for direct decompression
		cpi_buf[found_at_pos]=0xcb;

		uint16_t seg=0;
		uint16_t size=0x1500;
		if (!DOS_AllocateMemory(&seg,&size)) E_Exit("Not enough free low memory to unpack data");

		const auto dos_segment = static_cast<uint32_t>((seg << 4) + 0x100);
		assert(size_of_cpxdata <= cpi_buf.size());
		MEM_BlockWrite(dos_segment, cpi_buf.data(), size_of_cpxdata);

		// setup segments
		uint16_t save_ds=SegValue(ds);
		uint16_t save_es=SegValue(es);
		uint16_t save_ss=SegValue(ss);
		uint32_t save_esp=reg_esp;
		SegSet16(ds,seg);
		SegSet16(es,seg);
		SegSet16(ss,seg+0x1000);
		reg_esp=0xfffe;

		// let UPX unpack the file
		CALLBACK_RunRealFar(seg,0x100);

		SegSet16(ds,save_ds);
		SegSet16(es,save_es);
		SegSet16(ss,save_ss);
		reg_esp=save_esp;

		// get unpacked content
		MEM_BlockRead(dos_segment, cpi_buf.data(), cpi_buf.size());
		cpi_buf_size=65536;

		DOS_FreeMemory(seg);
	}

	constexpr auto data_start_index = 0x13;
	static_assert(data_start_index < cpi_buf.size());
	auto start_pos = host_readd(&cpi_buf[data_start_index]);

	// Internally unpacking some UPX code-page files can result in unparseable data
	if (start_pos >= cpi_buf_size) {
		LOG_WARNING("KEYBOARD: Could not parse %scode-data from: %s",
		            (upxfound) ? "UPX-unpacked " : "",
		            cp_filename.c_str());

		LOG(LOG_BIOS, LOG_ERROR)
		("Code-page file %s invalid start_pos=%u", cp_filename.c_str(), start_pos);
		return KEYB_INVALIDCPFILE;
	}

	const auto number_of_codepages = host_readw(&cpi_buf.at(start_pos));
	start_pos+=4;

	// search if codepage is provided by file
	for (uint16_t test_codepage = 0; test_codepage < number_of_codepages; test_codepage++) {
		// device type can be display/printer (only the first is supported)
		const auto device_type = host_readw(&cpi_buf.at(start_pos + 0x04));
		const auto font_codepage = host_readw(&cpi_buf.at(start_pos + 0x0e));
		const auto font_data_header_pt = host_readd(&cpi_buf.at(start_pos + 0x16));
		const auto font_type = host_readw(&cpi_buf.at(font_data_header_pt));

		if ((device_type==0x0001) && (font_type==0x0001) && (font_codepage==codepage_id)) {
			// valid/matching codepage found

			const auto number_of_fonts = host_readw(&cpi_buf.at(font_data_header_pt + 0x02));
			// const uint16_t font_data_length = host_readw(&cpi_buf[font_data_header_pt + 0x04]);

			auto font_data_start = font_data_header_pt + 0x06;

			// load all fonts if possible
			bool font_changed = false;
			for (uint16_t current_font = 0; current_font < number_of_fonts; ++current_font) {
				const auto font_height = cpi_buf.at(font_data_start);
				font_data_start += 6;
				if (font_height == 0x10) {
					// 16x8 font
					const auto font16pt = RealToPhysical(int10.rom.font_16);
					for (uint16_t i = 0; i < 256 * 16; ++i) {
						phys_writeb(font16pt + i, cpi_buf.at(font_data_start + i));
					}
					// terminate alternate list to prevent loading
					phys_writeb(RealToPhysical(int10.rom.font_16_alternate),0);
					font_changed=true;
				} else if (font_height == 0x0e) {
					// 14x8 font
					const auto font14pt = RealToPhysical(int10.rom.font_14);
					for (uint16_t i = 0; i < 256 * 14; ++i) {
						phys_writeb(font14pt + i, cpi_buf.at(font_data_start + i));
					}
					// terminate alternate list to prevent loading
					phys_writeb(RealToPhysical(int10.rom.font_14_alternate),0);
					font_changed=true;
				} else if (font_height == 0x08) {
					// 8x8 fonts
					auto font8pt = RealToPhysical(int10.rom.font_8_first);
					for (uint16_t i = 0; i < 128 * 8; ++i) {
						phys_writeb(font8pt + i, cpi_buf.at(font_data_start + i));
					}
					font8pt=RealToPhysical(int10.rom.font_8_second);
					for (uint16_t i = 0; i < 128 * 8; ++i) {
						phys_writeb(font8pt + i,
						            cpi_buf.at(font_data_start + i + 128 * 8));
					}
					font_changed=true;
				}
				font_data_start+=font_height*256;
			}

			LOG(LOG_BIOS,LOG_NORMAL)("Codepage %i successfully loaded",codepage_id);

			// set codepage entries
			dos.loaded_codepage=(uint16_t)(codepage_id&0xffff);

			// update font if necessary
			if (font_changed && (CurMode->type==M_TEXT) && (IS_EGAVGA_ARCH)) {
				INT10_ReloadFont();
			}
			INT10_SetupRomMemoryChecksum();

			// convert UTF-8 [autoexec] section to new code page
			AUTOEXEC_NotifyNewCodePage();

			return KEYB_NOERROR;
		}

		start_pos = host_readd(&cpi_buf.at(start_pos));
		start_pos+=2;
	}

	LOG(LOG_BIOS,LOG_ERROR)("Codepage %i not found",codepage_id);

	return KEYB_INVALIDCPFILE;
}

bool KeyboardLayout::HasLanguageCode(const char *requested_code)
{
	for (const auto &language_code : language_codes)
		if (iequals(language_code, requested_code))
			return true;
	return false;
}

KeyboardErrorCode KeyboardLayout::SwitchKeyboardLayout(const char *new_layout,
                                                       KeyboardLayout *&created_layout,
                                                       int32_t &tried_cp)
{
	assert(new_layout);

	if (!iequals(new_layout, "US")) {
		// switch to a foreign layout

		if (HasLanguageCode(new_layout)) {
			if (!use_foreign_layout) {
				// switch to foreign layout
				use_foreign_layout  = true;
				diacritics_character=0;
				LOG(LOG_BIOS, LOG_NORMAL)("Switched to layout %s", new_layout);
			}
		} else {
			KeyboardLayout *temp_layout = new KeyboardLayout();
			auto req_codepage           = temp_layout->ExtractCodePage(new_layout);
			tried_cp                    = req_codepage;
			auto rcode                  = temp_layout->ReadKeyboardFile(new_layout, req_codepage);
			if (rcode) {
				delete temp_layout;
				return rcode;
			}
			// ...else keyboard layout loaded successfully, change codepage accordingly
			rcode = temp_layout->ReadCodePageFile("auto", req_codepage);
			if (rcode) {
				delete temp_layout;
				return rcode;
			}
			// Everything went fine, switch to new layout
			created_layout=temp_layout;
		}
	} else if (use_foreign_layout) {
		// switch to the US layout
		use_foreign_layout  = false;
		diacritics_character=0;
		LOG(LOG_BIOS,LOG_NORMAL)("Switched to US layout");
	}
	return KEYB_NOERROR;
}

void KeyboardLayout::SwitchForeignLayout()
{
	use_foreign_layout   = !use_foreign_layout;
	diacritics_character = 0;
	if (use_foreign_layout)
		LOG(LOG_BIOS, LOG_NORMAL)("Switched to foreign layout");
	else
		LOG(LOG_BIOS, LOG_NORMAL)("Switched to US layout");
}

const char *KeyboardLayout::GetLayoutName()
{
	// get layout name (language ID or nullptr if default layout)
	if (use_foreign_layout) {
		if (strcmp(current_keyboard_file_name,"none") != 0) {
			return (const char*)&current_keyboard_file_name;
		}
	}
	return nullptr;
}

const char *KeyboardLayout::GetMainLanguageCode()
{
	if (!language_codes.empty()) {
		assert(!language_codes.front().empty());
		return language_codes.front().c_str();
	}
	return nullptr;
}

static std::unique_ptr<KeyboardLayout> loaded_layout = {};

#if 0
// Ctrl+Alt+F2 switches between foreign and US-layout using this function
static void SwitchKeyboardLayout(bool pressed) {
	if (!pressed)
		return;
	if (loaded_layout) loaded_layout->SwitchForeignLayout();
}
#endif

// called by int9-handler
bool DOS_LayoutKey(const uint8_t key, const uint8_t flags1,
                   const uint8_t flags2, const uint8_t flags3)
{
	if (loaded_layout)
		return loaded_layout->SetLayoutKey(key, flags1, flags2, flags3);
	else
		return false;
}

KeyboardErrorCode DOS_LoadKeyboardLayout(const char *layoutname, const int32_t codepage, const char *codepagefile)
{
	auto temp_layout = std::make_unique<KeyboardLayout>();

	// try to read the layout for the specified codepage
	auto rcode = temp_layout->ReadKeyboardFile(layoutname, codepage);
	if (rcode) {
		return rcode;
	}
	// ...else keyboard layout loaded successfully, change codepage accordingly
	rcode = temp_layout->ReadCodePageFile(codepagefile, codepage);
	if (rcode) {
		return rcode;
	}
	// Everything went fine, switch to new layout
	loaded_layout = std::move(temp_layout);
	return KEYB_NOERROR;
}

KeyboardErrorCode DOS_SwitchKeyboardLayout(const char *new_layout, int32_t &tried_cp)
{
	if (loaded_layout) {
		KeyboardLayout *changed_layout = nullptr;
		const auto rcode = loaded_layout->SwitchKeyboardLayout(new_layout, changed_layout, tried_cp);
		if (changed_layout) {
			// Remove old layout, activate new layout
			loaded_layout.reset(changed_layout);
		}
		return rcode;
	} else
		return KEYB_LAYOUTNOTFOUND;
}

// get currently loaded layout name (nullptr if no layout is loaded)
const char* DOS_GetLoadedLayout(void) {
	if (loaded_layout) {
		return loaded_layout->GetLayoutName();
	}
	return nullptr;
}

static const std::map<std::string, Country> country_code_map{
        // clang-format off
	// reference: https://gitlab.com/FreeDOS/base/keyb_lay/-/blob/master/DOC/KEYB/LAYOUTS/LAYOUTS.TXT
	{"ar462",  Country::Arabic         },
	{"ar470",  Country::Arabic         },
	{"az",     Country::Azerbaijan     },
	{"ba",     Country::Bosnia         },
	{"be",     Country::Belgium        },
	{"bg",     Country::Bulgaria       }, // 101-key
	{"bg103",  Country::Bulgaria       }, // 101-key, Phonetic
	{"bg241",  Country::Bulgaria       }, // 102-key
	{"bl",     Country::Belarus        },
	{"bn",     Country::Benin          },
	{"br",     Country::Brazil         }, // ABNT layout
	{"br274",  Country::Brazil         }, // US layout
	{"bx",     Country::Belgium        }, // International
	{"by",     Country::Belarus        },
	{"ca",     Country::Candian_French }, // Standard
	{"ce",     Country::Russia         }, // Chechnya Standard
	{"ce443",  Country::Russia         }, // Chechnya Typewriter
	{"cg",     Country::Montenegro     },
	{"cf",     Country::Candian_French }, // Standard
	{"cf445",  Country::Candian_French }, // Dual-layer
	{"co",     Country::United_States  }, // Colemak
	{"cz",     Country::Czech_Slovak   }, // Czechia, QWERTY
	{"cz243",  Country::Czech_Slovak   }, // Czechia, Standard
	{"cz489",  Country::Czech_Slovak   }, // Czechia, Programmers
	{"de",     Country::Germany        }, // Standard
	{"dk",     Country::Denmark        },
	{"dv",     Country::United_States  }, // Dvorak
	{"ee",     Country::Estonia        },
	{"el",     Country::Greece         }, // 319
	{"es",     Country::Spain          },
	{"et",     Country::Estonia        },
	{"fi",     Country::Finland        },
	{"fo",     Country::Faeroe_Islands },
	{"fr",     Country::France         }, // Standard
	{"fx",     Country::France         }, // International
	{"gk",     Country::Greece         }, // 319
	{"gk220",  Country::Greece         }, // 220
	{"gk459",  Country::Greece         }, // 101-key
	{"gr",     Country::Germany        }, // Standard
	{"gr453",  Country::Germany        }, // Dual-layer
	{"hr",     Country::Croatia        },
	{"hu",     Country::Hungary        }, // 101-key
	{"hu208",  Country::Hungary        }, // 102-key
	{"hy",     Country::Armenia        },
	{"il",     Country::Israel         },
	{"is",     Country::Iceland        }, // 101-key
	{"is161",  Country::Iceland        }, // 102-key
	{"it",     Country::Italy          }, // Standard
	{"it142",  Country::Italy          }, // Comma on Numeric Pad
	{"ix",     Country::Italy          }, // International
	{"jp",     Country::Japan          },
	{"ka",     Country::Georgia        },
	{"kk",     Country::Kazakhstan     },
	{"kk476",  Country::Kazakhstan     },
	{"kx",     Country::United_Kingdom }, // International
	{"ky",     Country::Kyrgyzstan     },
	{"la",     Country::Latin_America  },
	{"lh",     Country::United_States  }, // Left-Hand Dvorak
	{"lt",     Country::Lithuania      }, // Baltic
	{"lt210",  Country::Lithuania      }, // 101-key, Programmers
	{"lt211",  Country::Lithuania      }, // AZERTY
	{"lt221",  Country::Lithuania      }, // Standard
	{"lt456",  Country::Lithuania      }, // Dual-layout
	{"lv",     Country::Latvia         }, // Standard
	{"lv455",  Country::Latvia         }, // Dual-layout
	{"ml",     Country::Malta          }, // UK-based
	{"mk",     Country::Macedonia      },
	{"mn",     Country::Mongolia       },
	{"mo",     Country::Mongolia       },
	{"mt",     Country::Malta          }, // UK-based
	{"mt103",  Country::Malta          }, // US-based
	{"ne",     Country::Niger          },
	{"ng",     Country::Nigeria        },
	{"nl",     Country::Netherlands    }, // 102-key
	{"no",     Country::Norway         },
	{"ph",     Country::Philippines    },
	{"pl",     Country::Poland         }, // 101-key, Programmers
	{"pl214",  Country::Poland         }, // 102-key
	{"po",     Country::Portugal       },
	{"px",     Country::Portugal       }, // International
	{"ro",     Country::Romania        }, // Standard
	{"ro446",  Country::Romania        }, // QWERTY
	{"rh",     Country::United_States  }, // Right-Hand Dvorak
	{"ru",     Country::Russia         }, // Standard
	{"ru443",  Country::Russia         }, // Typewriter
	{"rx",     Country::Russia         }, // Extended Standard
	{"rx443",  Country::Russia         }, // Extended Typewriter
	{"sd",     Country::Switzerland    }, // German
	{"sf",     Country::Switzerland    }, // French
	{"sg",     Country::Switzerland    }, // German
	{"si",     Country::Slovenia       },
	{"sk",     Country::Czech_Slovak   }, // Slovakia
	{"sp",     Country::Spain          },
	{"sq",     Country::Albania        }, // No-deadkeys
	{"sq448",  Country::Albania        }, // Deadkeys
	{"sr",     Country::Serbia         }, // Deadkey
	{"su",     Country::Finland        },
	{"sv",     Country::Sweden         },
	{"sx",     Country::Spain          }, // International
	{"tj",     Country::Tadjikistan    },
	{"tm",     Country::Turkmenistan   },
	{"tr",     Country::Turkey         }, // QWERTY
	{"tr440",  Country::Turkey         }, // Non-standard
	{"tt",     Country::Russia         }, // Tatarstan Standard
	{"tt443",  Country::Russia         }, // Tatarstan Typewriter
	{"ua",     Country::Ukraine        }, // 101-key
	{"uk",     Country::United_Kingdom }, // Standard
	{"uk168",  Country::United_Kingdom }, // Allternate
	{"ur",     Country::Ukraine        }, // 101-key
	{"ur465",  Country::Ukraine        }, // 101-key
	{"ur1996", Country::Ukraine        }, // 101-key
	{"ur2001", Country::Ukraine        }, // 102-key
	{"ur2007", Country::Ukraine        }, // 102-key
	{"us",     Country::United_States  }, // Standard
	{"ux",     Country::United_States  }, // International
	{"uz",     Country::Uzbekistan     },
	{"vi",     Country::Vietnam        },
	{"yc",     Country::Serbia         }, // Deadkey
	{"yc450",  Country::Serbia         }, // No-deadkey
	{"yu",     Country::Yugoslavia     },
        // clang-format on
};

static const std::map<std::string, const char *> language_to_layout_exception_map{
        {"nl", "us"},
};

static bool country_number_exists(const int requested_number)
{
	for ([[maybe_unused]] const auto &[code, number] : country_code_map)
		if (requested_number == static_cast<int>(number))
			return true;
	return false;
}

static bool lookup_country_from_code(const char *country_code, Country &country)
{
	if (country_code) {
		const auto it = country_code_map.find(country_code);
		if (it != country_code_map.end()) {
			country = it->second;
			return true;
		}
	}
	return false;
}

static const char *lookup_language_to_layout_exception(const char *language_code)
{
	if (language_code) {
		const auto it = language_to_layout_exception_map.find(language_code);
		if (it != language_to_layout_exception_map.end())
			return it->second;
	}
	return language_code;
}

uint16_t assert_codepage(const uint16_t codepage)
{
	assert(!sv{get_builtin_cp_filename(codepage)}.empty());
	return codepage;
}

[[maybe_unused]] constexpr uint16_t lookup_codepage_from_country(const Country country)
{
	// grouped in ascending ordered by codepage value
	switch (country) {
	case Country::Poland: return assert_codepage(668);

	case Country::Lithuania: return assert_codepage(774);

	case Country::Belgium:
	case Country::Finland:
	case Country::France:
	case Country::Germany:
	case Country::Italy:
	case Country::Latin_America:
	case Country::Netherlands:
	case Country::Spain:
	case Country::Sweden:
	case Country::Switzerland: return assert_codepage(850);

	case Country::Albania:
	case Country::Croatia:
	case Country::Montenegro:
	case Country::Romania:
	case Country::Slovenia:
	case Country::Turkmenistan: return assert_codepage(852);

	case Country::Bosnia:
	case Country::Bulgaria:
	case Country::Macedonia:
	case Country::Serbia:
	case Country::Yugoslavia: return assert_codepage(855);

	case Country::Turkey: return assert_codepage(857);

	case Country::Brazil:
	case Country::Portugal: return assert_codepage(860);

	case Country::Iceland: return assert_codepage(861);

	case Country::Israel: return assert_codepage(862);

	case Country::Candian_French: return assert_codepage(863);

	case Country::Arabic: return assert_codepage(864);

	case Country::Denmark:
	case Country::Norway: return assert_codepage(865);

	case Country::Russia: return assert_codepage(866);

	case Country::Czech_Slovak: return assert_codepage(867);

	case Country::Greece: return assert_codepage(869);

	case Country::Estonia: return assert_codepage(1116);

	case Country::Latvia: return assert_codepage(1117);

	case Country::Ukraine: return assert_codepage(1125);

	case Country::Belarus: return assert_codepage(1131);

	case Country::Hungary: return assert_codepage(3845);

	case Country::Nigeria: return assert_codepage(30005);

	case Country::Azerbaijan: return assert_codepage(58210);

	case Country::Georgia: return assert_codepage(59829);

	default: return assert_codepage(default_cp_437);
	}
}

// Use OS-specific calls to extra the layout and from there convert it into a language
std::string get_lang_from_host_layout()
{
#if defined(WIN32)
#	include <windows.h>

	WORD cur_kb_layout = LOWORD(GetKeyboardLayout(0));
	WORD cur_kb_sub_id = 0;
	char layout_id_string[KL_NAMELENGTH];

	auto parse_hex_string = [](const char *s) {
		uint32_t value = 0;
		sscanf(s, "%x", &value);
		return value;
	};

	if (GetKeyboardLayoutName(layout_id_string)) {
		if (safe_strlen(layout_id_string) == 8) {
			const int cur_kb_layout_by_name = parse_hex_string(
			        (char *)&layout_id_string[4]);
			layout_id_string[4] = 0;
			const int sub_id    = parse_hex_string(
                                (char *)&layout_id_string[0]);
			if ((cur_kb_layout_by_name > 0) &&
			    (cur_kb_layout_by_name < 65536)) {
				// use layout _id extracted from the layout string
				cur_kb_layout = (WORD)cur_kb_layout_by_name;
			}
			if ((sub_id >= 0) && (sub_id < 100)) {
				// use sublanguage ID extracted from the layout
				// string
				cur_kb_sub_id = (WORD)sub_id;
			}
		}
	}
	// try to match emulated keyboard layout with host-keyboardlayout
	switch (cur_kb_layout) {
	case 1025:  // Saudi Arabia
	case 1119:  // Tamazight
	case 1120:  // Kashmiri
	case 2049:  // Iraq
	case 3073:  // Egypt
	case 4097:  // Libya
	case 5121:  // Algeria
	case 6145:  // Morocco
	case 7169:  // Tunisia
	case 8193:  // Oman
	case 9217:  // Yemen
	case 10241: // Syria
	case 11265: // Jordan
	case 12289: // Lebanon
	case 13313: // Kuwait
	case 14337: // U.A.E
	case 15361: // Bahrain
	case 16385: // Qatar
		return "ar462";

	case 1026: return "bg";    // Bulgarian
	case 1029: return "cz243"; // Czech
	case 1030: return "dk";    // Danish

	case 2055: // German - Switzerland
	case 3079: // German - Austria
	case 4103: // German - Luxembourg
	case 5127: // German - Liechtenstein
	case 1031: // German - Germany
		return "gr";

	case 1032: return "gk"; // Greek
	case 1034: return "sp"; // Spanish - Spain (Traditional Sort)
	case 1035: return "su"; // Finnish

	case 1036:  // French - France
	case 2060:  // French - Belgium
	case 4108:  // French - Switzerland
	case 5132:  // French - Luxembourg
	case 6156:  // French - Monaco
	case 7180:  // French - West Indies
	case 8204:  // French - Reunion
	case 9228:  // French - Democratic Rep. of Congo
	case 10252: // French - Senegal
	case 11276: // French - Cameroon
	case 12300: // French - Cote d'Ivoire
	case 13324: // French - Mali
	case 14348: // French - Morocco
	case 15372: // French - Haiti
	case 58380: // French - North Africa
		return "fr";

	case 1037: return "il"; // Hebrew
	case 1038: return cur_kb_sub_id ? "hu" : "hu208";
	case 1039: return "is161"; // Icelandic

	case 2064: // Italian - Switzerland
	case 1040: // Italian - Italy
		return "it";

	case 3084: return "ca"; // French - Canada
	case 1041: return "jp"; // Japanese

	case 2067: // Dutch - Belgium
	case 1043: // Dutch - Netherlands
		return "nl";

	case 1044: return "no"; // Norwegian (Bokmål)
	case 1045: return "pl"; // Polish
	case 1046: return "br"; // Portuguese - Brazil

	case 2073: // Russian - Moldava
	case 1049: // Russian
		return "ru";

	case 4122: // Croatian (Bosnia/Herzegovina)
	case 1050: // Croatian
		return "hr";

	case 1051: return "sk"; // Slovak
	case 1052: return "sq"; // Albanian - Albania

	case 2077: // Swedish - Finland
	case 1053: // Swedish
		return "sv";

	case 1055: return "tr"; // Turkish
	case 1058: return "ur"; // Ukrainian
	case 1059: return "bl"; // Belarusian
	case 1060: return "si"; // Slovenian
	case 1061: return "et"; // Estonian
	case 1062: return "lv"; // Latvian
	case 1063: return "lt"; // Lithuanian
	case 1064: return "tj"; // Tajik
	case 1066: return "vi"; // Vietnamese
	case 1067: return "hy"; // Armenian - Armenia
	case 1071: return "mk"; // F.Y.R.O. Macedonian
	case 1079: return "ka"; // Georgian
	case 2070: return "po"; // Portuguese - Portugal
	case 2072: return "ro"; // Romanian - Moldava
	case 5146: return "ba"; // Bosnian (Bosnia/Herzegovina)

	case 2058:  // Spanish - Mexico
	case 3082:  // Spanish - Spain (Modern Sort)
	case 4106:  // Spanish - Guatemala
	case 5130:  // Spanish - Costa Rica
	case 6154:  // Spanish - Panama
	case 7178:  // Spanish - Dominican Republic
	case 8202:  // Spanish - Venezuela
	case 9226:  // Spanish - Colombia
	case 10250: // Spanish - Peru
	case 11274: // Spanish - Argentina
	case 12298: // Spanish - Ecuador
	case 13322: // Spanish - Chile
	case 14346: // Spanish - Uruguay
	case 15370: // Spanish - Paraguay
	case 16394: // Spanish - Bolivia
	case 17418: // Spanish - El Salvador
	case 18442: // Spanish - Honduras
	case 19466: // Spanish - Nicaragua
	case 20490: // Spanish - Puerto Rico
	case 21514: // Spanish - United States
	case 58378: // Spanish - Latin America
		return "la";
	}

#endif
	return ""; // default to empty/US
}

// A helper that loads a layout given only a language
KeyboardErrorCode DOS_LoadKeyboardLayoutFromLanguage(const char * language_pref)
{
	assert(language_pref);

	// If a specific language wasn't provided, get it from setup
	std::string language = language_pref;
	if (language == "auto")
		language = SETUP_GetLanguage();

	// Does the language have a country associate with it?
	auto country       = default_country;
	bool found_country = lookup_country_from_code(language.c_str(), country);

	// If we can't find a country for the language, try from the host
	if (!found_country) {
		language      = get_lang_from_host_layout();
		found_country = lookup_country_from_code(language.c_str(), country);
	}
	// Inform the user if we couldn't find a valid country
	if (!language.empty() && !found_country) {
		LOG_WARNING("LAYOUT: A country could not be found for the language: %s",
		            language.c_str());
	}
	// Regardless of the above, carry on with setting up the layout
	const auto codepage = lookup_codepage_from_country(country);
	const auto layout = lookup_language_to_layout_exception(language.c_str());
	const auto result = DOS_LoadKeyboardLayout(layout, codepage, "auto");

	if (result == KEYB_NOERROR) {
		LOG_MSG("LAYOUT: Loaded codepage %d for detected language %s", codepage, language.c_str());
	} else if (country != default_country) {
		LOG_WARNING("LAYOUT: Failed loading codepage %d for detected language %s", codepage, language.c_str());
	}
	return result;
}

class DOS_KeyboardLayout final : public Module_base {
public:
	DOS_KeyboardLayout(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);

		dos.loaded_codepage = default_cp_437; // US codepage already initialized

		loaded_layout = std::make_unique<KeyboardLayout>();

		const char * layoutname=section->Get_string("keyboardlayout");

		// If the use only provided a single value (language), then try using it
		const auto layout_is_one_value = sv(layoutname).find(' ') == std::string::npos;
		if (layout_is_one_value) {
			if (!DOS_LoadKeyboardLayoutFromLanguage(layoutname)) {
				return; // success
			}
		}
		// Otherwise use the layout to get the codepage
		const auto req_codepage = loaded_layout->ExtractCodePage(layoutname);
		loaded_layout->ReadCodePageFile("auto", req_codepage);

		if (loaded_layout->ReadKeyboardFile(layoutname, dos.loaded_codepage)) {
			if (strncmp(layoutname, "auto", 4)) {
				LOG_ERR("LAYOUT: Failed to load keyboard layout %s",
				        layoutname);
			}
		} else {
			const char *lcode = loaded_layout->GetMainLanguageCode();
			if (lcode) {
				LOG_MSG("LAYOUT: DOS keyboard layout loaded with main language code %s for layout %s",lcode,layoutname);
			}
		}

		// Convert UTF-8 [autoexec] section to new code page
		AUTOEXEC_NotifyNewCodePage();
	}

	~DOS_KeyboardLayout(){
		if ((dos.loaded_codepage != default_cp_437) && (CurMode->type == M_TEXT)) {
			INT10_ReloadRomFonts();
			dos.loaded_codepage = default_cp_437; // US codepage
		}
		if (loaded_layout) {
			loaded_layout.reset();
		}
	}
};

static std::unique_ptr<DOS_KeyboardLayout> KeyboardLayout = {};

void DOS_KeyboardLayout_ShutDown(Section* /*sec*/) {
	KeyboardLayout.reset();
}

void DOS_SetCountry(uint16_t countryNo);

static void set_country_from_pref(const int country_pref)
{
	// default to the US
	auto country_number = static_cast<uint16_t>(default_country);

	// If the country preference is valid, use it
	if (country_pref > 0 && country_number_exists(country_pref)) {
		country_number = static_cast<uint16_t>(country_pref);
	} else if (const auto country_code = DOS_GetLoadedLayout(); country_code) {
		if (Country c; lookup_country_from_code(country_code, c)) {
			country_number = static_cast<uint16_t>(c);
		} else {
			LOG_ERR("LANGUAGE: The layout's country code: '%s' does not have a corresponding country",
			        country_code);
		}
	}
	// At this point, the country number is expected to be valid
	assert(country_number_exists(country_number));
	DOS_SetCountry(country_number);
}

void DOS_KeyboardLayout_Init(Section *sec)
{
	assert(sec);
	KeyboardLayout = std::make_unique<DOS_KeyboardLayout>(sec);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&DOS_KeyboardLayout_ShutDown, changeable_at_runtime);

	const auto settings = static_cast<const Section_prop *>(sec);
	set_country_from_pref(settings->Get_int("country"));
}
