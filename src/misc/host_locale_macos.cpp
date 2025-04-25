/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include "dosbox.h"

#if C_COREFOUNDATION

#include "host_locale.h"

#include "checks.h"
#include "string_utils.h"

#include <CoreFoundation/CoreFoundation.h>
#include <unordered_map>

CHECK_NARROWING();


// ***************************************************************************
// Detection data
// ***************************************************************************

// Constant to mark poor/imprecise keyboard layout mappings
constexpr bool Fuzzy = true;

// Mapping from Macintosh to DOS keyboard layout. Collected using:
// 'System Settings' -> 'Keyboard' -> 'Text Input' -> 'Input Sources' settings
// on macOS 'Sequoia' 15.1.1.
// clang-format off
static const std::unordered_map<int64_t, KeyboardLayoutMaybeCodepage> MacToDosKeyboard = {
	// US (standard, QWERTY/national)
	{  0,     { "us" }             }, // U. S.
	{  15,    { "us" }             }, // Australian
	{  29,    { "us" }             }, // Canadian
	{  252,   { "us" }             }, // ABC
	{ -1,     { "us" }             }, // Unicode Hex Input
	{ -2,     { "us" }             }, // ABC - Extended
	{ -3,     { "us" }             }, // ABC - India
	{ -50,    { "us", 30021 }      }, // Hawaiian
	{ -52,    { "us", 30021 }      }, // Samoan
	{ -26112, { "us", 30034 }      }, // Cherokee - Nation
	{ -26113, { "us", 30034 }      }, // Cherokee - QWERTY
	// US (international, QWERTY)
	{  15000, { "ux" }             }, // U.S. International - PC
	// US (Colemak)
	{  12825, { "co" }             }, // Colemak
	// US (Dvorak)
	{  16300, { "dv" }             }, // Dvorak
	{  16301, { "dv" }             }, // Dvorak - QWERTY
	// US (left-hand Dvorak)
	{  16302, { "lh" }             }, // Dvorak Left-Handed
	// US (right-hand Dvorak)
	{  16303, { "rh" }             }, // Dvorak Right-Handed
	// UK (standard, QWERTY)
	{  2,     { "uk" }             }, // British
	{  50,    { "uk" }             }, // Irish
	{ -500,   { "uk" }             }, // Irish - Extended
	{ -790,   { "uk", 30001 }      }, // Welsh
	// UK (alternate, QWERTY)
	{  250,   { "uk168" }          }, // British - PC
	// Arabic (AZERTY/Arabic)
	{ -17920, { "ar462" }          }, // Arabic
	{ -17923, { "ar462" }          }, // Arabic - 123
	{ -17940, { "ar462" }          }, // Arabic - AZERTY
	{ -17921, { "ar462" }          }, // Arabic - PC
	{ -2902,  { "ar462" }          }, // Afghan Dari
	{ -2904,  { "ar462" }          }, // Afghan Pashto
	{ -2903,  { "ar462" }          }, // Afghan Uzbek
	{ -17960, { "ar462" }          }, // Persian - Legacy
	{ -2901,  { "ar462" }          }, // Persian - Standard
	{ -31486, { "ar462" }          }, // Syriac - Arabic
	// Arabic (QWERTY/Arabic)
	{ -18000, { "ar470" }          }, // Arabic - QWERTY
	{ -19000, { "ar470" }          }, // Jawi
	{ -1959,  { "ar470" }          }, // Persian - QWERTY
	{ -22374, { "ar470" }          }, // Sindhi
	{ -17926, { "ar470" }          }, // Sorani Kurdish
	{ -30291, { "ar470" }          }, // Syriac - QWERTY
	{ -27000, { "ar470" }          }, // Uyghur
	// Azeri (QWERTY/Cyrillic)"
	{ -49,    { "az" }             }, // Azeri
	// Belgian (AZERTY)
	{  6,     { "be" }             }, // Belgian
	// Bulgarian (QWERTY/national)
	{  19528, { "bg" }             }, // Bulgarian - Standard
	// Bulgarian (QWERTY/phonetic)
	{  19529, { "bg103" }          }, // Bulgarian - QWERTY
	// Brazilian (ABNT layout, QWERTY)
	{  128,   { "br" }             }, // Brazilian - ABNT2
	// Brazilian (US layout, QWERTY)
	{  72,    { "br274" }          }, // Brazilian
	{  71,    { "br274" }          }, // Brazilian - Legacy
	// Belarusian (QWERTY/national)
	{  19517, { "by" }             }, // Belarusian
	// Canadian (standard, QWERTY)
	{ -19336, { "cf" }             }, // Canadian - PC
	// Canadian (dual-layer, QWERTY)
	{  80,    { "cf445" }          }, // Canadian - CSA
	// Czech (QWERTZ)
	{ -14193, { "cz" }             }, // Czech
	// Czech (programmers, QWERTY)
	{  30778, { "cz489" }          }, // Czech - QWERTY
	{  30779, { "cz489" }          }, // Slovak - QWERTY
	// German (standard, QWERTZ)
	{  3,     { "de" }             }, // German
	{ -18133, { "de" }             }, // German - Standard
	{  92,    { "de" }             }, // Austrian
	{  253,   { "de", 437 }        }, // ABC - QWERTZ
	// Danish (QWERTY)
	{  9,     { "dk" }             }, // Danish
	// Estonian (QWERTY)
	{  30764, { "ee" }             }, // Estonian
	// Spanish (QWERTY)
	{  87,    { "es" }             }, // Spanish
	{  8,     { "es" }             }, // Spanish - Legacy
	// Finnish (QWERTY/ASERTT)
	{  17,    { "fi" }             }, // Finnish
	{ -17,    { "fi" }             }, // Finnish - Extended
	{ -18,    { "fi", 30000 }      }, // Finnish Sámi - PC
	{ -1202,  { "fi", 30000 }      }, // Inari Sámi
	{ -1206,  { "fi", 30000 }      }, // Skolt Sámi
	// Faroese (QWERTY)
	{ -47,    { "fo" }             }, // Faroese
	// French (standard, AZERTY)
	{  1,     { "fr" }             }, // French
	{  60,    { "fr" }             }, // French - PC
	{  1111,  { "fr" }             }, // French - Numerical
	{  251,   { "fr", 437 }        }, // ABC - AZERTY
	// French (international, AZERTY)
	// TODO: Is 30024 or 30025 a better one for the ADLaM/Wolof languages?
	{ -29472, { "fx", 30025 }      }, // Adlam
	// Greek (459, non-standard/national)
	{ -18944, { "gk459" }          }, // Greek
	{ -18945, { "gk459" }          }, // Greek Polytonic
	// Croatian (QWERTZ/national)
	{ -69,    { "hr" }             }, // Croatian - QWERTZ
	// Hungarian (101-key, QWERTY)
	{  30767, { "hu" }             }, // Hungarian - QWERTY
	// Hungarian (102-key, QWERTZ)
	{  30763, { "hu208" }          }, // Hungarian
	// Armenian (QWERTY/national)
	{ -28161, { "hy" }             }, // Armenian - HM QWERTY
	{ -28164, { "hy" }             }, // Armenian - Western QWERTY
	// Hebrew (QWERTY/national)
	{ -18432, { "il" }             }, // Hebrew
	{ -18433, { "il" }             }, // Hebrew - PC
	{ -18500, { "il" }             }, // Hebrew - QWERTY
	{ -18501, { "il" }             }, // Yiddish - QWERTY
	// Icelandic (101-key, QWERTY)
	{ -21,    { "is" }             }, // Icelandic
	// Italian (standard, QWERTY/national)
	{  223,   { "it" }             }, // Italian
	// Georgian (QWERTY/national)
	{ -27650, { "ka" }             }, // Georgian - QWERTY
	// Kazakh (476, QWERTY/national)
	{ -19501, { "kk476" }          }, // Kazakh
	// Kyrgyz (QWERTY/national)
	{  19459, { "ky" }             }, // Kyrgyz
	// Latin American (QWERTY)
	{  89,    { "la" }             }, // Latin American
	// Lithuanian (Baltic, QWERTY/phonetic)
	{  30761, { "lt" }             }, // Lithuanian
	// Latvian (standard, QWERTY/phonetic)
	{  30765, { "lv" }             }, // Latvian
	// Macedonian (QWERTZ/national)
	{  19523, { "mk" }             }, // Macedonian
	// Mongolian (QWERTY/national)
	{ -2276,  { "mn" }             }, // Mongolian
	// Maltese (UK layout, QWERTY)
	{ -501,   { "mt" }             }, // Maltese
	// Nigerian (QWERTY)
	{ -2461,  { "ng" }             }, // Hausa
	{ -32355, { "ng" }             }, // Igbo
	{ -32377, { "ng" }             }, // Yoruba
	// Dutch (QWERTY)
	{  26,    { "nl" }             }, // Dutch
	// Norwegian (QWERTY/ASERTT)
	{  12,    { "no" }             }, // Norwegian
	{ -12,    { "no" }             }, // Norwegian - Extended
	{ -1209,  { "no", 30000 }      }, // Lule Sámi (Norway)
	{ -1200,  { "no", 30000 }      }, // North Sámi
	{ -1201,  { "no", 30000 }      }, // North Sámi - PC
	{ -13,    { "no", 30000 }      }, // Norwegian Sámi - PC
	{ -1207,  { "no", 30000 }      }, // South Sámi
	// Polish (programmers, QWERTY/phonetic)
	{  30788, { "pl" }             }, // Polish
	// Polish (typewriter, QWERTZ/phonetic)
	{  30762, { "pl214" }          }, // Polish - QWERTZ
	// Portuguese (QWERTY)
	{  10,    { "po" }             }, // Portuguese
	// Romanian (QWERTY/phonetic)
	{ -39,    { "ro446" }          }, // Romanian
	{ -38,    { "ro446" }          }, // Romanian - Standard
	// Russian (standard, QWERTY/national)
	{  19456, { "ru" }             }, // Russian
	{  19458, { "ru" }             }, // Russian - PC
	{  19457, { "ru" }             }, // Russian - QWERTY
	// Russian (extended standard, QWERTY/national)
	{ -14457, { "rx", 30013 }      }, // Chuvash
	{  23978, { "rx", 30011 }      }, // Ingush
	{  19690, { "rx", 30017 }      }, // Kildin Sámi
	// Swiss (German, QWERTZ)
	{  19,    { "sd" }             }, // Swiss German
	// Swiss (French, QWERTZ)
	{  18,    { "sf" }             }, // Swiss French
	// Slovak (QWERTZ)
	{ -11013, { "sk" }             }, // Slovak
	// Albanian (deadkeys, QWERTZ)
	{ -31882, { "sq448" }          }, // Albanian
	// Swedish (QWERTY/ASERTT)
	{  7,     { "sv" }             }, // Swedish
	{  224,   { "sv" }             }, // Swedish - Legacy
	{ -15,    { "sv", 30000 }      }, // Swedish Sámi - PC
	{ -1203,  { "sv", 30000 }      }, // Lule Sámi (Sweden)
	{ -1205,  { "sv", 30000 }      }, // Pite Sámi
	{ -1208,  { "sv", 30000 }      }, // Ume Sámi
	// Tajik (QWERTY/national)
	{  19460, { "tj" }             }, // Tajik (Cyrillic)
	// Turkmen (QWERTY/phonetic)
	{  15228, { "tm" }             }, // Turkmen
	// Turkish (QWERTY)
	{ -36,    { "tr" }             }, // Turkish Q
	{ -35,    { "tr" }             }, // Turkish Q - Legacy
	// Turkish (non-standard)
	{ -5482,  { "tr440" }          }, // Turkish F
	{ -24,    { "tr440" }          }, // Turkish F - Legacy
	// Ukrainian (101-key, QWERTY/national)
	{ -2354,  { "ua" }             }, // Ukrainian
	{  19518, { "ua" }             }, // Ukrainian - Legacy  
	{ -23205, { "ua" }             }, // Ukrainian - QWERTY
	// Uzbek (QWERTY/national)
	{  19461, { "uz" }             }, // Uzbek (Cyrillic)
	// Vietnamese (QWERTY)
	{ -31232, { "vi" }             }, // Vietnamese

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the QWERTY layout is typically used
	{ -32044, { "us", {},  Fuzzy } }, // Akan
	{ -18940, { "us", {},  Fuzzy } }, // Apache
	{ -14789, { "us", {},  Fuzzy } }, // Assamese - InScript
	{ -22528, { "us", {},  Fuzzy } }, // Bangla - InScript 
	{ -22529, { "us", {},  Fuzzy } }, // Bangla - QWERTY
	{ -11396, { "us", {},  Fuzzy } }, // Bodo
	{ -18438, { "us", {},  Fuzzy } }, // Chickasaw
	{ -17340, { "us", {},  Fuzzy } }, // Choctaw
	{ -20481, { "us", {},  Fuzzy } }, // Devanagari - QWERTY
	{ -17410, { "us", {},  Fuzzy } }, // Dhivehi - QWERTY
	{ -25281, { "us", {},  Fuzzy } }, // Dogri
	{ -2728,  { "us", {},  Fuzzy } }, // Dzongkha
	{ -27432, { "us", {},  Fuzzy } }, // Ge'ez
	{ -21504, { "us", {},  Fuzzy } }, // Gujarati - InScript
	{ -21505, { "us", {},  Fuzzy } }, // Gujarati - QWERTY
	{ -20992, { "us", {},  Fuzzy } }, // Gurmukhi - InScript
	{ -20993, { "us", {},  Fuzzy } }, // Gurmukhi - QWERTY
	{ -27472, { "us", {},  Fuzzy } }, // Hanifi Rohingya
	{ -20480, { "us", {},  Fuzzy } }, // Hindi - InScript
	{ -20564, { "us", {},  Fuzzy } }, // Hmong (Pahawh)
	{ -30606, { "us", {},  Fuzzy } }, // Inuktitut - Nattilik
	{ -30602, { "us", {},  Fuzzy } }, // Inuktitut - Nutaaq
	{ -30603, { "us", {},  Fuzzy } }, // Inuktitut - Nunavik
	{ -30604, { "us", {},  Fuzzy } }, // Inuktitut - Nunavut
	{ -30600, { "us", {},  Fuzzy } }, // Inuktitut - QWERTY
	{  11538, { "us", {},  Fuzzy } }, // Kabyle - QWERTY
	{ -24064, { "us", {},  Fuzzy } }, // Kannada - InScript
	{ -24065, { "us", {},  Fuzzy } }, // Kannada - QWERTY
	{ -22530, { "us", {},  Fuzzy } }, // Kashmiri (Devanagari)
	{ -26114, { "us", {},  Fuzzy } }, // Khmer
	{ -25282, { "us", {},  Fuzzy } }, // Konkani
	{ -361,   { "us", {},  Fuzzy } }, // Kurmanji Kurdish
	{ -26115, { "us", {},  Fuzzy } }, // Lao
	{ -23562, { "us", {},  Fuzzy } }, // Lushootseed
	{ -25283, { "us", {},  Fuzzy } }, // Maithili - InScript
	{ -24576, { "us", {},  Fuzzy } }, // Malayalam - InScript
	{ -24577, { "us", {},  Fuzzy } }, // Malayalam - QWERTY
	{ -3047,  { "us", {},  Fuzzy } }, // Mandaic - Arabic
	{ -17993, { "us", {},  Fuzzy } }, // Mandaic - QWERTY
	{ -22532, { "us", {},  Fuzzy } }, // Manipuri (Bengali)
	{ -22534, { "us", {},  Fuzzy } }, // Manipuri (Meetei Mayek)
	{ -51,    { "us", {},  Fuzzy } }, // Māori - InScript
	{ -25284, { "us", {},  Fuzzy } }, // Marathi
	{ -13161, { "us", {},  Fuzzy } }, // Mi'kmaq
	{ -23561, { "us", {},  Fuzzy } }, // Mvskoke
	{ -25602, { "us", {},  Fuzzy } }, // Myanmar
	{ -25601, { "us", {},  Fuzzy } }, // Myanmar - QWERTY
	{ -25709, { "us", {},  Fuzzy } }, // N'Ko - QWERTY
	{ -18939, { "us", {},  Fuzzy } }, // Navajo
	{ -25286, { "us", {},  Fuzzy } }, // Nepali - InScript
	{ -31135, { "us", {},  Fuzzy } }, // Nepali - Remington
	{ -22016, { "us", {},  Fuzzy } }, // Odiya - InScript
	{ -22017, { "us", {},  Fuzzy } }, // Odiya - QWERTY
	{  38342, { "us", {},  Fuzzy } }, // Osage - QWERTY
	{ -20563, { "us", {},  Fuzzy } }, // Rejang - QWERTY
	{ -23064, { "us", {},  Fuzzy } }, // Sanskrit
	{ -22538, { "us", {},  Fuzzy } }, // Santali (Devanagari) - InScript
	{ -22536, { "us", {},  Fuzzy } }, // Santali - (Ol Chiki)
	{ -16901, { "us", {},  Fuzzy } }, // Sindhi (Devanagari) - InScript
	{ -25088, { "us", {},  Fuzzy } }, // Sinhala
	{ -25089, { "us", {},  Fuzzy } }, // Sinhala - QWERTY
	{ -23552, { "us", {},  Fuzzy } }, // Telugu - InScript
	{ -23553, { "us", {},  Fuzzy } }, // Telugu - QWERTY
	{ -26624, { "us", {},  Fuzzy } }, // Thai
	{ -24616, { "us", {},  Fuzzy } }, // Thai - Pattachote
	{ -26628, { "us", {},  Fuzzy } }, // Tibetan - Otani
	{ -26625, { "us", {},  Fuzzy } }, // Tibetan - QWERTY
	{ -2398,  { "us", {},  Fuzzy } }, // Tibetan - Wylie
	{  88,    { "us", {},  Fuzzy } }, // Tongan
	{ -17925, { "us", {},  Fuzzy } }, // Urdu
	{ -23498, { "us", {},  Fuzzy } }, // Wancho - QWERTY
	{  4300,  { "us", {},  Fuzzy } }, // Wolastoqey

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the AZERTY layout is typically used
	{  6983,  { "fr", 437, Fuzzy } }, // Kabyle - AZERTY
	{ -25708, { "fr", 437, Fuzzy } }, // N'Ko
	{ -12482, { "fr", 437, Fuzzy } }, // Tifinagh - AZERTY

	// In some cases we do not have a matching QWERTY layout; if so, use
	// the US/International keyboard with the best available code page
	{ -68,    { "ux", 850, Fuzzy } }, // Croatian - QWERTY
	{  19521, { "us", 855, Fuzzy } }, // Serbian
	{ -19521, { "ux", 850, Fuzzy } }, // Serbian (Latin)
	{ -66,    { "ux", 850, Fuzzy } }, // Slovenian
};
// clang-format on

