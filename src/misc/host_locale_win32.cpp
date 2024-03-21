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

#if defined(WIN32)

#include "host_locale.h"

#include "checks.h"

CHECK_NARROWING();


// XXX fill-in, describe, use it
// XXX see https://kbdlayout.info
static const std::map<std::string, KeyboardLayoutMaybeCodepage> WinToDosKeyboard = {
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
	{ "0000046f", { "dk", 30004}   }, // Greenlandic
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
	{ "0000044d", { "us" }         }, // Assamese - INSCRIPT
	{ "00000445", { "us" }         }, // Bangla
	{ "00020445", { "us" }         }, // Bangla - INSCRIPT
	{ "00010445", { "us" }         }, // Bangla - INSCRIPT (Legacy)
	{ "000b0c00", { "us" }         }, // Buginese
	{ "00000804", { "us" }         }, // Chinese (Simplified) - US
	{ "00001004", { "us" }         }, // Chinese (Simplified, Singapore) - US
	{ "00000404", { "us" }         }, // Chinese (Traditional) - US
	{ "00000c04", { "us" }         }, // Chinese (Traditional, Hong Kong S.A.R.) - US
	{ "00001404", { "us" }         }, // Chinese (Traditional, Macao S.A.R.) - US
	{ "00000439", { "us" }         }, // Devanagari - INSCRIPT
	{ "00000465", { "us" }         }, // Divehi Phonetic
	{ "00010465", { "us" }         }, // Divehi Typewriter
	{ "00000c51", { "us" }         }, // Dzongkha
	{ "00120c00", { "us" }         }, // Futhark
	{ "00000447", { "us" }         }, // Gujarati
	{ "00010439", { "us" }         }, // Hindi Traditional
	{ "00000411", { "us" }         }, // Japanese
	{ "00110c00", { "us" }         }, // Javanese
	{ "0000044b", { "us" }         }, // Kannada
	{ "00000453", { "us" }         }, // Khmer
	{ "00010453", { "us" }         }, // Khmer (NIDA)
	{ "00000412", { "us" }         }, // Korean
	{ "00000454", { "us" }         }, // Lao
	{ "00070c00", { "us" }         }, // Lisu (Basic)
	{ "00080c00", { "us" }         }, // Lisu (Standard)
	{ "0000044c", { "us" }         }, // Malayalam
	{ "0000044e", { "us" }         }, // Marathi
	{ "00010c00", { "us" }         }, // Myanmar (Phonetic order)
	{ "00130c00", { "us" }         }, // Myanmar (Visual order)
	{ "00000461", { "us" }         }, // Nepali
	{ "00020c00", { "us" }         }, // New Tai Lue
	{ "00000448", { "us" }         }, // Odia
	{ "00040c00", { "uk" }         }, // Ogham
	{ "000d0c00", { "us" }         }, // Ol Chiki
	{ "000f0c00", { "it" }         }, // Old Italic
	{ "00150c00", { "us" }         }, // Osage
	{ "000e0c00", { "us" }         }, // Osmanya
	{ "000a0c00", { "us" }         }, // Phags-pa
	{ "00000446", { "us" }         }, // Punjabi
	{ "0000045b", { "us" }         }, // Sinhala
	{ "0001045b", { "us" }         }, // Sinhala - Wij 9
	{ "00100c00", { "us" }         }, // Sora
	{ "0000045a", { "us" }         }, // Syriac
	{ "0001045a", { "us" }         }, // Syriac Phonetic
	{ "00030c00", { "us" }         }, // Tai Le
	{ "00000449", { "us" }         }, // Tamil
	{ "00020449", { "us" }         }, // Tamil 99
	{ "00030449", { "us" }         }, // Tamil Anjal
	{ "0000044a", { "us" }         }, // Telugu
	{ "0000041e", { "us" }         }, // Thai Kedmanee
	{ "0002041e", { "us" }         }, // Thai Kedmanee (non-ShiftLock)
	{ "0001041e", { "us" }         }, // Thai Pattachote
	{ "0003041e", { "us" }         }, // Thai Pattachote (non-ShiftLock)
	{ "00000451", { "us" }         }, // Tibetan (PRC)
	{ "00010451", { "us" }         }, // Tibetan (PRC) - Updated
	{ "0000105f", { "us" }         }, // Tifinagh (Basic)
	{ "0001105f", { "us" }         }, // Tifinagh (Extended)
	{ "00000420", { "us" }         }, // Urdu

	// In some cases we do not have a matching QWERTY layout; if so, use
	// the US International keyboard with the best available code page
	{ "0001041b", { "ux", 437 }    }, // Slovak (QWERTY)

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the QWERTZ layout is typically used
	{ "000c0c00", { "de" }         }, // Gothic

	// For some keyboard families we don't have code pages, but in the
	// corresponding states the AZERTY layout is typically used
	{ "00090c00", { "fr", 437 }    }, // N’Ko

};

/* XXX use [MS-LCID]: Windows Language Code Identifier (LCID) Reference

*/


HostLocale DetectHostLocale()
{
	HostLocale locale = {};

	// XXX detect the following:
	// XXX locale.keyboard_layout  - registry, GetKeyboardLayout
	// XXX locale.dos_country      - GetUserDefaultLocaleName
	// XXX locale.numeric
	// XXX locale.time_date
	// XXX locale.currency

	// XXX provide log_info

	return locale;
}

HostLanguage DetectHostLanguage()
{
	HostLanguage locale = {};

	// XXX locale.language  - GetUserDefaultUILanguage

	// XXX provide log_info

	return locale
}




std::string DOS_GetLayoutFromHost() // XXX this one is outdated, rework it
{
	// Use Windows-specific calls to extract the keyboard layout

	WORD current_kb_layout = LOWORD(GetKeyboardLayout(0));
	WORD current_kb_sub_id = 0;
	char layout_id_string[KL_NAMELENGTH];

	auto parse_hex_string = [](const char* s) {
		uint32_t value = 0;
		sscanf(s, "%x", &value);
		return static_cast<int>(value);
	};

	if (GetKeyboardLayoutName(layout_id_string) &&
	    (safe_strlen(layout_id_string) == 8)) {
		const int current_kb_layout_by_name = parse_hex_string(
		        (char*)&layout_id_string[4]);
		layout_id_string[4] = 0;

		const int sub_id = parse_hex_string((char*)&layout_id_string[0]);

		if ((current_kb_layout_by_name > 0) &&
		    (current_kb_layout_by_name <= UINT16_MAX)) {
			// use layout _id extracted from the layout string
			current_kb_layout = static_cast<WORD>(current_kb_layout_by_name);
		}
		if ((sub_id >= 0) && (sub_id < 100)) {
			// use sublanguage ID extracted from the layout
			// string
			current_kb_sub_id = static_cast<WORD>(sub_id);
		}
	}

	// Try to match emulated keyboard layout with host-keyboardlayout
	switch (current_kb_layout) {
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
	case 1038: return current_kb_sub_id ? "hu" : "hu208";
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

	return "";
}

/* XXX
#if defined(WIN32)
	auto get_lang_from_windows = []() -> std::string {
		std::string lang = {};
		wchar_t w_buf[LOCALE_NAME_MAX_LENGTH];
		if (!GetUserDefaultLocaleName(w_buf, LOCALE_NAME_MAX_LENGTH))
			return lang;

		// Convert the wide-character string into a normal buffer
		char buf[LOCALE_NAME_MAX_LENGTH];
		wcstombs(buf, w_buf, LOCALE_NAME_MAX_LENGTH);

		lang = buf;
		clear_language_if_default(lang);
		return lang;
	};
#endif
*/

#endif
