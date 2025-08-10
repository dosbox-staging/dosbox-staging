// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "host_locale.h"

#if defined(WIN32)

#include "util/checks.h"
#include "util/string_utils.h"

#include <stringapiset.h>
#include <unordered_map>
#include <windows.h>
#include <winnls.h>

CHECK_NARROWING();

// Constant to mark poor/imprecise keyboard layout mappings
constexpr bool Fuzzy = true;

// Mapping from modern Windows to DOS keyboard layouts. Developed using
// https://kbdlayout.info web page for layout visualization
// clang-format off
static const std::unordered_map<std::string, KeyboardLayoutMaybeCodepage> WinToDosKeyboard = {
	// US (standard, QWERTY/national)
	{ "00000409", { "us" }         }, // US
	{ "00050409", { "us" }         }, // US English Table for IBM Arabic 238_L
	{ "00004009", { "us" }         }, // English (India)
	{ "0000085f", { "us" }         }, // Central Atlas Tamazight
	{ "0000045c", { "us", 30034 }  }, // Cherokee Nation
	{ "0001045c", { "us", 30034 }  }, // Cherokee Phonetic
	{ "00000475", { "us", 30021 }  }, // Hawaiian
	{ "00000481", { "us", 30021 }  }, // Maori
	{ "00001409", { "us", 30021 }  }, // NZ Aotearoa
	// US (international, QWERTY)
	{ "00020409", { "ux" }         }, // United States-International
	{ "00000432", { "ux", 30023 }  }, // Setswana
	// US (Colemak)
	{ "00060409", { "co" }         }, // Colemak
	// US (Dvorak)
	{ "00010409", { "dv" }         }, // United States-Dvorak
	// US (left-hand Dvorak)
	{ "00030409", { "lh" }         }, // United States-Dvorak for left hand
	// US (right-hand Dvorak)
	{ "00040409", { "rh" }         }, // United States-Dvorak for right hand
	// UK (standard, QWERTY)
	{ "00000809", { "uk" }         }, // United Kingdom
	{ "00000452", { "uk" }         }, // United Kingdom Extended
	{ "00001809", { "uk" }         }, // Irish
	{ "00011809", { "uk", 30001 }  }, // Scottish Gaelic
	// UK (international, QWERTY)
	{ "0000046c", { "kx", 30023 }  }, // Sesotho sa Leboa
	// Arabic (AZERTY/Arabic)
	{ "00020401", { "ar462" }      }, // Arabic (102) AZERTY
	// Arabic (QWERTY/Arabic)
	{ "00000401", { "ar470" }      }, // Arabic (101)
	{ "00010401", { "ar470" }      }, // Arabic (102)
	{ "00000492", { "ar470" }      }, // Central Kurdish
	{ "00000463", { "ar470" }      }, // Pashto (Afghanistan)
	{ "00000429", { "ar470" }      }, // Persian
	{ "00050429", { "ar470" }      }, // Persian (Standard)
	{ "00010480", { "ar470" }      }, // Uyghur
	{ "00000480", { "ar470" }      }, // Uyghur (Legacy)
	// Azeri (QWERTY/Cyrillic)"
	{ "0001042c", { "az" }         }, // Azerbaijani (Standard)
	{ "0000082c", { "az" }         }, // Azerbaijani Cyrillic
	{ "0000042c", { "az" }         }, // Azerbaijani Latin
	// Bosnian (QWERTZ)
	{ "0000201a", { "ba" }         }, // Bosnian (Cyrillic)
	// Belgian (AZERTY)
	{ "0001080c", { "be" }         }, // Belgian (Comma)
	{ "00000813", { "be" }         }, // Belgian (Period)
	{ "0000080c", { "be" }         }, // Belgian French
	// Bulgarian (QWERTY/national)
	{ "00030402", { "bg" }         }, // Bulgarian
	{ "00010402", { "bg" }         }, // Bulgarian (Latin)
	{ "00000402", { "bg" }         }, // Bulgarian (Typewriter)
	// Bulgarian (QWERTY/phonetic)
	{ "00040402", { "bg103" }      }, // Bulgarian (Phonetic Traditional)
	{ "00020402", { "bg103" }      }, // Bulgarian (Phonetic)
	// Brazilian (ABNT layout, QWERTY)
	{ "00000416", { "br" }         }, // Portuguese (Brazil ABNT)
	{ "00010416", { "br" }         }, // Portuguese (Brazil ABNT2)
	// Belarusian (QWERTY/national)
	{ "00000423", { "by" }         }, // Belarusian
	// Canadian (standard, QWERTY)
	{ "00001009", { "cf" }         }, // Canadian French
	{ "0000085d", { "cf", 30022 }  }, // Inuktitut - Latin
	{ "0001045d", { "cf", 30022 }  }, // Inuktitut - Naqittaut
	// Canadian (dual-layer, QWERTY)
	{ "00000c0c", { "cf445" }      }, // Canadian French (Legacy)
	{ "00011009", { "cf445" }      }, // Canadian Multilingual Standard
	// Czech (QWERTZ)
	{ "00000405", { "cz" }         }, // Czech
	// Czech (programmers, QWERTY)
	{ "00010405", { "cz489" }      }, // Czech (QWERTY)
	{ "00020405", { "cz489" }      }, // Czech Programmers
	{ "0001041b", { "cz489" }      }, // Slovak (QWERTY)
	// German (standard, QWERTZ)
	{ "00000407", { "de" }         }, // German
	{ "00010407", { "de" }         }, // German (IBM)
	// German (dual-layer, QWERTZ)
	{ "00020407", { "gr453" }      }, // German Extended (E1)
	{ "00030407", { "gr453" }      }, // German Extended (E2)
	{ "0001042e", { "gr453", 852 } }, // Sorbian Extended
	{ "0002042e", { "gr453", 852 } }, // Sorbian Standard
	{ "0000042e", { "gr453", 852 } }, // Sorbian Standard (Legacy)
	// Danish (QWERTY)
	{ "00000406", { "dk" }         }, // Danish
	{ "0000046f", { "dk", 30004 }  }, // Greenlandic
	// Estonian (QWERTY)
	{ "00000425", { "ee" }         }, // Estonian
	// Spanish (QWERTY)
	{ "0000040a", { "es" }         }, // Spanish
	{ "0001040a", { "es" }         }, // Spanish Variation
	// Finnish (QWERTY/ASERTT)
	{ "0000040b", { "fi" }         }, // Finnish
	{ "0001083b", { "fi", 30000 }  }, // Finnish with Sami
	// Faroese (QWERTY)
	{ "00000438", { "fo" }         }, // Faeroese
	// French (standard, AZERTY)
	{ "0000040c", { "fr" }         }, // French (Legacy, AZERTY)
	{ "0001040c", { "fr" }         }, // French (Standard, AZERTY)
	{ "0002040c", { "fr" }         }, // French (Standard, BÉPO)
	// French (international, AZERTY)
	// TODO: Is 30024 or 30025 a better one for the ADLaM/Wolof languages?
	{ "00140c00", { "fx", 30025 }  }, // ADLaM
	{ "00000488", { "fx", 30025 }  }, // Wolof
	// Greek (319, QWERTY/national)
	{ "00000408", { "gk" }         }, // Greek
	{ "00050408", { "gk" }         }, // Greek Latin
	{ "00060408", { "gk" }         }, // Greek Polytonic
	{ "00020408", { "gk" }         }, // Greek (319)
	{ "00040408", { "gk" }         }, // Greek (319) Latin
	// Greek (220, QWERTY/national)
	{ "00010408", { "gk220" }      }, // Greek (220)
	{ "00030408", { "gk220" }      }, // Greek (220) Latin
	// Hungarian (101-key, QWERTY)
	{ "0001040e", { "hu" }         }, // Hungarian 101-key
	// Hungarian (102-key, QWERTZ)
	{ "0000040e", { "hu208" }      }, // Hungarian
	// Armenian (QWERTY/national)
	{ "0000042b", { "hy" }         }, // Armenian Eastern (Legacy)
	{ "0002042b", { "hy" }         }, // Armenian Phonetic
	{ "0003042b", { "hy" }         }, // Armenian Typewriter
	{ "0001042b", { "hy" }         }, // Armenian Western (Legacy)
	// Hebrew (QWERTY/national)
	{ "0000040d", { "il" }         }, // Hebrew
	{ "0002040d", { "il" }         }, // Hebrew (Standard)
	{ "0003040d", { "il" }         }, // Hebrew (Standard, 2018)
	// Icelandic (102-key, QWERTY)
	{ "0000040f", { "is161" }      }, // Icelandic
	// Italian (standard, QWERTY/national)
	{ "00000410", { "it" }         }, // Italian
	// Italian (142, QWERTY/national)
	{ "00010410", { "it142" }      }, // Italian (142)
	// Georgian (QWERTY/national)
	{ "00020437", { "ka" }         }, // Georgian (Ergonomic)
	{ "00000437", { "ka" }         }, // Georgian (Legacy)
	{ "00030437", { "ka" }         }, // Georgian (MES)
	{ "00040437", { "ka" }         }, // Georgian (Old Alphabets)
	{ "00010437", { "ka" }         }, // Georgian (QWERTY)
	// Kazakh (476, QWERTY/national)
	{ "0000043f", { "kk476" }      }, // Kazakh
	// Kyrgyz (QWERTY/national)
	{ "00000440", { "ky" }         }, // Kyrgyz Cyrillic
	// Latin American (QWERTY)
	{ "0000080a", { "la" }         }, // Latin American
	{ "00000474", { "la", 30003 }  }, // Guarani
	// Lithuanian (Baltic, QWERTY/phonetic)
	{ "00010427", { "lt" }         }, // Lithuanian
	// Lithuanian (AZERTY/phonetic)
	{ "00000427", { "lt211" }      }, // Lithuanian IBM
	// Lithuanian (LST 1582, AZERTY/phonetic)
	{ "00020427", { "lt221" }      }, // Lithuanian Standard
	// Latvian (standard, QWERTY/phonetic)
	{ "00010426", { "lv" }         }, // Latvian (QWERTY)
	{ "00020426", { "lv" }         }, // Latvian (Standard)
	// Latvian (QWERTY/UGJRMV/phonetic)
	{ "00000426", { "lv455" }      }, // Latvian
	// Macedonian (QWERTZ/national)
	{ "0000042f", { "mk" }         }, // Macedonian
	{ "0001042f", { "mk" }         }, // Macedonian - Standard
	// Mongolian (QWERTY/national)
	{ "00000850", { "mn" }         }, // Mongolian (Mongolian Script)
	{ "00000450", { "mn" }         }, // Mongolian Cyrillic
	{ "00010850", { "mn" }         }, // Traditional Mongolian (Standard)
	// Maltese (UK layout, QWERTY)
	{ "0001043a", { "mt" }         }, // Maltese 48-Key
	// Maltese (US layout, QWERTY)
	{ "0000043a", { "mt103" }      }, // Maltese 47-Key
	// Nigerian (QWERTY)
	{ "00000468", { "ng" }         }, // Hausa
	{ "0000046a", { "ng" }         }, // Yoruba
	{ "00000470", { "ng" }         }, // Igbo
	// Dutch (QWERTY)
	{ "00000413", { "nl" }         }, // Dutch
	// Norwegian (QWERTY/ASERTT)
	{ "00000414", { "no" }         }, // Norwegian
	{ "0000043b", { "no", 30000 }  }, // Norwegian with Sami
	{ "0001043b", { "no", 30000 }  }, // Sami Extended Norway
	// Polish (programmers, QWERTY/phonetic)
	{ "00000415", { "pl" }         }, // Polish (Programmers)
	// Polish (typewriter, QWERTZ/phonetic)
	{ "00010415", { "pl214" }      }, // Polish (214)
	// Portuguese (QWERTY)
	{ "00000816", { "po" }         }, // Portuguese
	// Romanian (standard, QWERTZ/phonetic)
	{ "00000418", { "ro" }         }, // Romanian (Legacy)
	// Romanian (QWERTY/phonetic)
	{ "00020418", { "ro446" }      }, // Romanian (Programmers)
	{ "00010418", { "ro446" }      }, // Romanian (Standard)
	// Russian (standard, QWERTY/national)
	{ "00000419", { "ru" }         }, // Russian
	{ "00020419", { "ru" }         }, // Russian - Mnemonic
	// Russian (typewriter, QWERTY/national)
	{ "00010419", { "ru443" }      }, // Russian (Typewriter)
	// Russian (extended standard, QWERTY/national)
	{ "0000046d", { "rx", 30013 }  }, // Bashkir
	{ "00000485", { "rx", 30012 }  }, // Sakha
	// Swiss (German, QWERTZ)
	{ "00000807", { "sd" }         }, // Swiss German
	// Swiss (French, QWERTZ)
	{ "0000100c", { "sf" }         }, // Swiss French
	{ "0000046e", { "sf" }         }, // Luxembourgish
	// Slovenian (QWERTZ)
	{ "00000424", { "si" }         }, // Slovenian
	{ "0000041a", { "si" }         }, // Standard
	// Slovak (QWERTZ)
	{ "0000041b", { "sk" }         }, // Slovak
	// Albanian (deadkeys, QWERTZ)
	{ "0000041c", { "sq448" }      }, // Albanian
	// Swedish (QWERTY/ASERTT)
	{ "0000041d", { "sv" }         }, // Swedish
	{ "0000083b", { "sv", 30000 }  }, // Swedish with Sami
	{ "0002083b", { "sv", 30000 }  }, // Sami Extended Finland-Sweden
	// Tajik (QWERTY/national)
	{ "00000428", { "tj" }         }, // Tajik
	// Turkmen (QWERTY/phonetic)
	{ "00000442", { "tm" }         }, // Turkmen
	// Turkish (QWERTY)
	{ "0000041f", { "tr" }         }, // Turkish Q
	// Turkish (non-standard)
	{ "0001041f", { "tr440" }      }, // Turkish F
	// Tatar (standard, QWERTY/national)
	{ "00010444", { "tt" }         }, // Tatar
	{ "00000444", { "tt" }         }, // Tatar (Legacy)
	// Ukrainian (102-key, 2001, QWERTY/national)
	{ "00000422", { "ur2001" }     }, // Ukrainian
	{ "00020422", { "ur2001" }     }, // Ukrainian (Enhanced)
	// Uzbek (QWERTY/national)
	{ "00000843", { "uz" }         }, // Uzbek Cyrillic
	// Vietnamese (QWERTY)
	{ "0000042a", { "vi" }         }, // Vietnamese
	// Serbian (deadkey, QWERTZ/national)
	{ "00000c1a", { "yc" }         }, // Serbian (Cyrillic)
	{ "0000081a", { "yc" }         }, // Serbian (Latin)

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the QWERTY layout is typically used
	{ "0000044d", { "us", {},  Fuzzy } }, // Assamese - INSCRIPT
	{ "00000445", { "us", {},  Fuzzy } }, // Bangla
	{ "00020445", { "us", {},  Fuzzy } }, // Bangla - INSCRIPT
	{ "00010445", { "us", {},  Fuzzy } }, // Bangla - INSCRIPT (Legacy)
	{ "000b0c00", { "us", {},  Fuzzy } }, // Buginese
	{ "00000804", { "us", {},  Fuzzy } }, // Chinese (Simplified) - US
	{ "00001004", { "us", {},  Fuzzy } }, // Chinese (Simplified, Singapore) - US
	{ "00000404", { "us", {},  Fuzzy } }, // Chinese (Traditional) - US
	{ "00000c04", { "us", {},  Fuzzy } }, // Chinese (Traditional, Hong Kong S.A.R.) - US
	{ "00001404", { "us", {},  Fuzzy } }, // Chinese (Traditional, Macao S.A.R.) - US
	{ "00000439", { "us", {},  Fuzzy } }, // Devanagari - INSCRIPT
	{ "00000465", { "us", {},  Fuzzy } }, // Divehi Phonetic
	{ "00010465", { "us", {},  Fuzzy } }, // Divehi Typewriter
	{ "00000c51", { "us", {},  Fuzzy } }, // Dzongkha
	{ "00120c00", { "us", {},  Fuzzy } }, // Futhark
	{ "00000447", { "us", {},  Fuzzy } }, // Gujarati
	{ "00010439", { "us", {},  Fuzzy } }, // Hindi Traditional
	{ "00000411", { "us", {},  Fuzzy } }, // Japanese
	{ "00110c00", { "us", {},  Fuzzy } }, // Javanese
	{ "0000044b", { "us", {},  Fuzzy } }, // Kannada
	{ "00000453", { "us", {},  Fuzzy } }, // Khmer
	{ "00010453", { "us", {},  Fuzzy } }, // Khmer (NIDA)
	{ "00000412", { "us", {},  Fuzzy } }, // Korean
	{ "00000454", { "us", {},  Fuzzy } }, // Lao
	{ "00070c00", { "us", {},  Fuzzy } }, // Lisu (Basic)
	{ "00080c00", { "us", {},  Fuzzy } }, // Lisu (Standard)
	{ "0000044c", { "us", {},  Fuzzy } }, // Malayalam
	{ "0000044e", { "us", {},  Fuzzy } }, // Marathi
	{ "00010c00", { "us", {},  Fuzzy } }, // Myanmar (Phonetic order)
	{ "00130c00", { "us", {},  Fuzzy } }, // Myanmar (Visual order)
	{ "00000461", { "us", {},  Fuzzy } }, // Nepali
	{ "00020c00", { "us", {},  Fuzzy } }, // New Tai Lue
	{ "00000448", { "us", {},  Fuzzy } }, // Odia
	{ "00040c00", { "uk", {},  Fuzzy } }, // Ogham
	{ "000d0c00", { "us", {},  Fuzzy } }, // Ol Chiki
	{ "000f0c00", { "it", {},  Fuzzy } }, // Old Italic
	{ "00150c00", { "us", {},  Fuzzy } }, // Osage
	{ "000e0c00", { "us", {},  Fuzzy } }, // Osmanya
	{ "000a0c00", { "us", {},  Fuzzy } }, // Phags-pa
	{ "00000446", { "us", {},  Fuzzy } }, // Punjabi
	{ "0000045b", { "us", {},  Fuzzy } }, // Sinhala
	{ "0001045b", { "us", {},  Fuzzy } }, // Sinhala - Wij 9
	{ "00100c00", { "us", {},  Fuzzy } }, // Sora
	{ "0000045a", { "us", {},  Fuzzy } }, // Syriac
	{ "0001045a", { "us", {},  Fuzzy } }, // Syriac Phonetic
	{ "00030c00", { "us", {},  Fuzzy } }, // Tai Le
	{ "00000449", { "us", {},  Fuzzy } }, // Tamil
	{ "00020449", { "us", {},  Fuzzy } }, // Tamil 99
	{ "00030449", { "us", {},  Fuzzy } }, // Tamil Anjal
	{ "0000044a", { "us", {},  Fuzzy } }, // Telugu
	{ "0000041e", { "us", {},  Fuzzy } }, // Thai Kedmanee
	{ "0002041e", { "us", {},  Fuzzy } }, // Thai Kedmanee (non-ShiftLock)
	{ "0001041e", { "us", {},  Fuzzy } }, // Thai Pattachote
	{ "0003041e", { "us", {},  Fuzzy } }, // Thai Pattachote (non-ShiftLock)
	{ "00000451", { "us", {},  Fuzzy } }, // Tibetan (PRC)
	{ "00010451", { "us", {},  Fuzzy } }, // Tibetan (PRC) - Updated
	{ "0000105f", { "us", {},  Fuzzy } }, // Tifinagh (Basic)
	{ "0001105f", { "us", {},  Fuzzy } }, // Tifinagh (Extended)
	{ "00000420", { "us", {},  Fuzzy } }, // Urdu

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the QWERTZ layout is typically used
	{ "000c0c00", { "de", {},  Fuzzy } }, // Gothic

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the AZERTY layout is typically used
	{ "00090c00", { "fr", 437, Fuzzy } }, // N’Ko
};
// clang-format on