// ***************************************************************************
// Generic helper routines
// ***************************************************************************

static std::string to_string(const CFStringRef string_ref)
{
	if (!string_ref) {
		return {};
	}

	constexpr auto Encoding = kCFStringEncodingASCII;

	const char* buffer = CFStringGetCStringPtr(string_ref, Encoding);
	if (buffer != nullptr) {
		return buffer;
	}

	const auto max_length = CFStringGetMaximumSizeForEncoding(
	        CFStringGetLength(string_ref), Encoding);

	std::string result = {};
	result.resize(max_length + 1);

	if (!CFStringGetCString(string_ref, result.data(), max_length + 1, Encoding)) {
		// Conversion attempt failed
		return {};
	}

	return result.substr(0, result.find('\0'));
}

// ***************************************************************************
// Detection routines
// ***************************************************************************

static std::string get_locale(const CFLocaleKey key)
{
	const auto locale_ref = CFLocaleCopyCurrent();
	const auto value_ref  = CFLocaleGetValue(locale_ref, key);

	const auto result = to_string(static_cast<CFStringRef>(value_ref));

	CFRelease(locale_ref);
	return result;
}

static HostLocaleElement get_dos_country()
{
	HostLocaleElement result = {};

	const auto language = get_locale(kCFLocaleLanguageCode);
	const auto country  = get_locale(kCFLocaleCountryCode);

	result.log_info = language + "-" + country;

	result.country_code = iso_to_dos_country(language, country);
	return result;
}

