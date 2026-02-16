// SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_locale.h"

#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

using namespace LocaleData;

// NOTE: Locale settings below were selected based on our knowledge and various
// public sources. Since we are only a small group of volunteers and not
// linguists, and there are about 200 recognized countries in the world,
// mistakes and unfortunate choices could happen.
// Sorry for that, we do not mean to offend or discriminate anyone!

// ***************************************************************************
// Data structure methods
// ***************************************************************************

std::string CountryInfoEntry::GetMsgName() const
{
	const std::string base_str = "COUNTRY_NAME_";
	return base_str + country_code;
}

std::string ScriptInfoEntry::GetMsgName() const
{
	const std::string base_str = "SCRIPT_NAME_";
	return base_str + script_code;
}

std::string CodePageInfoEntry::GetMsgName(const uint16_t code_page)
{
	const std::string base_str = "CODE_PAGE_DESCRIPTION_";
	return base_str + std::to_string(code_page);
}

std::string KeyboardLayoutInfoEntry::GetMsgName() const
{
	assert(!layout_codes.empty());
	const std::string base_str = "KEYBOARD_LAYOUT_NAME_";
	auto layout_code = layout_codes[0];
	upcase(layout_code);
	return base_str + layout_code;
}

// ***************************************************************************
// Code page (screen font) information
// ***************************************************************************

// Reference:
// - https://gitlab.com/FreeDOS/base/cpidos/-/blob/master/DOC/CPIDOS/CODEPAGE.TXT

// clang-format off
const std::map<std::string, std::vector<uint16_t>> LocaleData::BundledCpiContent = {
	// FreeDOS code pages - standard package
	{ "EGA.CPI",     { 437, 850, 852, 853, 857, 858 } },
	{ "EGA2.CPI",    { 775, 859, 1116, 1117, 1118, 1119 } },
	{ "EGA3.CPI",    { 771, 772, 808, 855, 866, 872 } },
	{ "EGA4.CPI",    { 848, 849, 1125, 1131, 3012, 30010 } },
	{ "EGA5.CPI",    { 113, 737, 851, 852, 858, 869 } },
	{ "EGA6.CPI",    { 899, 30008, 58210, 59829, 60258, 60853 } },
	{ "EGA7.CPI",    { 30011, 30013, 30014, 30017, 30018, 30019 } },
	{ "EGA8.CPI",    { 770, 773, 774, 775, 777, 778 } },
	{ "EGA9.CPI",    { 858, 860, 861, 863, 865, 867 } },
	{ "EGA10.CPI",   { 667, 668, 790, 852, 991, 3845 } },
	{ "EGA11.CPI",   { 858, 30000, 30001, 30004, 30007, 30009 } },
	{ "EGA12.CPI",   { 852, 858, 30003, 30029, 30030, 58335 } },
	{ "EGA13.CPI",   { 852, 895, 30002, 58152, 59234, 62306 } },
	{ "EGA14.CPI",   { 30006, 30012, 30015, 30016, 30020, 30021 } },
	{ "EGA15.CPI",   { 30023, 30024, 30025, 30026, 30027, 30028 } },
	{ "EGA16.CPI",   { 858, 3021, 30005, 30022, 30031, 30032 } },
	{ "EGA17.CPI",   { 862, 864, 30034, 30033, 30039, 30040 } },
	{ "EGA18.CPI",   { 856, 3846, 3848 } },
	// FreeDOS code pages - ISO pack
	{ "EGAISO.CPI",  { 819, 912, 913, 923, 58163, 61235 } },
	{ "EGA2ISO.CPI", { 819, 901, 914, 921, 58258, 61235 } },
	{ "EGA3ISO.CPI", { 819, 902, 919, 922, 61235, 63283 } },
	{ "EGA4ISO.CPI", { 819, 59187, 60211, 61235, 65500, 65501 } },
	{ "EGA5ISO.CPI", { 813, 819, 920, 61235, 65504 } },
	{ "EGA6ISO.CPI", { 915, 1124, 58259, 59283, 65502, 65503 } },
	// FreeDOS code pages - KOI Cyrillic pack
	{ "EGAKOI.CPI",  { 878, 58222, 59246, 60270, 61294, 62318 } },
	{ "EGA2KOI.CPI", { 878, 63342 } },
	// FreeDOS code pages - macOS pack
	{ "EGAMAC.CPI",  { 1275, 1282, 1284, 1285, 58619, 58630 } },
	{ "EGA2MAC.CPI", { 1275, 1280, 1281, 1283, 1286, 58627 } },
	// FreeDOS code pages - Windows pack
	{ "EGAWIN.CPI",  { 1250, 1252, 1257, 1270, 1361, 58601 } },
	{ "EGA2WIN.CPI", { 1252, 1253, 1254, 58596, 58598, 65506 } },
	{ "EGA3WIN.CPI", { 1251, 58595, 59619, 60643, 61667, 62691 } },
	{ "EGA4WIN.CPI", { 1252, 59620 } },
};
// clang-format on

// List of code pages from bundled CPI files needing the given patch

// clang-format off
const std::set<uint16_t> LocaleData::NeedsPatchDottedI  = {
	60258
};
const std::set<uint16_t> LocaleData::NeedsPatchLowCodes = {
	1275, 1281, 1282, 1283, 1284, 1285, 1286,
	58619, 58627, 58630, 60643, 61667, 62691
};
// clang-format on

// clang-format off
const std::map<Script, ScriptInfoEntry> LocaleData::ScriptInfo = {
        { Script::Latin,    { "LATN", "Latin"    } },
        { Script::Arabic,   { "ARAB", "Arabic"   } },
        { Script::Armenian, { "ARMN", "Armenian" } },
        { Script::Cherokee, { "CHER", "Cherokee" } },
        { Script::Cyrillic, { "CYRL", "Cyrillic" } },
        { Script::Georgian, { "GEOR", "Georgian" } },
        { Script::Greek,    { "GREK", "Greek"    } },
        { Script::Hebrew,   { "HEBR", "Hebrew"   } },
};
// clang-format on