static std::string to_string(const wchar_t* input, const size_t input_length)
{
	if (input_length == 0) {
		return {};
	}

	const auto buffer_size = WideCharToMultiByte(DefaultCodePage, 0,
                                                     input, check_cast<int>(input_length),
                                                     nullptr, 0, nullptr, nullptr);
	if (buffer_size == 0) {
		return {};
	}

	std::vector<char> buffer(buffer_size + 1);
	WideCharToMultiByte(DefaultCodePage,  0, input, check_cast<int>(input_length),
	                    buffer.data(), buffer_size, nullptr, nullptr);
	buffer[buffer_size] = '\0';

	return std::string(buffer.begin(), std::find(buffer.begin(), buffer.end(), '\0'));
}

static std::map<std::string, std::string> read_layouts_registry(const std::string& subkey)
{
	const DWORD MaxLength = 16;
	const std::string Key = TEXT("Keyboard Layout\\");

	// Open registry key
	HKEY handle = nullptr;
	auto result = RegOpenKeyExA(
	        HKEY_CURRENT_USER, (Key + subkey).c_str(), 0, KEY_READ, &handle);
	if (result != ERROR_SUCCESS) {
		return {};
	}

	std::map<std::string, std::string> layouts = {};

	// Read the key content
	DWORD index = 0;
	while (true) {
		DWORD length1 = MaxLength;
		DWORD length2 = MaxLength;

		std::vector<char> buffer1(MaxLength);
		std::vector<unsigned char> buffer2(MaxLength);

		DWORD type = 0;

		result = RegEnumValueA(handle, index++, buffer1.data(), &length1,
                                       nullptr, &type, buffer2.data(), &length2);

		if (result == ERROR_MORE_DATA) {
			continue;
		}
		if (result != ERROR_SUCCESS) {
			break;
		}
		if (type != REG_SZ) {
			continue;
		}

		auto value1 = std::string(buffer1.begin(),
		                          std::find(buffer1.begin(), buffer1.end(), 0));
		auto value2 = std::string(buffer2.begin(),
		                          std::find(buffer2.begin(), buffer2.end(), 0));

		if (value1.empty() || value2.empty()) {
			continue;
		}

		layouts[value1] = value2;
	}

	RegCloseKey(handle);

	return layouts;
}