static std::vector<std::string> get_preferred_languages()
{
	std::vector<std::string> result = {};

	const auto languages_ref = CFLocaleCopyPreferredLanguages();

	// We expect the root of the file to be an array
	if (CFGetTypeID(languages_ref) != CFArrayGetTypeID()) {
		CFRelease(languages_ref);
		return {};
	}

	// Get all the array elements
	const size_t num_languages = CFArrayGetCount(languages_ref);
	for (size_t idx = 0; idx < num_languages; ++idx) {
		const auto language_ref = CFArrayGetValueAtIndex(languages_ref, idx);

		// We expect the language value to be a string
		if (CFGetTypeID(language_ref) != CFStringGetTypeID()) {
			continue;
		}

		const auto language_str = to_string(
		        static_cast<CFStringRef>(language_ref));
		if (!language_str.empty()) {
			result.push_back(language_str);
		}
	}

	CFRelease(languages_ref);
	return result;
}

static HostLanguages get_host_languages()
{
	HostLanguages result = {};

	auto get_language_file = [&](const std::string& input) -> std::string {
		const auto tokens = split(input, "-");
		if (tokens.empty()) {
			return {};
		}
		if (tokens.size() == 1) {
			return iso_to_language_file (tokens.at(0), "");
		}
		return iso_to_language_file(tokens.at(0), tokens.at(1));
	};

	// Get the list of preferred languages
	const auto preferred_languages = get_preferred_languages();
	for (const auto& entry : preferred_languages) {
		if (!result.log_info.empty()) {
			result.log_info += ", ";
		}
		result.log_info += entry;

		result.language_files.push_back(get_language_file(entry));
	}

	// Get the GUI language
	const auto language  = get_locale(kCFLocaleLanguageCode);
	const auto territory = get_locale(kCFLocaleCountryCode);

	if (!language.empty()) {
		if (!result.log_info.empty()) {
			result.log_info += "; ";
		}
		result.log_info += "GUI: ";
		result.log_info += language + "-" + territory;

		result.language_file_gui = iso_to_language_file(language, territory);
	}

	return result;
}

