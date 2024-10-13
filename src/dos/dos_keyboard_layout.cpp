/*
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
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

#include "../ints/int10.h"
#include "autoexec.h"
#include "bios.h"
#include "bios_disk.h"
#include "callback.h"
#include "dos_inc.h"
#include "dos_locale.h"
#include "drives.h"
#include "mapper.h"
#include "math_utils.h"
#include "regs.h"
#include "setup.h"
#include "string_utils.h"

extern void DOS_UpdateCurrentProgramName();

static void notify_code_page_changed(const bool keyboard_layout_changed = false)
{
	// Re-create various information to match new code page
	DOS_UpdateCurrentProgramName();
	DOS_RefreshCountryInfo(keyboard_layout_changed);
	AUTOEXEC_NotifyNewCodePage();
}

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
		const auto ldp = std::dynamic_pointer_cast<localDrive>(Drives[drive]);
		if (ldp) {
			if (const auto fp = ldp->GetHostFilePtr(fullname, file_perms); fp) {
				return FILE_unique_ptr(fp);
			}
		}
	} catch (...) {}

	// Then try from the local filesystem
	if (const auto fp = fopen(name, file_perms); fp)
		return FILE_unique_ptr(fp);

	// Finally try from a built-in resources
	if (resource_dir)
		if (const auto rp = get_resource_path(resource_dir, name); !rp.empty())
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

static void log_layout_read_error()
{
	LOG_WARNING("DOS: Error reading keyboard layout file: '%s'",
	            strerror(errno));
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
		log_layout_read_error();
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
			log_layout_read_error();
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
			log_layout_read_error();
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
				log_layout_read_error();
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
	if (!strcmp(keyboard_file_name, "none")) {
		return DefaultCodePage437;
	}

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
			return DefaultCodePage437;
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
			return DefaultCodePage437;
		}

		fseek(tempfile.get(), 0, SEEK_SET);
		read_buf_size = fread(read_buf, sizeof(uint8_t), 65535, tempfile.get());
	}
	if (read_buf_size == 0) {
		LOG_WARNING("CODEPAGE: Could not read data from layout file %s", keyboard_file_name);
		return DefaultCodePage437;
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
		return DefaultCodePage437;
	}

	// check all submappings and use them if general submapping or same
	// codepage submapping
	for (uint16_t sub_map=0; (sub_map<submappings); sub_map++) {
		uint16_t submap_cp;

		// read codepage of submapping
		submap_cp=host_readw(&read_buf[start_pos+0x14+sub_map*8]);

		if (submap_cp!=0) return submap_cp;
	}
	return DefaultCodePage437;
}

KeyboardErrorCode KeyboardLayout::ReadCodePageFile(const char *requested_cp_filename, const int32_t codepage_id)
{
	assert(requested_cp_filename);
	std::string cp_filename = requested_cp_filename;

	if (cp_filename.empty() || cp_filename == "none" || codepage_id == dos.loaded_codepage)
		return KEYB_NOERROR;

	if (cp_filename == "auto") {
		cp_filename = DOS_GetBundledCodePageFileName(codepage_id);
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
		const auto upx_id     = std::string_view{"UPX!"};
		const auto scan_buf   = std::string_view{reinterpret_cast<char*>(cpi_buf.data()), scan_size};
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

			notify_code_page_changed();
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

static KeyboardErrorCode load_keyboard_layout(const char* layoutname,
                                              const int32_t codepage,
                                              const char* codepagefile)
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

KeyboardErrorCode DOS_LoadKeyboardLayout(const char* layoutname,
                                         const int32_t codepage,
                                         const char* codepagefile)
{
	const auto result = load_keyboard_layout(layoutname, codepage, codepagefile);
	if (!result) {
		LOG_MSG("DOS: Loaded codepage %d", codepage); // success!
	}

	return result;
}

KeyboardErrorCode DOS_SwitchKeyboardLayout(const char* new_layout, int32_t& tried_cp)
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
const char* DOS_GetLoadedLayout()
{
	if (loaded_layout) {
		return loaded_layout->GetLayoutName();
	}
	return nullptr;
}

// A helper that loads a layout given only a language
KeyboardErrorCode DOS_LoadKeyboardLayoutFromLanguage(const char * language_pref)
{
	assert(language_pref);

	// If a specific language wasn't provided, get it from setup
	std::string language = language_pref;
	if (language == "auto") {
		language = control->GetLanguage();
	}

	// TODO: This code mixes language code with keyboard layout; this should
	//       be cleaned up eventually, possibly we should use
	//       'use get_language_from_os()' from 'cross.h'

	// Does the language have a country associate with it?
	auto country       = DOS_GetDefaultCountry();
	bool found_country = DOS_GetCountryFromLayout(language, country);

	// If we can't find a country for the language, try from the host
	if (!found_country) {
		language      = DOS_GetLayoutFromHost();
		found_country = DOS_GetCountryFromLayout(language, country);
	}
	// Inform the user if we couldn't find a valid country
	if (!language.empty() && !found_country) {
		LOG_WARNING("DOS: A country could not be found for the language: %s",
		            language.c_str());
	}

	// Regardless of the above, carry on with setting up the layout
	const auto codepage = DOS_GetCodePageFromCountry(country);
	const auto layout   = DOS_CheckLanguageToLayoutException(language);
	const auto result   = load_keyboard_layout(layout.c_str(), codepage, "auto");

	if (result == KEYB_NOERROR) {
		LOG_MSG("DOS: Loaded codepage %d for detected language '%s'",
		        codepage,
		        language.c_str());
	} else if (country != DOS_GetDefaultCountry()) {
		LOG_WARNING("DOS: Failed loading codepage %d for detected language '%s'",
		            codepage,
		            language.c_str());
	}
	return result;
}

class DOS_KeyboardLayout final : public Module_base {
public:
	DOS_KeyboardLayout(Section* configuration) : Module_base(configuration)
	{
		Section_prop* section = static_cast<Section_prop*>(configuration);
		assert(section);

		// US codepage already initialized
		dos.loaded_codepage = DefaultCodePage437;

		loaded_layout = std::make_unique<KeyboardLayout>();

		std::string layoutname = section->Get_string("keyboardlayout");

		// If the use only provided a single value (language), then try using it
		constexpr bool reason_keyboard_layout = true;
		const auto layout_is_one_value = layoutname.find(' ') == std::string::npos;
		if (layout_is_one_value) {
			if (!DOS_LoadKeyboardLayoutFromLanguage(layoutname.c_str())) {
				// Success - re-create country information to
				// match new keyboard layout
				DOS_RefreshCountryInfo(reason_keyboard_layout);
				return;
			}
		}
		// Otherwise use the layout to get the codepage
		const auto req_codepage = loaded_layout->ExtractCodePage(layoutname.c_str());
		loaded_layout->ReadCodePageFile("auto", req_codepage);

		if (loaded_layout->ReadKeyboardFile(layoutname.c_str(), dos.loaded_codepage)) {
			if (strncmp(layoutname.c_str(), "auto", 4)) {
				LOG_ERR("DOS: Failed to load keyboard layout '%s'",
				        layoutname.c_str());
			}
		} else {
			const char *lcode = loaded_layout->GetMainLanguageCode();
			if (lcode) {
				LOG_MSG("DOS: Loaded keyboard layout '%s' with main language code '%s'",
				        layoutname.c_str(),
				        lcode);
			}
		}

		constexpr bool keyboard_layout_changed = true;
		notify_code_page_changed(keyboard_layout_changed);
	}

	~DOS_KeyboardLayout()
	{
		if ((dos.loaded_codepage != DefaultCodePage437) &&
		    (CurMode->type == M_TEXT)) {
			INT10_ReloadRomFonts();
			dos.loaded_codepage = DefaultCodePage437; // US codepage
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

void DOS_KeyboardLayout_Init(Section *sec)
{
	assert(sec);
	KeyboardLayout = std::make_unique<DOS_KeyboardLayout>(sec);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&DOS_KeyboardLayout_ShutDown, changeable_at_runtime);
}