static HostKeyboardLayouts get_host_keyboard_layouts()
{
	HostKeyboardLayouts result = {};
	auto& result_list = result.keyboard_layout_list;

	// First look for the user-preferred layouts
	const auto substitutes = read_layouts_registry(TEXT("Substitutes"));
	for (const auto& entry : substitutes) {
		// Get information for log
		if (!result.log_info.empty()) {
			result.log_info += ";";
		}
		result.log_info += std::string(entry.first);
		result.log_info += "->";
		result.log_info += entry.second;

		// Check if we know a matching keyboard layout
		auto key = entry.second;
		lowcase(key);
		if (WinToDosKeyboard.contains(key)) {
			result_list.push_back(WinToDosKeyboard.at(key));
		}
	}

	if (!result_list.empty()) {
		return result;
	}

	// Then check all the available layouts in the system
	const auto preload = read_layouts_registry(TEXT("Preload"));
	for (const auto& entry : preload) {
		// Get information for log
		if (!result.log_info.empty()) {
			result.log_info += ";";
		}
		result.log_info += entry.second;

		auto key = entry.second;
                lowcase(key);
                if (WinToDosKeyboard.contains(key)) {
			result_list.push_back(WinToDosKeyboard.at(key));
		}
	}

	return result;
}

static HostLocaleElement get_dos_country()
{
	HostLocaleElement result = {};

	wchar_t buffer[LOCALE_NAME_MAX_LENGTH];
	if (!GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH)) {
		LOG_WARNING("LOCALE: Could not get default locale name, error code %lu",
		            GetLastError());
		return {};
	}
	const auto locale_name = to_string(&buffer[0], LOCALE_NAME_MAX_LENGTH);

	result.log_info = locale_name;

	const auto tokens = split(locale_name, "-");
	if (tokens.size() < 2) {
		return {};
	}

	result.country_code = iso_to_dos_country(tokens[0], tokens[1]);
	return result;
}