using AppleLayouts = std::vector<std::pair<int64_t, std::string>>;

static HostKeyboardLayouts get_host_keyboard_layouts(const AppleLayouts& apple_layouts)
{
	HostKeyboardLayouts result = {};
	auto& result_list = result.keyboard_layout_list;

	for (const auto& [layout_id, layout_name] : apple_layouts) {
		// LOG_MSG("{ %- lld, { \"\" }         }, // %s",
		//         layout_id, layout_name.c_str());
		if (!result.log_info.empty()) {
			result.log_info += "; ";
		}
		result.log_info += std::to_string(layout_id) + " (" +
		                   layout_name + ")";

		if (MacToDosKeyboard.contains(layout_id)) {
			result_list.push_back(MacToDosKeyboard.at(layout_id));
		}
	}

	return result;
}

static CFURLRef create_home_url()
{
	// Retrieve home directory from the environment
	const char *home_dir = getenv("HOME");
	if (!home_dir) {
		return nullptr;
	}

	// Create string containing home directory name
	const auto home_string_ref = CFStringCreateWithCString(kCFAllocatorDefault,
		home_dir, kCFStringEncodingUTF8);
	if (!home_string_ref) {
		return nullptr;
	}

	constexpr bool UrlToDirectory = true;

	// Create home driectory URL
	const auto home_url_ref = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
		home_string_ref, kCFURLPOSIXPathStyle, UrlToDirectory);
	CFRelease(home_string_ref);
	
	return home_url_ref;
}