// clang-format off
const std::vector<CodePagePackInfo> LocaleData::CodePageInfo = { {
	// ROM code page
	{ 437,   { "United States",                                       Script::Latin    } },
}, {    // Most common code pages
	{ 113,   { "Yugoslavian",                                         Script::Latin    } },
	{ 667,   { "Polish, Mazovia encoding",                            Script::Latin    } },
	{ 668,   { "Polish, 852-compatible",                              Script::Latin    } },
	{ 737,   { "Greek-2",                                             Script::Greek    } },
	{ 770,   { "Baltic, RST 1095:89 encoding",                        Script::Latin    } },
	{ 771,   { "Lithuanian and Russian, KBL encoding",                Script::Cyrillic } },
	{ 773,   { "Baltic, KBL encoding",                                Script::Latin    } },
	{ 775,   { "Latin-7 (Baltic)",                                    Script::Latin    } },
	{ 777,   { "Lithuanian, accented, KBL encoding",                  Script::Latin    } },
	{ 778,   { "Lithuanian, accented, LST 1590-2 encoding",           Script::Latin    } },
	{ 808,   { "Russian, with EUR symbol",                            Script::Cyrillic } },
	{ 848,   { "Ukrainian, with EUR symbol",                          Script::Cyrillic } },
	{ 849,   { "Belarusian, with EUR symbol",                         Script::Cyrillic } },
	{ 850,   { "Latin-1 (Western European)",                          Script::Latin    } },
	{ 851,   { "Greek, old encoding",                                 Script::Greek    } },
	{ 852,   { "Latin-2 (Central European), with EUR symbol",         Script::Latin    } },
	{ 853,   { "Latin-3 (Turkish, Maltese, Esperanto)",               Script::Latin    } },
	{ 855,   { "South Slavic",                                        Script::Cyrillic } },
	{ 856,   { "Hebrew-2, with EUR symbol",                           Script::Hebrew   } },
	{ 857,   { "Latin-5 (Turkish), with EUR symbol",                  Script::Latin    } },
	{ 858,   { "Latin-1 (Western European), with EUR symbol",         Script::Latin    } },
	{ 859,   { "Latin-9 (Western European), with EUR symbol",         Script::Latin    } },
	{ 860,   { "Portuguese",                                          Script::Latin    } },
	{ 861,   { "Icelandic",                                           Script::Latin    } },
	{ 862,   { "Hebrew-2",                                            Script::Hebrew   } },
	{ 863,   { "Canadian French",                                     Script::Latin    } },
	{ 864,   { "Arabic",                                              Script::Arabic   } },
	{ 865,   { "Nordic",                                              Script::Latin    } },
	{ 866,   { "Russian",                                             Script::Cyrillic } },
	{ 867,   { "Czech and Slovak, Kamenický encoding",                Script::Latin    } },
	{ 869,   { "Greek, with EUR symbol",                              Script::Greek    } },
	{ 872,   { "South Slavic, with EUR symbol",                       Script::Cyrillic } },
	{ 899,   { "Armenian, ArmSCII-8A encoding",                       Script::Armenian } },
	{ 991,   { "Polish, Mazovia encoding, with PLN symbol",           Script::Latin    } },
	{ 1116,  { "Estonian",                                            Script::Latin    } },
	{ 1117,  { "Latvian",                                             Script::Latin    } },
	{ 1118,  { "Lithuanian, LST 1283 encoding",                       Script::Latin    } },
	{ 1119,  { "Lithuanian and Russian, LST 1284 encoding",           Script::Cyrillic } },
	{ 1125,  { "Ukrainian",                                           Script::Cyrillic } },
	{ 1131,  { "Belarusian",                                          Script::Cyrillic } },
	{ 3012,  { "Latvian and Russian, RusLat encoding",                Script::Cyrillic } },
	{ 3021,  { "Bulgarian, MIK encoding",                             Script::Cyrillic } },
	{ 3845,  { "Hungarian, CWI-2 encoding",                           Script::Latin    } },
	{ 3846,  { "Turkish",                                             Script::Latin    } },
	{ 3848,  { "Brazilian, ABICOMP encoding",                         Script::Latin    } },
	{ 30000, { "Saami, Kalo, Finnic",                                 Script::Latin    } },
	{ 30001, { "Celtic and Scots, with EUR symbol",                   Script::Latin    } },
	{ 30002, { "Tajik, with EUR symbol",                              Script::Cyrillic } },
	{ 30003, { "Latin American, with EUR symbol",                     Script::Latin    } },
	{ 30004, { "Greenlandic and North Germanic, with EUR symbol",     Script::Latin    } },
	{ 30005, { "Nigerian, with EUR symbol",                           Script::Latin    } },
	{ 30006, { "Vietnamese, VISCII encoding",                         Script::Latin    } },
	{ 30007, { "Latin and Romansh, with EUR symbol",                  Script::Latin    } },
	{ 30008, { "Abkhaz and Ossetian, with EUR symbol",                Script::Cyrillic } },
	{ 30009, { "Romani and Turkic, with EUR symbol",                  Script::Latin    } },
	{ 30010, { "Gagauz and Moldovan, with EUR symbol",                Script::Cyrillic } },
	{ 30011, { "Russian Southern, with EUR symbol",                   Script::Cyrillic } },
	{ 30012, { "Siberian, with EUR symbol",                           Script::Cyrillic } },
	{ 30013, { "Turkic, with EUR symbol",                             Script::Cyrillic } },
	{ 30014, { "Finno-ugric (Mari, Udmurt), with EUR symbol",         Script::Cyrillic } },
	{ 30015, { "Khanty, with EUR symbol",                             Script::Cyrillic } },
	{ 30016, { "Mansi, with EUR symbol",                              Script::Cyrillic } },
	{ 30017, { "Russian Northwestern, with EUR symbol",               Script::Cyrillic } },
	{ 30018, { "Tatar Latin and Russian, with EUR symbol",            Script::Cyrillic } },
	{ 30019, { "Chechen Latin and Russian, with EUR symbol",          Script::Cyrillic } },
	{ 30020, { "Low Saxon and Frisian, with EUR symbol",              Script::Latin    } },
	{ 30021, { "Oceanic, with EUR symbol",                            Script::Latin    } },
	{ 30022, { "Canadian First Nations, with EUR symbol",             Script::Latin    } },
	{ 30023, { "Southern African, with EUR symbol",                   Script::Latin    } },
	{ 30024, { "Northern and Eastern African, with EUR symbol",       Script::Latin    } },
	{ 30025, { "Western African, with EUR symbol",                    Script::Latin    } },
	{ 30026, { "Central African, with EUR symbol",                    Script::Latin    } },
	{ 30027, { "Beninese, with EUR symbol",                           Script::Latin    } },
	{ 30028, { "Nigerien, with EUR symbol",                           Script::Latin    } },
	{ 30029, { "Mexican, with EUR symbol",                            Script::Latin    } },
	{ 30030, { "Mexican-2, with EUR symbol",                          Script::Latin    } },
	{ 30031, { "Latin-4 (Northern European), with EUR symbol",        Script::Latin    } },
	{ 30032, { "Latin-6 (Nordic), with EUR symbol",                   Script::Latin    } },
	{ 30033, { "Crimean Tatar, with UAH symbol",                      Script::Latin    } },
	{ 30034, { "Cherokee",                                            Script::Cherokee } },
	{ 30039, { "Ukrainian, with UAH symbol",                          Script::Cyrillic } },
	{ 30040, { "Russian, with UAH symbol",                            Script::Cyrillic } },
	{ 58152, { "Kazakh, with EUR symbol",                             Script::Cyrillic } },
	{ 58210, { "Azeri Cyrillic and Russian",                          Script::Cyrillic } },
	{ 59234, { "Tatar",                                               Script::Cyrillic } },
	{ 58335, { "Kashubian, Mazovia-based, with PLN symbol",           Script::Latin    } },
	{ 58601, { "Lithuanian, accented, LST 1590-4, with EUR symbol",   Script::Latin    } },
	{ 59829, { "Georgian",                                            Script::Georgian } },
	{ 60258, { "Azeri Latin and Russian",                             Script::Cyrillic } },
	{ 60853, { "Georgian with capital letters",                       Script::Georgian } },
	{ 62306, { "Uzbek",                                               Script::Cyrillic } },
	{ 65506, { "Armenian, ArmSCII-8 encoding",                        Script::Armenian } },
}, {    // ISO series (8859)
	{ 813,   { "ISO-8859-7 (Greek), with EUR symbol",                 Script::Greek    } },
	{ 819,   { "ISO-8859-1 (Western European)",                       Script::Latin    } },
	{ 901,   { "ISO-8859-13 (Baltic), with EUR symbol",               Script::Latin    } },
	{ 912,   { "ISO-8859-2 (Central European)",                       Script::Latin    } },
	{ 913,   { "ISO-8859-3 (South European)",                         Script::Latin    } },
	{ 914,   { "ISO-8859-4 (North European)",                         Script::Latin    } },
	{ 915,   { "ISO-8859-5 (Cyrillic)",                               Script::Cyrillic } },
	{ 919,   { "ISO-8859-10 (Nordic)",                                Script::Latin    } },
	{ 920,   { "ISO-8859-9 (Turkish)",                                Script::Latin    } },
	{ 921,   { "ISO-8859-13 (Baltic)",                                Script::Latin    } },
	{ 923,   { "ISO-8859-15 (Western European), with EUR symbol",     Script::Latin    } },
	{ 1124,  { "ISO 8859-5 (modified for Ukrainian)",                 Script::Cyrillic } },
	{ 58163, { "ISO-8859-14 (Celtic)",                                Script::Latin    } },
	{ 58258, { "ISO-8859-4 (North European), with EUR symbol",        Script::Latin    } },
	{ 61235, { "ISO-8859-1 (Western European), with EUR symbol",      Script::Latin    } },
	{ 63283, { "ISO-8859-1 (modified for Lithuanian)",                Script::Latin    } },
	{ 65500, { "ISO-8859-16 (South-Eastern European)",                Script::Latin    } },
}, {    // ISO series (remaining)
	{ 902,   { "ISO-8 (Estonian), with EUR symbol",                   Script::Latin    } },
	{ 922,   { "ISO-8 (Estonian)",                                    Script::Latin    } },
	{ 58259, { "ISO-IR-201 (Volgaic)",                                Script::Cyrillic } },
	{ 59187, { "ISO-IR-197 (Saami)",                                  Script::Latin    } },
	{ 59283, { "ISO-IR-200 (Uralic)",                                 Script::Cyrillic } },
	{ 60211, { "ISO-IR-209 (Saami and Finnish Romani)",               Script::Latin    } },
	{ 65501, { "ISO-IR-123 (Canadian and Spanish)",                   Script::Latin    } },
	{ 65502, { "ISO-IR-143 (Technical Set)",                          Script::Latin    } },
	{ 65503, { "ISO-IR-181 (Electrotechnical Set)",                   Script::Latin    } },
	{ 65504, { "ISO-IR-39 (African)",                                 Script::Latin    } },
}, {    // KOI series
	{ 878,   { "KOI8-R (Russian)",                                    Script::Cyrillic } },
	{ 58222, { "KOI8-U (Russian and Ukrainian)",                      Script::Cyrillic } },
	{ 59246, { "KOI8-RU (Russian, Belarusian, Ukrainian)",            Script::Cyrillic } },
	{ 60270, { "KOI8-F (full Slavic)",                                Script::Cyrillic } },
	{ 61294, { "KOI8-CA (full Slavic and non-Slavic)",                Script::Cyrillic } },
	{ 62318, { "KOI8-T (Russian and Tajik)",                          Script::Cyrillic } },
	{ 63342, { "KOI8-C (Russian and Old Russian), with EUR symbol",   Script::Cyrillic } },
}, {    // Apple series
	{ 1275,  { "Apple Western European",                              Script::Latin    } },
	{ 1280,  { "Apple Greek",                                         Script::Greek    } },
	{ 1281,  { "Apple Turkish",                                       Script::Latin    } },
	{ 1282,  { "Apple Central European and Baltic",                   Script::Latin    } },
	{ 1283,  { "Apple Cyrillic",                                      Script::Cyrillic } },
	{ 1284,  { "Apple Croatian",                                      Script::Latin    } },
	{ 1285,  { "Apple Romanian",                                      Script::Latin    } },
	{ 1286,  { "Apple Icelandic",                                     Script::Latin    } },
	{ 58619, { "Apple Gaelic (old ortography), Welsh",                Script::Latin    } },
	{ 58627, { "Apple Ukrainian",                                     Script::Cyrillic } },
	{ 58630, { "Apple Saami, Kalo, Finnic, with EUR symbol",          Script::Latin    } },
}, {    // Windows series	
	{ 1250,  { "Windows Central European, with EUR symbol",           Script::Latin    } },
	{ 1251,  { "Windows Cyrillic, with EUR symbol",                   Script::Cyrillic } },
	{ 1252,  { "Windows Western European, with EUR symbol",           Script::Latin    } },
	{ 1253,  { "Windows Greek, with EUR symbol",                      Script::Greek    } },
	{ 1254,  { "Windows Turkish, with EUR symbol",                    Script::Latin    } },
	{ 1257,  { "Windows Baltic, with EUR symbol",                     Script::Latin    } },
	{ 1270,  { "Windows Saami, Kalo, Finnic, with EUR symbol",        Script::Latin    } },
	{ 1361,  { "Windows South European, with EUR symbol",             Script::Latin    } },
	{ 58595, { "Windows Kazakh, with EUR symbol",                     Script::Cyrillic } },
	{ 58596, { "Windows Georgian",                                    Script::Georgian } },
	{ 58598, { "Windows Azeri, with EUR symbol",                      Script::Latin    } },
	{ 59619, { "Windows Central Asian",                               Script::Cyrillic } },
	{ 59620, { "Windows Gaelic (old ortography), Welsh",              Script::Latin    } },
	{ 60643, { "Windows Northeastern Iranian",                        Script::Cyrillic } },
	{ 61667, { "Windows Inuit-Aleut",                                 Script::Cyrillic } },
	{ 62691, { "Windows Tungus-Manchu",                               Script::Cyrillic } },
} };
// clang-format on