static std::vector<std::string> get_preferred_languages()
{
	std::vector<std::string> result = {};

	// At least on Windows 11, 'GetUserPreferredUILanguages' only returns
	// the first language - let's read the registry instead

	const LPCTSTR KeyPath = TEXT("Control Panel\\International\\User Profile\\");
	const LPCTSTR KeyName = TEXT("Languages");

	DWORD buffer_size = 0;

	// Get the buffer size
	if (RegGetValueA(HKEY_CURRENT_USER,
	                 KeyPath,
	                 KeyName,
	                 RRF_RT_REG_MULTI_SZ,
	                 nullptr,
	                 nullptr,
	                 &buffer_size) != ERROR_SUCCESS ||
	    buffer_size == 0) {
		return {};
	}

	// Get the actual data
	std::vector<char> buffer(buffer_size + 1);
	buffer.resize(buffer_size);
	if (RegGetValueA(HKEY_CURRENT_USER,
	                 KeyPath,
	                 KeyName,
	                 RRF_RT_REG_MULTI_SZ,
	                 nullptr,
	                 buffer.data(),
	                 &buffer_size) != ERROR_SUCCESS) {
		return {};
	}
	buffer.back() = '\0';

	// Extract the values
	auto iter = buffer.begin();
	while (iter < buffer.end()) {
		const auto element = std::string(iter, std::find(iter, buffer.end(), '\0'));
		if (element.empty()) {
			break;
		}

		result.push_back(element);
		iter += element.size() + 1;
	}

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
			return iso_to_language_file(tokens.at(0), "");
		}
		return iso_to_language_file(tokens.at(0), tokens.at(1));
	};

	// Get the list of preferred languages
	for (const auto& entry : get_preferred_languages()) {
		if (!result.log_info.empty()) {
			result.log_info += ", ";
		}
		result.log_info += entry;

		result.language_files.push_back(get_language_file(entry));
	}

	// Get the default GUI language
	wchar_t buffer[LOCALE_NAME_MAX_LENGTH];
	const auto ui_language = GetUserDefaultUILanguage();
	if (LCIDToLocaleName(ui_language, buffer, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
		LOG_WARNING("LOCALE: Could not get locale name for language 0x%04x, error code %lu",
		            ui_language,
		            GetLastError());
		return {};
	}

	const auto language_territory = to_string(&buffer[0], LOCALE_NAME_MAX_LENGTH);

	if (!result.log_info.empty()) {
		result.log_info += "; ";
	}
	result.log_info += "GUI: ";
	result.log_info += language_territory;

	result.language_file_gui = get_language_file(language_territory);
	return result;
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

// ***************************************************************************
// Overridden generic locale fetch routines
// ***************************************************************************

void StdLibLocale::DetectCurrencyFormat([[maybe_unused]] const std::locale& locale)
{
	// Not implemented for Windows
}

#endif