static CFPropertyListRef read_plist_file()
{
	const auto InputFile = CFSTR("Library/Preferences/com.apple.HIToolbox.plist");

	// Create home driectory URL
	const auto home_url_ref = create_home_url();
	if (!home_url_ref) {
		return nullptr;
	}

	constexpr bool UrlToFile = false;

	// Create property file URL
	const auto file_url_ref = CFURLCreateWithFileSystemPathRelativeToBase(kCFAllocatorDefault,
		InputFile, kCFURLPOSIXPathStyle, UrlToFile, home_url_ref);
	CFRelease(home_url_ref);
	if (!file_url_ref) {
		return nullptr;
	}

	// Create stream for reading
	const auto stream_ref = CFReadStreamCreateWithFile(kCFAllocatorDefault,
		file_url_ref);
	CFRelease(file_url_ref);
	if (!stream_ref) {
		return nullptr;
	}

	// Open the stream
	if (!CFReadStreamOpen(stream_ref)) {
		CFRelease(stream_ref);
		return nullptr;		
	}

	constexpr CFIndex       ReadUntilEnd = 0;
	constexpr CFOptionFlags NoOptions    = {};

	// Read the property list from the stream
	const auto plist_ref = CFPropertyListCreateWithStream(kCFAllocatorDefault,
		stream_ref, ReadUntilEnd, NoOptions, nullptr, nullptr);
	CFRelease(stream_ref);

	return plist_ref;
}