// ***************************************************************************
// Keyboard layout information
// ***************************************************************************

// References:
// - https://gitlab.com/FreeDOS/base/keyb_lay/-/tree/master/DOC/KEYB/LAYOUTS

// NOTE: Default code pages for certain international layouts are set to 30023
// (Southern Africa) or 30026 (Central Africa) - these were selected based on
// the percentage of the language users (as checked in April 2024), see:
// - https://en.wikipedia.org/wiki/EF_English_Proficiency_Index
// - https://en.wikipedia.org/wiki/Geographical_distribution_of_French_speakers
// - https://en.wikipedia.org/wiki/List_of_countries_and_territories_where_Spanish_is_an_official_language
// - https://en.wikipedia.org/wiki/Portuguese_language (language usage map)

// clang-format off
const std::vector<KeyboardLayoutInfoEntry> LocaleData::KeyboardLayoutInfo = {
	// Layouts for English - 1.456 billion speakers worldwide
	{
		{ "us" }, "US (standard, QWERTY/national)",
		// A very popular keyboard layout
		AutodetectionPriority::High,
		437,
		KeyboardScript::LatinQwerty,
		{
			{ 30034, KeyboardScript::Cherokee },
		},
	},
	{
		{ "ux" }, "US (international, QWERTY)",
		// The QWERTY layer is almost very similar to the US layout,
		// but uses dead keys, which might be confusing
		AutodetectionPriority::Low,
		850,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "co" }, "US (Colemak)",
		// A very niche/exotic layout
		AutodetectionPriority::Low,
	 	437,
	 	KeyboardScript::LatinColemak,
	},
	{
		{ "dv" }, "US (Dvorak)",
		// A very niche/exotic layout
		AutodetectionPriority::Low,
		437,
		KeyboardScript::LatinDvorak,
	},
	{
		{ "lh" }, "US (left-hand Dvorak)",
		// A very niche/exotic layout
		AutodetectionPriority::Low,
		437,
		KeyboardScript::LatinDvorak,
	},
	{
		{ "rh" }, "US (right-hand Dvorak)",
		// A very niche/exotic layout
		AutodetectionPriority::Low,
	 	437,
	 	KeyboardScript::LatinDvorak,
	},
	{
		{ "uk" }, "UK (standard, QWERTY)",
		// A very popular keyboard layout
		AutodetectionPriority::High,
		437,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "uk168" }, "UK (alternate, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		437,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "kx" }, "UK (international, QWERTY)",
		// The QWERTY layer is almost identical to the UK layout
		AutodetectionPriority::High,
		30023,
		KeyboardScript::LatinQwerty,
	},
	// Layouts for other languages, sorted by the main symbol
	{
		{ "ar462" }, "Arabic (AZERTY/Arabic)",
		// QWERTY/AZERTY variant is selected later by a specialized code
		AutodetectionPriority::High,
		864,
		KeyboardScript::LatinAzerty,
		{
			{ 864, KeyboardScript::Arabic },
		},
	},
	{
		{ "ar470" }, "Arabic (QWERTY/Arabic)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		864,
		KeyboardScript::LatinQwerty,
		{
			{ 864, KeyboardScript::Arabic },
		},
	},
	{
		{ "az" }, "Azeri (QWERTY/Cyrillic)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		58210, // 60258 replaces ASCII 'I' with other symbol, avoid it!
		KeyboardScript::LatinQwerty,
		{
			{ 58210, KeyboardScript::Cyrillic },
			{ 60258, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ba" }, "Bosnian (QWERTZ)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		852,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "be" }, "Belgian (AZERTY)",
		// Assuming French speaking countries are mostly using the
		// AZERTY layouts
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinAzerty,
	},
	{
		{ "bx" }, "Belgian (international, AZERTY)",
		// Assuming French speaking countries are mostly using the
		// AZERTY layouts
		AutodetectionPriority::High,
		30026,
		KeyboardScript::LatinAzerty,
	},
	{
		{ "bg" }, "Bulgarian (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		// Code page approved by a native speaker
		3021, // MIK encoding
		KeyboardScript::LatinQwerty,
		{
			{ 808,  KeyboardScript::Cyrillic },
			{ 855,  KeyboardScript::Cyrillic },
			{ 866,  KeyboardScript::Cyrillic },
			{ 872,  KeyboardScript::Cyrillic },
			{ 3021, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "bg103" }, "Bulgarian (QWERTY/phonetic)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		// Code page approved by a native speaker
		3021, // MIK encoding
		KeyboardScript::LatinQwerty,
		{
			{ 808,  KeyboardScript::CyrillicPhonetic },
			{ 855,  KeyboardScript::CyrillicPhonetic },
			{ 866,  KeyboardScript::CyrillicPhonetic },
			{ 872,  KeyboardScript::CyrillicPhonetic },
			{ 3021, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "bg241" }, "Bulgarian (JCUKEN/national)",
		// Latin layer is atypical
		AutodetectionPriority::Low,
		// Code page approved by a native speaker
		3021, // MIK encoding
		KeyboardScript::LatinJcuken,
		{
			{ 808,  KeyboardScript::Cyrillic },
			{ 849,  KeyboardScript::Cyrillic },
			{ 855,  KeyboardScript::Cyrillic },
			{ 866,  KeyboardScript::Cyrillic },
			{ 872,  KeyboardScript::Cyrillic },
			{ 3021, KeyboardScript::Cyrillic },
		},
	}, 
	{
		{ "bn" }, "Beninese (AZERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		30027,
		KeyboardScript::LatinAzerty,
	},
	{
		{ "br" }, "Brazilian (ABNT layout, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		860,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "br274" }, "Brazilian (US layout, QWERTY)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		860,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "by", "bl" }, "Belarusian (QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		1131,
		KeyboardScript::LatinQwerty,
		{
			{ 849,  KeyboardScript::Cyrillic },
			{ 855,  KeyboardScript::Cyrillic },
			{ 872,  KeyboardScript::Cyrillic },
			{ 1131, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ce", }, "Chechen (standard, QWERTY/national)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		30019,
		KeyboardScript::LatinQwerty,
		{
			{ 30011, KeyboardScript::Cyrillic },
			{ 30019, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ce443", }, "Chechen (typewriter, QWERTY/national)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		30019,
		KeyboardScript::LatinQwerty,
		{
			{ 30011, KeyboardScript::Cyrillic },
			{ 30019, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "cf", "ca" }, "Canadian (standard, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		863,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "cf445" }, "Canadian (dual-layer, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		863,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "cg" }, "Montenegrin (QWERTZ)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		852,
		KeyboardScript::LatinQwertz,
	},
	// Use Kamenický encoding for Slovakia instead of Microsoft code page,
	// it is said to be much more popular.
	{
		{ "cz" }, "Czech (QWERTZ)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		867, // Kamenický encoding; no EUR variant, unfortunately
		KeyboardScript::LatinQwertz,
	},
	{
		{ "cz243" }, "Czech (standard, QWERTZ)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		867, // Kamenický encoding; no EUR variant, unfortunately
		KeyboardScript::LatinQwertz,
	},
	{
		{ "cz489" }, "Czech (programmers, QWERTY)",
		// Unintrusive US layout modification, uses right ALT for
		// entering the national characters
		AutodetectionPriority::High,
		867, // Kamenický encoding; no EUR variant, unfortunately
		KeyboardScript::LatinQwerty,
	},
	{
		{ "de", "gr" }, "German (standard, QWERTZ)",
		// Priority approved by a native speaker
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "gr453" }, "German (dual-layer, QWERTZ)",
		// German speaking countries seem to be mostly using the QWERTZ
		// layouts
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "dk" }, "Danish (QWERTY)",
		// National layouts seem to be popular in the Nordic region
		AutodetectionPriority::High,
		865,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "ee", "et" }, "Estonian (QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		1116, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
	},
	{
		{ "es", "sp" }, "Spanish (QWERTY)",
		// Not sure if the layout is popular, high priority for now
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "sx" }, "Spanish (international, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		30026,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "fi", "su" }, "Finnish (QWERTY/ASERTT)",
		// National layouts seem to be popular in the Nordic region
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwerty,
		{
			{ 30000, KeyboardScript::LatinAsertt },
		},
	},
	{
		{ "fo" }, "Faroese (QWERTY)",
		// National layouts seem to be popular in the Nordic region
		AutodetectionPriority::High,
		861,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "fr" }, "French (standard, AZERTY)",
		// Priority approved by a native speaker
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinAzerty,
	},
	{
		{ "fx" }, "French (international, AZERTY)",
		// Assuming French speaking countries are mostly using the
		// AZERTY layouts
		AutodetectionPriority::High,
		30026,
		KeyboardScript::LatinAzerty,
	},
	{
		{ "gk", "el" }, "Greek (319, QWERTY/national)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		869, // chosen because it has EUR currency symbol;
                     // TODO: native speaker feedback would be appreciated
		KeyboardScript::LatinQwerty,
		{
			{ 737, KeyboardScript::Greek },
			{ 851, KeyboardScript::Greek },
			{ 869, KeyboardScript::Greek },
		},
	},
	{
		{ "gk220" }, "Greek (220, QWERTY/national)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		869, // chosen because it has EUR currency symbol;
                     // TODO: native speaker feedback would be appreciated
		KeyboardScript::LatinQwerty,
		{
			{ 737, KeyboardScript::Greek },
			{ 851, KeyboardScript::Greek },
			{ 869, KeyboardScript::Greek },
		},
	},
	{
		{ "gk459" }, "Greek (459, non-standard/national)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		869, // chosen because it has EUR currency symbol;
                     // TODO: native speaker feedback would be appreciated
		KeyboardScript::LatinNonStandard,
		{
			{ 737, KeyboardScript::Greek },
			{ 851, KeyboardScript::Greek },
			{ 869, KeyboardScript::Greek },
		},
	},
	{
		{ "hr" }, "Croatian (QWERTZ/national)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		852,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "hu" }, "Hungarian (101-key, QWERTY)",
		// Priority approved by a native speaker
		AutodetectionPriority::Low,
		// Code page approved by a native speaker
		852,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "hu208" }, "Hungarian (102-key, QWERTZ)",
		// Priority approved by a native speaker
		AutodetectionPriority::Low,
		// Code page approved by a native speaker
		852,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "hy" }, "Armenian (QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		899,
		KeyboardScript::LatinQwerty,
		{
			{ 899, KeyboardScript::Armenian },
		},
	},
	{
		{ "il" }, "Hebrew (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		862,
		KeyboardScript::LatinQwerty,
		{
			{ 856, KeyboardScript::Hebrew },
			{ 862, KeyboardScript::Hebrew },
		},
	},
	{
		{ "is" }, "Icelandic (101-key, QWERTY)",
		// National layouts seem to be popular in the Nordic region
		AutodetectionPriority::High,
		861,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "is161" }, "Icelandic (102-key, QWERTY)",
		// National layouts seem to be popular in the Nordic region
		AutodetectionPriority::High,
		861,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "it" }, "Italian (standard, QWERTY/national)",
		// Priority approved by a native speaker
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwerty,
		{
			{ 869, KeyboardScript::Greek },
		},
	},
	{
		{ "it142" }, "Italian (142, QWERTY/national)",
		// Priority approved by a native speaker
		AutodetectionPriority::Low,
		850,
		KeyboardScript::LatinQwerty,
		{
			{ 869, KeyboardScript::Greek },
		},
	},
	{
		{ "ix" }, "Italian (international, QWERTY)",
		// Priority approved by a native speaker
		AutodetectionPriority::Low,
		30024,
		KeyboardScript::LatinQwerty,
	},
	// Japan layout disabled due to missing DBCS code pages support.
	// { { "jp" }, "Japanese", 932, ... },
	{
		{ "ka" }, "Georgian (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		59829,
		KeyboardScript::LatinQwerty,
		{
			{ 30008, KeyboardScript::Cyrillic },
			{ 59829, KeyboardScript::Georgian },
			{ 60853, KeyboardScript::Georgian },
		},
	},
	{
		{ "kk" }, "Kazakh (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		58152,
		KeyboardScript::LatinQwerty,
		{
			{ 58152, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "kk476" }, "Kazakh (476, QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		58152,
		KeyboardScript::LatinQwerty,
		{
			{ 58152, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ky" }, "Kyrgyz (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		58152,
		KeyboardScript::LatinQwerty,
		{
			{ 58152, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "la" }, "Latin American (QWERTY)",
		// Not sure if the layout is popular, high priority for now
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "lt" }, "Lithuanian (Baltic, QWERTY/phonetic)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		774, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
		{
			{ 771, KeyboardScript::CyrillicPhonetic },
			{ 772, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "lt210" }, "Lithuanian (programmers, QWERTY/phonetic)",
		// Unintrusive US layout modification, uses right ALT for
		// entering the national characters
		AutodetectionPriority::High,
		774, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
		{
			{ 771, KeyboardScript::CyrillicPhonetic },
			{ 772, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "lt211" }, "Lithuanian (AZERTY/phonetic)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		774, // No EUR currency variant, unfortunately
		KeyboardScript::LatinAzerty,
		{
			{ 771, KeyboardScript::CyrillicPhonetic },
			{ 772, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "lt221" }, "Lithuanian (LST 1582, AZERTY/phonetic)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		774, // No EUR currency variant, unfortunately
		KeyboardScript::LatinAzerty,
		{
			{ 771, KeyboardScript::CyrillicPhonetic },
			{ 772, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "lt456" }, "Lithuanian (QWERTY/AZERTY/phonetic)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		774, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
		{
			{ 770 , KeyboardScript::LatinAzerty },
			{ 771 , KeyboardScript::LatinAzerty },
			{ 772 , KeyboardScript::LatinAzerty },
			{ 773 , KeyboardScript::LatinAzerty },
			{ 774 , KeyboardScript::LatinAzerty },
			{ 775 , KeyboardScript::LatinAzerty },
			{ 777 , KeyboardScript::LatinAzerty },
			{ 778 , KeyboardScript::LatinAzerty },
		},
		{
			{ 771, KeyboardScript::CyrillicPhonetic },
			{ 772, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "lv" }, "Latvian (standard, QWERTY/phonetic)",
		// Unintrusive US layout modification, uses right ALT for
		// entering the national characters
		AutodetectionPriority::High,
		1117, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
		{
			{ 3012, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "lv455" }, "Latvian (QWERTY/UGJRMV/phonetic)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		1117, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
		{
			{  770, KeyboardScript::LatinUgjrmv },
			{  773, KeyboardScript::LatinUgjrmv },
			{  775, KeyboardScript::LatinUgjrmv },
			{ 1117, KeyboardScript::LatinUgjrmv },
			{ 3012, KeyboardScript::LatinUgjrmv },
		},
		{
			{ 3012, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "mk" }, "Macedonian (QWERTZ/national)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		855,
		KeyboardScript::LatinQwertz,
		{
			{ 855, KeyboardScript::Cyrillic },
			{ 872, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "mn", "mo" }, "Mongolian (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		58152,
		KeyboardScript::LatinQwerty,
		{
			{ 58152, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "mt", "ml" }, "Maltese (UK layout, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		853,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "mt103" }, "Maltese (US layout, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		853,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "ne" }, "Nigerien (AZERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		30028,
		KeyboardScript::LatinAzerty,
	},
	{
		{ "ng" }, "Nigerian (QWERTY)",
		// Unintrusive US layout modification, uses right ALT for
		// entering the national characters
		AutodetectionPriority::High,
		30005,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "nl" }, "Dutch (QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		850,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "no" }, "Norwegian (QWERTY/ASERTT)",
		// National layouts seem to be popular in the Nordic region
		AutodetectionPriority::High,
		865,
		KeyboardScript::LatinQwerty,
		{
			{ 30000, KeyboardScript::LatinAsertt },
		},
	},
	{
		{ "ph" }, "Filipino (QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		850,
		KeyboardScript::LatinQwerty,
	},
	// For Polish, use a stripped-down Microsoft code page 852 variant,
	// which preserves much more table drawing characters.
	{
		{ "pl" }, "Polish (programmers, QWERTY/phonetic)",
		// Priority approved by a native speaker
		AutodetectionPriority::High,
		// Code page approved by a native speaker
		668, // stripped-down 852
		KeyboardScript::LatinQwerty,
		{
			{ 848, KeyboardScript::CyrillicPhonetic },
			{ 849, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "pl214" }, "Polish (typewriter, QWERTZ/phonetic)",
		// Priority approved by a native speaker
		AutodetectionPriority::Low,
		// Code page approved by a native speaker
		668, // stripped-down 852
		KeyboardScript::LatinQwertz,
		{
			{ 848, KeyboardScript::CyrillicPhonetic },
			{ 849, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "po" }, "Portuguese (QWERTY)",
		// Not sure if the layout is popular, high priority for now
		AutodetectionPriority::High,
		860, // No EUR currency variant, unfortunately
		KeyboardScript::LatinQwerty,
	},
	{
		{ "px" }, "Portuguese (international, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		30026,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "ro" }, "Romanian (standard, QWERTZ/phonetic)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		852,
		KeyboardScript::LatinQwertz,
		{
			{ 848,   KeyboardScript::CyrillicPhonetic },
			{ 30010, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "ro446" }, "Romanian (QWERTY/phonetic)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		852,
		KeyboardScript::LatinQwerty,
		{
			{ 848,   KeyboardScript::CyrillicPhonetic },
			{ 30010, KeyboardScript::CyrillicPhonetic },
		},
	},
	{
		{ "ru" }, "Russian (standard, QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		866,
		KeyboardScript::LatinQwerty,
		{
			{ 808, KeyboardScript::Cyrillic },
			{ 855, KeyboardScript::Cyrillic },
			{ 866, KeyboardScript::Cyrillic },
			{ 872, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ru443" }, "Russian (typewriter, QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		866,
		KeyboardScript::LatinQwerty,
		{
			{ 808, KeyboardScript::Cyrillic },
			{ 855, KeyboardScript::Cyrillic },
			{ 866, KeyboardScript::Cyrillic },
			{ 872, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "rx" }, "Russian (extended standard, QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		30011,
		KeyboardScript::LatinQwerty,
		{
			{ 30011, KeyboardScript::Cyrillic },
			{ 30012, KeyboardScript::Cyrillic },
			{ 30013, KeyboardScript::Cyrillic },
			{ 30014, KeyboardScript::Cyrillic },
			{ 30015, KeyboardScript::Cyrillic },
			{ 30016, KeyboardScript::Cyrillic },
			{ 30017, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "rx443" }, "Russian (extended typewriter, QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		30011,
		KeyboardScript::LatinQwerty,
		{
			{ 30011, KeyboardScript::Cyrillic },
			{ 30012, KeyboardScript::Cyrillic },
			{ 30013, KeyboardScript::Cyrillic },
			{ 30014, KeyboardScript::Cyrillic },
			{ 30015, KeyboardScript::Cyrillic },
			{ 30016, KeyboardScript::Cyrillic },
			{ 30017, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "sd", "sg" }, "Swiss (German, QWERTZ)",
		// Priority approved by a native speaker
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "sf" }, "Swiss (French, QWERTZ)",
		// Assuming French speaking part of Switzerland also uses the
		// QWERTZ layouts
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "si" }, "Slovenian (QWERTZ)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		852,
		KeyboardScript::LatinQwertz,
	},
	// Use Kamenický encoding for Slovakia instead of Microsoft code page,
	// it is said to be much more popular.
	{
		{ "sk" }, "Slovak (QWERTZ)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		867, // Kamenický encoding; no EUR variant, unfortunately
		KeyboardScript::LatinQwertz,
	},
	{
		{ "sq" }, "Albanian (no deadkeys, QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		852,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "sq448" }, "Albanian (deadkeys, QWERTZ)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		852,
		KeyboardScript::LatinQwertz,
	},
	{
		{ "sv" }, "Swedish (QWERTY/ASERTT)",
		// A popular layout in Sweden (personal experience)
		AutodetectionPriority::High,
		850,
		KeyboardScript::LatinQwerty,
		{
			{ 30000, KeyboardScript::LatinAsertt },
		},
	},
	{
		{ "tj" }, "Tajik (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		30002,
		KeyboardScript::LatinQwerty,
		{
			{ 30002, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "tm" }, "Turkmen (QWERTY/phonetic)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		59234,
		KeyboardScript::LatinQwerty,
		{
			{ 59234, KeyboardScript::CyrillicPhonetic },
		},
	}, 
	{
		{ "tr" }, "Turkish (QWERTY)",
		// Not sure if the layout is popular, low priority for now
		AutodetectionPriority::Low,
		857,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "tr440" }, "Turkish (non-standard)",
		// Latin layer is atypical
		AutodetectionPriority::Low,
		857,
		KeyboardScript::LatinNonStandard,
	}, 
	{
		{ "tt" }, "Tatar (standard, QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		30018,
		KeyboardScript::LatinQwerty,
		{
			{ 30013, KeyboardScript::Cyrillic },
			{ 30018, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "tt443" }, "Tatar (typewriter, QWERTY/national)",
		// The QWERTY layer is almost identical to the US layout
		AutodetectionPriority::High,
		30018,
		KeyboardScript::LatinQwerty,
		{
			{ 30013, KeyboardScript::Cyrillic },
			{ 30018, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ur", "ua" }, "Ukrainian (101-key, QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		1125,
		KeyboardScript::LatinQwerty,
		{
			{ 808,   KeyboardScript::Cyrillic },
			{ 848,   KeyboardScript::Cyrillic },
			{ 857,   KeyboardScript::Cyrillic },
			{ 866,   KeyboardScript::Cyrillic },
			{ 1125,  KeyboardScript::Cyrillic },
			{ 30039, KeyboardScript::Cyrillic },
			{ 30040, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ur1996" }, "Ukrainian (101-key, 1996, QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		1125,
		KeyboardScript::LatinQwerty,
		{
			{ 808,   KeyboardScript::Cyrillic },
			{ 848,   KeyboardScript::Cyrillic },
			{ 857,   KeyboardScript::Cyrillic },
			{ 866,   KeyboardScript::Cyrillic },
			{ 1125,  KeyboardScript::Cyrillic },
			{ 30039, KeyboardScript::Cyrillic },
			{ 30040, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ur2001" }, "Ukrainian (102-key, 2001, QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		1125,
		KeyboardScript::LatinQwerty,
		{
			{ 808,   KeyboardScript::Cyrillic },
			{ 848,   KeyboardScript::Cyrillic },
			{ 857,   KeyboardScript::Cyrillic },
			{ 866,   KeyboardScript::Cyrillic },
			{ 1125,  KeyboardScript::Cyrillic },
			{ 30039, KeyboardScript::Cyrillic },
			{ 30040, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ur2007" }, "Ukrainian (102-key, 2007, QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		1125,
		KeyboardScript::LatinQwerty,
		{
			{ 808,   KeyboardScript::Cyrillic },
			{ 848,   KeyboardScript::Cyrillic },
			{ 857,   KeyboardScript::Cyrillic },
			{ 866,   KeyboardScript::Cyrillic },
			{ 1125,  KeyboardScript::Cyrillic },
			{ 30039, KeyboardScript::Cyrillic },
			{ 30040, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "ur465" }, "Ukrainian (101-key, 465, QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		1125,
		KeyboardScript::LatinQwerty,
		{
			{ 808,   KeyboardScript::Cyrillic },
			{ 848,   KeyboardScript::Cyrillic },
			{ 857,   KeyboardScript::Cyrillic },
			{ 866,   KeyboardScript::Cyrillic },
			{ 1125,  KeyboardScript::Cyrillic },
			{ 30039, KeyboardScript::Cyrillic },
			{ 30040, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "uz" }, "Uzbek (QWERTY/national)",
		// The QWERTY layer seems to be identical to the US layout
		AutodetectionPriority::High,
		62306,
		KeyboardScript::LatinQwerty,
		{
			{ 62306, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "vi" }, "Vietnamese (QWERTY)",
		// The layout differs a lot from the standard QWERTY
		AutodetectionPriority::Low,
		30006,
		KeyboardScript::LatinQwerty,
	},
	{
		{ "yc", "sr" }, "Serbian (deadkey, QWERTZ/national)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		855,
		KeyboardScript::LatinQwertz,
		{
			{ 848, KeyboardScript::Cyrillic },
			{ 855, KeyboardScript::Cyrillic },
			{ 872, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "yc450" }, "Serbian (no deadkey, QWERTZ/national)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		855,
		KeyboardScript::LatinQwertz,
		{
			{ 848, KeyboardScript::Cyrillic },
			{ 855, KeyboardScript::Cyrillic },
			{ 872, KeyboardScript::Cyrillic },
		},
	},
	{
		{ "yu" }, "Yugoslavian (QWERTZ)",
		// Balkan countries seem to be mostly using the QWERTZ layouts
		AutodetectionPriority::High,
		113,
		KeyboardScript::LatinQwertz,
	}
};
// clang-format on

// ***************************************************************************
// Country info - time/date format, currency, etc.
// ***************************************************************************

// clang-format off
const std::map<uint16_t, DosCountry> LocaleData::CodeToCountryCorrectionMap = {

        // Duplicates listed mentioned in Ralf Brown's Interrupt List and
        // confirmed using various COUNTRY.SYS versions:
        { 35,  DosCountry::Bulgaria },
        { 88,  DosCountry::Taiwan   }, // also Paragon PTS DOS standard code
        { 112, DosCountry::Belarus  }, // from Ralph Brown Interrupt List
        { 384, DosCountry::Croatia  }, // most likely a mistake in MS-DOS 6.22

        // Puerto Rico uses two telephone area codes, 787 and 939
        { 939, DosCountry::PuertoRico },
};
// clang-format on

// clang-format off
const std::map<DosCountry, CountryInfoEntry> LocaleData::CountryInfo = {
	{ DosCountry::International, { "International (English)", "XXA", { // stateless
		{ LocalePeriod::Modern, {
			// C
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 61
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Albania, { "Albania", "ALB", {
		{ LocalePeriod::Modern, {
			// sq_AL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "L" }, "ALL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 355
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Lek" }, "ALL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Algeria, { "Algeria", "DZA", {
		{ LocalePeriod::Modern, {
			// fr_DZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺟ.", "DA" }, "DZD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 213
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺟ.", "DA" }, "DZD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Andorra, { "Andorra", "AND", {
		{ LocalePeriod::Modern, {
			// ca_AD
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Arabic, { "Arabic (Middle East)", "XME", { // custom country code
		{ LocalePeriod::Modern, {
			// (common/representative values for Arabic languages)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "¤", "$" }, "USD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 785
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ", "¤", "$" }, "USD", 3,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Argentina, { "Argentina", "ARG", {
		{ LocalePeriod::Modern, {
			// es_AR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "ARS", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "ARS", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Armenia, { "Armenia", "ARM", {
		{ LocalePeriod::Modern, {
			// hy_AM
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "֏" }, "AMD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Aruba, { "Aruba", "ABW", {
		{ LocalePeriod::Modern, {
			// nl_AW
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Aƒ", "Afl" }, "AWG", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::AsiaEnglish, { "Asia (English)", "XAE", { // custom country code
		{ LocalePeriod::Modern, {
			// en_HK, en_MO, en_IN, en_PK
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "¤", "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 99
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Australia, { "Australia", "AUS", {
		{ LocalePeriod::Modern, {
			// en_AU
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "AU$", "$" }, "AUD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 61
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "AUD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Austria, { "Austria", "AUT", {
		{ LocalePeriod::Modern, {
			// de_AT
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 43
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "öS", "S" }, "ATS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Azerbaijan, { "Azerbaijan", "AZE", {
		{ LocalePeriod::Modern, {
			// az_AZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₼" }, "AZN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Bahrain, { "Bahrain", "BHR", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺑ.", "BD" }, "BHD", 3,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 973
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺑ.", "BD" }, "BHD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Belarus, { "Belarus", "BLR", {
		{ LocalePeriod::Modern, {
			// be_BY
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Руб", "Br" }, "BYN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 375
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Start currency from uppercase letter,
			// to match typical MS-DOS style.
			{ "р.", "p." }, "BYB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Belgium, { "Belgium", "BEL", {
		{ LocalePeriod::Modern, {
			// fr_BE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 32
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "BF" }, "BEF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Benin, { "Benin", "BEN", {
		{ LocalePeriod::Modern, {
			// fr_BJ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Bolivia, { "Bolivia", "BOL", {
		{ LocalePeriod::Modern, {
			// es_BO
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Bs" }, "BOB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 591
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Bs" }, "BOB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::BosniaLatin, { "Bosnia and Herzegovina (Latin)", "BIH_LAT", {
		{ LocalePeriod::Modern, {
			// bs_BA
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "KM" }, "BAM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 387
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Din" }, "BAD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::BosniaCyrillic, { "Bosnia and Herzegovina (Cyrillic)", "BIH_CYR", {
		{ LocalePeriod::Modern, {
			// bs_BA
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "КМ", "KM" }, "BAM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// PC-DOS 2000; country 388
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Дин", "Din" }, "BAD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma            // list separator
		} }
	} } },
	{ DosCountry::Botswana, { "Botswana", "BWA", {
		{ LocalePeriod::Modern, {
			// en_BW
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "P" }, "BWP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Brazil, { "Brazil", "BRA", {
		{ LocalePeriod::Modern, {
			// pt_BR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R$", "$" }, "BRL", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 55
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Cr$" }, "BRR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Bulgaria, { "Bulgaria", "BGR", {
		{ LocalePeriod::Modern, {
			// bg_BG
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 359
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "лв.", "lv." }, "BGL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::CanadaEnglish, { "Canada (English)", "CAN_EN", {
		{ LocalePeriod::Modern, {
			// en_CA
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 4
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::CanadaFrench, { "Canada (French)", "CAN_FR", {
		{ LocalePeriod::Modern, {
			// fr_CA
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 2
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Chile, { "Chile", "CHL", {
		{ LocalePeriod::Modern, {
			// es_CL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CLP", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 56
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "CLP", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::China, { "China", "CHN", {
		{ LocalePeriod::Modern, {
			// zh_CN
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "¥" }, "CNY", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 86
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "¥" }, "CNY", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Colombia, { "Colombia", "COL", {
		{ LocalePeriod::Modern, {
			// es_CO
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Col$", "$" }, "COP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 57
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "C$", "$" }, "COP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Congo, { "Congo", "COG", {
		{ LocalePeriod::Modern, {
			// ln_CD
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "FC" }, "CDF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::CostaRica, { "Costa Rica", "CRI", {
		{ LocalePeriod::Modern, {
			// es_CR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₡", "C" }, "CRC", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 506
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₡", "C" }, "CRC", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Croatia, { "Croatia", "HRV", {
		{ LocalePeriod::Modern, {
			// hr_HR
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 384
			// (most likely MS-DOS used wrong code instead of 385)
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Din" }, "HRD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Cuba, { "Cuba", "CUB", {
		{ LocalePeriod::Modern, {
			// es_CU
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "CUP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Czechia, { "Czechia", "CZE", {
		{ LocalePeriod::Modern, {
			// cs_CZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Kč", "Kc" }, "CZK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 42
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "Kč", "Kc" }, "CZK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Denmark, { "Denmark", "DNK", {
		{ LocalePeriod::Modern, {
			// da_DK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 45
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Ecuador, { "Ecuador", "ECU", {
		{ LocalePeriod::Modern, {
			// es_EC
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 593
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Egypt, { "Egypt", "EGY", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺟ.ﻣ.", "£E", "LE" }, "EGP", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 20
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺟ.ﻣ.", "£E", "LE" }, "EGP", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::ElSalvador, { "El Salvador", "SLV", {
		{ LocalePeriod::Modern, {
			// es_SV
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 503
			DosDateFormat::MonthDayYear, LocaleSeparator::Dash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₡", "C" }, "SVC", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Emirates, { "United Arab Emirates", "ARE", {
		{ LocalePeriod::Modern, {
			// en_AE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.", "DH" }, "AED", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 971
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.", "DH" }, "AED", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Eritrea, { "Eritrea", "ERI", {
		{ LocalePeriod::Modern, {
			// aa_ER, byn_ER, gez_ER, ssy_ER, tig_ER, ti_ER
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Nfk" }, "ERN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Estonia, { "Estonia", "EST", {
		{ LocalePeriod::Modern, {
			// et_EE
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 372
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Contrary to Windows ME results, 'KR' is a typical
			// Estonian kroon currency sign, not '$.'
			{ "KR" }, "EEK", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::FaroeIslands, { "Faroe Islands", "FRO", {
		{ LocalePeriod::Modern, {
			// fo_FO
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "DKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Finland, { "Finland", "FIN", {
		{ LocalePeriod::Modern, {
			// fi_FI
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 358
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "mk" }, "FIM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::France, { "France", "FRA", {
		{ LocalePeriod::Modern, {
			// fr_FR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 33
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "F" }, "FRF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Georgia, { "Georgia", "GEO", {
		{ LocalePeriod::Modern, {
			// ka_GE
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₾" }, "GEL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Germany, { "Germany", "DEU", {
		{ LocalePeriod::Modern, {
			// de_DE
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 49
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "DM" }, "DEM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Ghana, { "Ghana", "GHA", {
		{ LocalePeriod::Modern, {
			// ak_GH
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "GH₵" }, "GHS", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Greece, { "Greece", "GRC", {
		{ LocalePeriod::Modern, {
			// el_GR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 30
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₯", "Δρχ", "Dp" }, "GRD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Greenland, { "Greenland", "GRL", {
		{ LocalePeriod::Modern, {
			// kl_GL
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "kr." }, "DKK", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Guatemala, { "Guatemala", "GTM", {
		{ LocalePeriod::Modern, {
			// es_GT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Q" }, "GTQ", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 502
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Q" }, "GTQ", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Haiti, { "Haiti", "HTI", {
		{ LocalePeriod::Modern, {
			// ht_HT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "G" }, "HTG", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Honduras, { "Honduras", "HND", {
		{ LocalePeriod::Modern, {
			// es_HN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "L" }, "HNL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 504
			DosDateFormat::MonthDayYear, LocaleSeparator::Dash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "L" }, "HNL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::HongKong, { "Hong Kong", "HKG", {
		{ LocalePeriod::Modern, {
			// en_HK, zh_HK
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "HK$", "$" }, "HKD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 852
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "HK$", "$" }, "HKD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Hungary, { "Hungary", "HUN", {
		{ LocalePeriod::Modern, {
			// hu_HU
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ft" }, "HUF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 36
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "Ft" }, "HUF", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Iceland, { "Iceland", "ISL", {
		{ LocalePeriod::Modern, {
			// is_IS
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "ISK", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 354
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "ISK", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::India, { "India", "IND", {
		{ LocalePeriod::Modern, {
			// hi_IN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₹", "Rs" }, "INR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 91
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Rs" }, "INR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Indonesia, { "Indonesia", "IDN", {
		{ LocalePeriod::Modern, {
			// id_ID
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Rp" }, "IDR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 62
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Rp" }, "IDR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Ireland, { "Ireland", "IRL", {
		{ LocalePeriod::Modern, {
			// en_IE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 353
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "IR£" }, "IEP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Israel, { "Israel", "ISR", {
		{ LocalePeriod::Modern, {
			// he_IL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₪" }, "NIS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 972
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₪", "NIS" }, "NIS", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Italy, { "Italy", "ITA", {
		{ LocalePeriod::Modern, {
			// it_IT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 39
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "L." }, "ITL", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Japan, { "Japan", "JPN", {
		{ LocalePeriod::Modern, {
			// ja_JP
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "¥" }, "JPY", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 81
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "¥" }, "JPY", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Jordan, { "Jordan", "JOR", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺍ.", "JD" }, "JOD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 962
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺍ.", "JD" }, "JOD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Kazakhstan, { "Kazakhstan", "KAZ", {
		{ LocalePeriod::Modern, {
			// kk_KZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₸" }, "KZT", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Kenya, { "Kenya", "KEN", {
		{ LocalePeriod::Modern, {
			// om_KE, so_KE, sw_KE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ksh" }, "KES", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Kuwait, { "Kuwait", "KWT", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "KD" }, "KWD", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 965
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "KD" }, "KWD", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Kyrgyzstan, { "Kyrgyzstan", "KGZ", {
		{ LocalePeriod::Modern, {
			// ky_KG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "⃀", "сом" }, "KGS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::LatinAmerica, { "Latin America", "XLA", { // custom country code
		{ LocalePeriod::Modern, {
			// es_419
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			//  MS-DOS 6.22; country 3
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Latvia, { "Latvia", "LVA", {
		{ LocalePeriod::Modern, {
			// lv_LV
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 371
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ls" }, "LVL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Lebanon, { "Lebanon", "LBN", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "LL" }, "LBP", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 961
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻛ.", "LL" }, "LBP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Liechtenstein, { "Liechtenstein", "LIE", {
		{ LocalePeriod::Modern, {
			// de_LI
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Fr." }, "CHF", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Apostrophe,    // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Lithuania, { "Lithuania", "LTU", {
		{ LocalePeriod::Modern, {
			// lt_LT
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 370
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Lt" }, "LTL", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,       // thousands separator
			LocaleSeparator::Comma,        // decimal separator
			LocaleSeparator::Semicolon     // list separator
		} }
	} } },
	{ DosCountry::Luxembourg, { "Luxembourg", "LUX", {
		{ LocalePeriod::Modern, {
			// lb_LU
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 352
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "F" }, "LUF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Madagascar, { "Madagascar", "MDG", {
		{ LocalePeriod::Modern, {
			// mg_MG,
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ar" }, "MGA", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Malaysia, { "Malaysia", "MYS", {
		{ LocalePeriod::Modern, {
			// ms_MY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "RM" }, "MYR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 60
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$", "M$" }, "MYR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Malta, { "Malta", "MLT", {
		{ LocalePeriod::Modern, {
			// mt_MT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Mexico, { "Mexico", "MEX", {
		{ LocalePeriod::Modern, {
			// es_MX
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Mex$", "$" }, "MXN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 52
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "N$" }, "MXN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma            // list separator
		} }
	} } },
	{ DosCountry::Monaco, { "Monaco", "MCO", {
		{ LocalePeriod::Modern, {
			// fr_FR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 33
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "F" }, "FRF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Mongolia, { "Mongolia", "MNG", {
		{ LocalePeriod::Modern, {
			// mn_MN
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₮" }, "MNT", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Montenegro, { "Montenegro", "MNE", {
		{ LocalePeriod::Modern, {
			// sr_ME
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 381, but with DM currency
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "DM" }, "DEM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Morocco, { "Morocco", "MAR", {
		{ LocalePeriod::Modern, {
			// fr_MA
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻣ.", "DH" }, "MAD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 212
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ﺩ.ﻣ.", "DH" }, "MAD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Netherlands, { "Netherlands", "NLD", {
		{ LocalePeriod::Modern, {
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 31
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ƒ", "f" }, "NLG", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::NewZealand, { "New Zealand", "NZL", {
		{ LocalePeriod::Modern, {
			// en_NZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "NZD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 64
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$" }, "NZD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Nicaragua, { "Nicaragua", "NIC", {
		{ LocalePeriod::Modern, {
			// es_NI
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "C$" }, "NIO", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 505
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$C" }, "NIO", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Niger, { "Niger", "NER", {
		{ LocalePeriod::Modern, {
			// fr_NE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Nigeria, { "Nigeria", "NGA", {
		{ LocalePeriod::Modern, {
			// en_NG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₦" }, "NGN", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::NorthMacedonia, { "North Macedonia", "MKD", {
		{ LocalePeriod::Modern, {
			// mk_MK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ден.", "ден", "den.", "den" }, "MKD", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 389
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ден", "Den" }, "MKD", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Norway, { "Norway", "NOR", {
		{ LocalePeriod::Modern, {
			// nn_NO
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "NOK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 47
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "NOK", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Oman, { "Oman", "OMN", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻋ.", "R.O" }, "OMR", 3,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 968
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻋ.", "R.O" }, "OMR", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Pakistan, { "Pakistan", "PAK", {
		{ LocalePeriod::Modern, {
			// en_PK
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Rs" }, "PKR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 92
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Rs" }, "PKR", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Panama, { "Panama", "PAN", {
		{ LocalePeriod::Modern, {
			// es_PA
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "B/." }, "PAB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 507
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "B" }, "PAB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Paraguay, { "Paraguay", "PRY", {
		{ LocalePeriod::Modern, {
			// es_PY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₲", "Gs." }, "PYG", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 595
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₲", "G" }, "PYG", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Peru, { "Peru", "PER", {
		{ LocalePeriod::Modern, {
			// es_PE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "S/" }, "PEN", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Philippines, { "Philippines", "PHL", {
		{ LocalePeriod::Modern, {
			// fil_PH
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "₱" }, "PHP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Poland, { "Poland", "POL", {
		{ LocalePeriod::Modern, {
			// pl_PL
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// TODO: Support 'zł' symbol from code pages 991 and 58335
			{ "zł", "zl" }, "PLN", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 48
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Zł", "Zl" }, "PLZ", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Portugal, { "Portugal", "PRT", {
		{ LocalePeriod::Modern, {
			// pt_PT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 351
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Esc." }, "PTE", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::PuertoRico, { "Puerto Rico", "PRI", {
		{ LocalePeriod::Modern, {
			// es_PR
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Qatar, { "Qatar", "QAT", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻗ.", "QR" }, "QAR", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 974
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻗ.", "QR" }, "QAR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Romania, { "Romania", "ROU", {
		{ LocalePeriod::Modern, {
			// ro_RO
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// TODO: Romania is expected to switch currency to EUR
                        // soon - adapt this when it happens
			{ "Lei" }, "RON", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 40
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Lei" }, "ROL", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Russia, { "Russia", "RUS", {
		{ LocalePeriod::Modern, {
			// ru_RU
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₽", "руб", "р.", "Rub" }, "RUB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 7
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₽", "р.", "р.", }, "RUB", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Rwanda, { "Rwanda", "RWA", {
		{ LocalePeriod::Modern, {
			// rw_RW
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R₣", "RF" }, "RWF", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::SanMarino, { "San Marino", "SMR", {
		{ LocalePeriod::Modern, {
			// it_IT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 39
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "L." }, "ITL", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::SaudiArabia, { "Saudi Arabia", "SAU", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﺳ.", "SR" }, "SAR", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 966
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﺳ.", "SR" }, "SAR", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Senegal, { "Senegal", "SEN", {
		{ LocalePeriod::Modern, {
			// wo_SN
			DosDateFormat::DayMonthYear, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "FCFA" }, "XOF", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Serbia, { "Serbia", "SRB", {
		{ LocalePeriod::Modern, {
			// sr_RS
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "дин", "DIN" }, "RSD", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 381
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Дин", "Din" }, "RSD", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Seychelles, { "Seychelles", "SYC", {
		{ LocalePeriod::Modern, {
			// en_SC
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "SR" }, "SCR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Singapore, { "Singapore", "SGP", {
		{ LocalePeriod::Modern, {
			// ms_SG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "S$", "$" }, "SGD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 65
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "SGD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Slovakia, { "Slovakia", "SVK", {
		{ LocalePeriod::Modern, {
			// sk_SK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 421
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "Sk" }, "SKK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Slovenia, { "Slovenia", "SVN", {
		{ LocalePeriod::Modern, {
			// sl_SI
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 386
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// MS-DOS 6.22 seems to be wrong here, Slovenia used
			// tolars, not dinars; used definition from PC-DOS 2000
			{ }, "SIT", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::SouthAfrica, { "South Africa", "ZAF", {
		{ LocalePeriod::Modern, {
			// af_ZA
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R" }, "ZAR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 27
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "R" }, "ZAR", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::SouthKorea, { "South Korea", "KOR", {
		{ LocalePeriod::Modern, {
			// ko_KR
			DosDateFormat::YearMonthDay, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₩", "W" }, "KRW", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separatorr
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 82
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// MS-DOS states precision is 2, but Windows ME states
			// it is 0. Given that even back then 1 USD was worth
			// hundreds South Korean wons, 0 is more sane.
			{ "₩", "W" }, "KRW", 0,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Spain, { "Spain", "ESP", {
		{ LocalePeriod::Modern, {
			// es_ES
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 34
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₧", "Pts" }, "ESP ", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Sweden, { "Sweden", "SWE", {
		{ LocalePeriod::Modern, {
			// sv_SE
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "SEK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 46
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "kr" }, "SEK", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Switzerland, { "Switzerland", "CHE", {
		{ LocalePeriod::Modern, {
			// de_CH
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Fr." }, "CHF", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Apostrophe,    // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 41
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "SFr." }, "CHF", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Apostrophe,    // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Syria, { "Syria", "SYR", {
		{ LocalePeriod::Modern, {
			// fr_SY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﻟ.ﺳ.", "LS" }, "SYP", 0,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 963
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﻟ.ﺳ.", "LS" }, "SYP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Tajikistan, { "Tajikistan", "TJK", {
		{ LocalePeriod::Modern, {
			// tg_TJ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "сом", "SM" }, "TJS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Taiwan, { "Taiwan", "TWN", {
		{ LocalePeriod::Modern, {
			// zh_TW
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "NT$", "NT" }, "TWD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 886
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "NT$", }, "TWD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Tanzania, { "Tanzania", "TZA", {
		{ LocalePeriod::Modern, {
			// sw_TZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Tsh" }, "TZS", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Thailand, { "Thailand", "THA", {
		{ LocalePeriod::Modern, {
			// th_TH
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "฿", "B" }, "THB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// Windows ME; country 66
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Windows ME uses dollar symbol for currency, this 
			// looks wrong, or perhaps it is a workaround for some
			// OS limitation
			{ "฿", "B" }, "THB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Tonga, { "Tonga", "TON", {
		{ LocalePeriod::Modern, {
			// to_TO
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "T$", "PT" }, "TOP", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Tunisia, { "Tunisia", "TUN", {
		{ LocalePeriod::Modern, {
			// fr_TN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺗ.", "DT" }, "TND", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 216
			DosDateFormat::YearMonthDay, LocaleSeparator::Dash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "ﺩ.ﺗ.", "DT" }, "TND", 3,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Slash          // list separator
		} }
	} } },
	{ DosCountry::Turkey, { "Turkey", "TUR", {
		{ LocalePeriod::Modern, {
			// tr_TR
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₺", "TL" }, "TRY", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 90
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₺", "TL" }, "TRL", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Turkmenistan, { "Turkmenistan", "TKM", {
		{ LocalePeriod::Modern, {
			// tk_TM
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "m" }, "TMT", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Uganda, { "Uganda", "UGA", {
		{ LocalePeriod::Modern, {
			// lg_UG
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Ush" }, "UGX", 0,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Ukraine, { "Ukraine", "UKR", {
		{ LocalePeriod::Modern, {
			// uk_UA
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₴", "грн", "hrn" }, "UAH", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// FreeDOS 1.3, Windows ME; country 380
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			// Start currency from uppercase letter,
			// to match typical MS-DOS style.
			// Windows ME has a strange currency symbol,
			// so the whole format was taken from FreeDOS.
			{ "₴", "Грн", "Hrn" }, "UAH", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::UnitedKingdom, { "United Kingdom", "GBR", {
		{ LocalePeriod::Modern, {
			// en_GB
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "£" }, "GBP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 44
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "£" }, "GBP", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::UnitedStates, { "United States", "USA", {
		{ LocalePeriod::Modern, {
			// en_US
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 1
			DosDateFormat::MonthDayYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "$" }, "USD", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Uruguay, { "Uruguay", "URY", {
		{ LocalePeriod::Modern, {
			// es_UY
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "$U", "$" }, "UYU", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 598
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "NU$", "$" }, "UYU", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Uzbekistan, { "Uzbekistan", "UZB", {
		{ LocalePeriod::Modern, {
			// uz_UZ
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "сўм", "soʻm", "so'm", "som" }, "UZS", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Space,         // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::VaticanCity, { "Vatican City", "VAT", {
		{ LocalePeriod::Modern, {
			// it_IT
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "€" }, "EUR", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 39
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Period,
			{ "L." }, "ITL", 0,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Venezuela, { "Venezuela", "VEN", {
		{ LocalePeriod::Modern, {
			// es_VE
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Bs.F" }, "VEF", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 58
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "Bs." }, "VEB", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Vietnam, { "Vietnam", "VNM", {
		{ LocalePeriod::Modern, {
			// vi_VN
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "₫", "đ" }, "VND", 0,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }
	} } },
	{ DosCountry::Yemen, { "Yemen", "YEM", {
		{ LocalePeriod::Modern, {
			// (taken from the common Arabic, adapted the currency)
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻱ.", "YRI" }, "YER", 2,
			DosCurrencyFormat::AmountSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// OS/2 Warp 4.52; country 967
			DosDateFormat::YearMonthDay, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "ﺭ.ﻱ.", "YRI" }, "YER", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Semicolon      // list separator
		} }
	} } },
	{ DosCountry::Yugoslavia, { "Yugoslavia", "YUG", { // obsolete country code
		{ LocalePeriod::Modern, {
			// sr_RS, sr_ME, hr_HR, sk_SK, bs_BA, mk_MK
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "дин.", "дин", "din.", "din" }, "YUM", 2,
			DosCurrencyFormat::AmountSpaceSymbol,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
		} }, { LocalePeriod::Historic, {
			// MS-DOS 6.22; country 38
			DosDateFormat::DayMonthYear, LocaleSeparator::Period,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ "Din" }, "YUM", 2,
			DosCurrencyFormat::SymbolSpaceAmount,
			LocaleSeparator::Period,        // thousands separator
			LocaleSeparator::Comma,         // decimal separator
			LocaleSeparator::Comma          // list separator
		} }
	} } },
	{ DosCountry::Zambia, { "Zambia", "ZMB", {
		{ LocalePeriod::Modern, {
			// bem_ZM, en_ZM
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time12H,      LocaleSeparator::Colon,
			{ "K" }, "ZMW", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } },
	{ DosCountry::Zimbabwe, { "Zimbabwe", "ZWE", {
		{ LocalePeriod::Modern, {
			// en_ZW
			DosDateFormat::DayMonthYear, LocaleSeparator::Slash,
			DosTimeFormat::Time24H,      LocaleSeparator::Colon,
			{ }, "ZWG", 2,
			DosCurrencyFormat::SymbolAmount,
			LocaleSeparator::Comma,         // thousands separator
			LocaleSeparator::Period,        // decimal separator
		} }
	} } }
};
// clang-format on