static HostKeyboardLayouts get_host_keyboard_layouts()
{
	constexpr auto MaxIntSize = static_cast<CFIndex>(sizeof(int64_t));

	const auto KeyMain = CFSTR("AppleEnabledInputSources");

	const auto KeySourceKind = CFSTR("InputSourceKind");
	const auto KeyLayoutName = CFSTR("KeyboardLayout Name");
	const auto KeyLayoutId   = CFSTR("KeyboardLayout ID");

	const std::string keyboardLayoutSourceKind = "Keyboard Layout";

	AppleLayouts apple_layouts = {};

	// Read the property list with keyboard settings
	auto plist_ref = read_plist_file();
	if (!plist_ref) {
		return {};
	}

	// We expect the root of the file to be a dictionary
	if (CFGetTypeID(plist_ref) != CFDictionaryGetTypeID()) {
		CFRelease(plist_ref);
		return {};
	}

	// Find the array containing input sources
	const auto root_ref = static_cast<CFDictionaryRef>(plist_ref);
	if (!CFDictionaryContainsKey(root_ref, KeyMain)) {
		CFRelease(plist_ref);
		return {};
	}
	const auto array_ptr = CFDictionaryGetValue(root_ref, KeyMain);
	if (!array_ptr || CFGetTypeID(array_ptr) != CFArrayGetTypeID()) {
		CFRelease(plist_ref);
		return {};
	}
	const auto array_ref = static_cast<CFArrayRef>(array_ptr);

	// Loop over all the input sources
	const auto sources_count = CFArrayGetCount(array_ref);
	for (auto index = 0; index < sources_count; ++index) {
		// NOTE: this code does not recognize logographic (Chinese,
		// Korean, Japanese, etc.) writing systems nor transliteration
		// input sources; their configuration is much more complex,
		// plus we do not support these alphabets anyway.

		// Make sure the array element is a dictionary, skip otherwise
		const auto entry_ptr = CFArrayGetValueAtIndex(array_ref, index);
		if (!entry_ptr || CFGetTypeID(entry_ptr) != CFDictionaryGetTypeID()) {
			continue;
		}
		const auto entry_ref = static_cast<CFDictionaryRef>(entry_ptr);

		// Make sure the entry is a keyboard layout, skip otherwise
		if (!CFDictionaryContainsKey(entry_ref, KeySourceKind)) {
			continue;
		}
		const auto source_kind_ptr = CFDictionaryGetValue(entry_ref, KeySourceKind);
		if (!source_kind_ptr || CFGetTypeID(source_kind_ptr) != CFStringGetTypeID()) {
			continue;		
		}
		const auto source_kind_ref = static_cast<CFStringRef>(source_kind_ptr);
		if (to_string(source_kind_ref) != keyboardLayoutSourceKind) {
			continue;
		}

		// Retrieve keyboard layout name and identifier
		if (!CFDictionaryContainsKey(entry_ref, KeyLayoutName) ||
		    !CFDictionaryContainsKey(entry_ref, KeyLayoutId)) {
			continue;
		}
		const auto layout_name_ptr = CFDictionaryGetValue(entry_ref, KeyLayoutName);
		const auto layout_id_ptr   = CFDictionaryGetValue(entry_ref, KeyLayoutId);
		if (!layout_name_ptr || !layout_id_ptr ||
		    CFGetTypeID(layout_name_ptr) != CFStringGetTypeID() ||
		    CFGetTypeID(layout_id_ptr) != CFNumberGetTypeID()) {
			continue;
		}
		const auto layout_id_ref  = static_cast<CFNumberRef>(layout_id_ptr);
		if (CFNumberIsFloatType(layout_id_ref) ||
		    CFNumberGetByteSize(layout_id_ref) > MaxIntSize) {
		    	continue;
		}
		const auto layout_name_ref = static_cast<CFStringRef>(layout_name_ptr);

		// Convert keyboard layout name and identifier to C++ types
		const auto layout_name = to_string(layout_name_ref);
		int64_t layout_id = 0;
		CFNumberGetValue(layout_id_ref, kCFNumberSInt64Type, &layout_id);

		apple_layouts.emplace_back(layout_id, layout_name);
	}

	CFRelease(plist_ref);
	return get_host_keyboard_layouts(apple_layouts);
}

bool IsMonetaryUtf8([[maybe_unused]] const std::locale& locale)
{
	return false;
}

const HostLocale& GetHostLocale()
{
	static std::optional<HostLocale> locale = {};

	if (!locale) {
		locale = HostLocale();

		locale->country = get_dos_country();
	}

	return *locale;
}

const HostKeyboardLayouts& GetHostKeyboardLayouts()
{
	static std::optional<HostKeyboardLayouts> locale = {};

	if (!locale) {
		locale = get_host_keyboard_layouts();
	}

	return *locale;
}

const HostLanguages& GetHostLanguages()
{
	static std::optional<HostLanguages> locale = {};

	if (!locale) {
		locale = get_host_languages();
	}

	return *locale;
}

#endif
