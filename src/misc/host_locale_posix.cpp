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

#if !defined(WIN32)
#if !C_COREFOUNDATION

#include "host_locale.h"

#include "checks.h"
#include "dosbox.h"
#include "fs_utils.h"
#include "std_filesystem.h"
#include "string_utils.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>

CHECK_NARROWING();

// ***************************************************************************
// Detection data
// ***************************************************************************

// Names of various environment variables used
static const std::string LcAll             = "LC_ALL";
static const std::string LcMessages        = "LC_MESSAGES";
static const std::string LcNumeric         = "LC_NUMERIC";
static const std::string LcMonetary        = "LC_MONETARY";
static const std::string LcTime            = "LC_TIME";
static const std::string LcTelephone       = "LC_TELEPHONE";
static const std::string XdgCurrentDesktop = "XDG_CURRENT_DESKTOP";
static const std::string XdgSessionDesktop = "XDG_SESSION_DESKTOP";
static const std::string VariableLang      = "LANG";
static const std::string VariableLanguage  = "LANGUAGE";
static const std::string WayfireConfigFile = "WAYFIRE_CONFIG_FILE";

// Strings for logging
static const std::string SourceTty     = "TTY: ";
static const std::string SourceKde     = "KDE: ";
static const std::string SourceGnome   = "GNOME: ";
static const std::string SourceWayfire = "WAYFIRE: ";

// These keyboard models are going to be considered as 102-key
// keyboards, as they have a '>/<' key between right shift and 'Z'
static const std::set<std::string> KeyboardModels102 = {"pc102",
                                                        "pc104alt",
                                                        "pc105",
                                                        "pc105+inet"};

// Mapping from X11 to DOS keyboard layout. Reference:
// - /usr/share/X11/xkb/rules/evdev.lst
// - 'localectl list-x11-keymap-variants <layout>' command
// clang-format off
static const std::map<std::string, KeyboardLayoutMaybeCodepage> X11ToDosKeyboard = {
	// US (standard, QWERTY/national)
	{ "us",                         { "us" }         },
	{ "us:chr",                     { "us", 30034 }  }, // Cherokee
	{ "us:haw",                     { "us", 30021 }  }, // Hawaiian
	{ "au",                         { "us" }         }, // Australia
	{ "cm",                         { "us" }         }, // Cameroon
	{ "epo",                        { "us", 853 }    }, // Esperanto
	{ "pt:nativo-epo",              { "us", 853 }    },
	{ "nl:mac",                     { "us" }         }, // Netherlands
	{ "nl:us",                      { "us" }         },
	{ "nz",                         { "us" }         }, // New Zealand
	{ "nz:mao",                     { "us", 30021 }  }, // Maori
	// US (international, QWERTY)
	{ "us:intl",                    { "ux" }         },
	{ "us:alt-intl",                { "ux" }         },
	{ "us:altgr-intl",              { "ux" }         },
	{ "bw",                         { "ux", 30023 }  }, // Tswana
	// TODO: Is 30024 or 30026 a better one for the Swahili language?
	{ "ke",                         { "ux", 30024 }  }, // Kenya (Swahili)
	{ "tz",                         { "ux", 30024 }  }, // Tanzania (Swahili)
	{ "za",                         { "ux", 30023 }  }, // South Africa
	// US (Colemak)
	{ "us:colemak",                 { "co" }         },
	{ "us:colemak_dh",              { "co" }         },
	{ "us:colemak_dh_iso",          { "co" }         },
	{ "us:colemak_dh_ortho",        { "co" }         },
	{ "us:colemak_dh_wide",         { "co" }         },
	{ "us:colemak_dh_wide_iso",     { "co" }         },
	{ "gb:colemak",                 { "co" }         }, // UK
	{ "gb:colemak_dh",              { "co" }         },
	{ "latam:colemak",              { "co", 850 }    }, // Latin America
	{ "no:colemak",                 { "co" }         }, // Norway
	{ "no:colemak_dh",              { "co" }         },
	{ "no:colemak_dh_wide",         { "co" }         },
	{ "ph:colemak",                 { "co" }         }, // Philippines
	{ "ph:colemak-bay",             { "co" }         },
	// US (Dvorak)
	{ "us:dvorak",                  { "dv" }         },
	{ "us:dvorak-alt-intl",         { "dv" }         },
	{ "us:dvorak-classic",          { "dv" }         },
	{ "us:dvorak-intl",             { "dv" }         },
	{ "us:dvorak-mac",              { "dv" }         },
	{ "us:dvp",                     { "dv" }         },
	{ "gb:dvorak",                  { "dv" }         }, // UK
	{ "gb:dvorakukp",               { "dv" }         },
	{ "br:dvorak",                  { "dv", 850 }    }, // Brasilia
	{ "ca:fr-dvorak",               { "dv", 850 }    }, // Canada
	{ "cm:dvorak",                  { "dv" }         }, // Cameroon
	{ "cz:dvorak-ucw",              { "dv", 850 }    }, // Czechia
	{ "de:dvorak",                  { "dv", 850 }    }, // Germany
	{ "dk:dvorak",                  { "dv" }         }, // Denmark
	{ "ee:dvorak",                  { "dv" }         }, // Estonia
	{ "es:dvorak",                  { "dv" }         }, // Spain
	{ "fr:dvorak",                  { "dv", 850 }    }, // France
	{ "is:dvorak",                  { "dv", 850 }    }, // Iceland
	{ "jp:dvorak",                  { "dv" }         }, // Japan
	{ "latam:dvorak",               { "dv", 850 }    }, // Latin America
	{ "no:dvorak",                  { "dv" }         }, // Norway
	{ "ph:capewell-dvorak",         { "dv" }         }, // Philippines
	{ "ph:capewell-dvorak-bay",     { "dv" }         },
	{ "ph:dvorak",                  { "dv" }         },
	{ "ph:dvorak-bay",              { "dv" }         },
	{ "pl:dvorak",                  { "dv" }         }, // Poland
	{ "pl:dvorak_quotes",           { "dv" }         },
	{ "pl:dvorak_altquotes",        { "dv" }         },
	{ "pl:ru_phonetic_dvorak",      { "dv" }         },
	{ "ru:phonetic_dvorak",         { "dv", 850 }    }, // Russia
	{ "se:dvorak",                  { "dv", 850 }    }, // Sweden
	{ "se:svdvorak",                { "dv", 850 }    },
	{ "se:us_dvorak",               { "dv", 850 }    },
	// US (left-hand Dvorak)
	{ "us:dvorak-l",                { "lh" }         },
	// US (right-hand Dvorak)
	{ "us:dvorak-r",                { "rh" }         },
	// UK (standard, QWERTY)
	{ "gb",	                        { "uk" }         },
	{ "gb:gla",                     { "uk", 30001 }  }, // Scottish Gaelic
	{ "ie",	                        { "uk" }         }, // Ireland
	// UK (international, QWERTY)
	{ "gb:intl",                    { "kx" }         },
	{ "gb:mac_intl",                { "kx" }         },
	// Arabic (AZERTY/Arabic)
	{ "ara",                        { "ar462" }      },
	{ "dz:azerty-deadkeys",         { "ar462" }      }, // Algeria
	{ "ma",                         { "ar462" }      }, // Morocco
	// Arabic (QWERTY/Arabic)
	{ "af",                         { "ar470" }      }, // Dari
	{ "cn:ug",                      { "ar470" }      }, // Uyghur
	{ "dz",                         { "ar470" }      }, // Algeria
	{ "eg",                         { "ar470" }      }, // Egypt
	{ "id:melayu-phonetic",         { "ar470" }      }, // Indonesia
	{ "id:melayu-phoneticx",        { "ar470" }      },
	{ "id:pegon-phonetic",          { "ar470" }      },
	{ "iq",                         { "ar470" }      }, // Iraq
	{ "ir",                         { "ar470" }      }, // Iran
	{ "my",                         { "ar470" }      }, // Malaysia
	{ "pk",                         { "ar470" }      }, // Pakistan
	{ "sy",                         { "ar470" }      }, // Syria
	// Azeri (QWERTY/Cyrillic)"
	{ "az",                         { "az" }         },
	// Bosnian (QWERTZ)
	{ "ba",                         { "ba" }         },
	// Belgian (AZERTY)
	{ "be",                         { "be" }         },
	// Bulgarian (QWERTY/national)
	{ "bg",                         { "bg" }         },
	// Bulgarian (QWERTY/phonetic)
	{ "bg:phonetic",                { "bg103" }      },
	{ "bg:bas_phonetic",            { "bg103" }      },
	// Brazilian (ABNT layout, QWERTY)
	{ "br",                         { "br" }         },
	// Belarusian (QWERTY/national)
	{ "by",                         { "by" }         },
	// Canadian (standard, QWERTY)
	{ "ca",                         { "cf" }         },
	{ "ca:ike",                     { "cf", 30022 }  }, // Inuktitut
	// Canadian (dual-layer, QWERTY)
	{ "ca:fr-legacy",               { "cf445" }      },
	// Montenegrin (QWERTZ)
	{ "me",                         { "cg" }         },
	// Czech (standard, QWERTZ)
	{ "cz",                         { "cz243" }      },
	// Czech (programmers, QWERTY)
	{ "cz:qwerty",                  { "cz489" }      },
	{ "cz:qwerty-mac",              { "cz489" }      },
	{ "cz:qwerty_bksl",             { "cz489" }      },
	{ "cz:winkeys-qwerty",          { "cz489" }      },
	// German (standard, QWERTZ)
	{ "de:mac",                     { "de" }         },
	{ "de:mac_nodeadkeys",          { "de" }         },
	{ "de:neo",                     { "de" }         },
	{ "at:mac",                     { "de" }         }, // Austria	
	// German (dual-layer, QWERTZ)
	{ "de",                         { "gr453" }      },
	{ "de:dsb_qwertz",              { "gr453", 852 } }, // Sorbian
	{ "de:hu",                      { "gr453", 852 } }, // German with Hungarian letters
	{ "de:pl",                      { "gr453", 852 } }, // German with Polish letters
	{ "at",                         { "gr453" }      }, // Austria
	// Danish (QWERTY)
	{ "dk",                         { "dk" }         },
	// Estonian (QWERTY)
	{ "ee",                         { "ee" }         },
	// Spanish (QWERTY)
	{ "es",                         { "es" }         },
	{ "es:cat",                     { "es", 30007 }  }, // Catalan
	// Finnish (QWERTY/ASERTT)
	{ "fi",                         { "fi" }         },
	{ "fi:smi",                     { "fi", 30000 }  }, // Saami
	// Faroese (QWERTY)
	{ "fo",                         { "fo" }         },
	// French (standard, AZERTY)
	{ "fr",                         { "fr" }         },
	// French (international, AZERTY)
	{ "cd",                         { "fx", 30026 }  }, // Congo
	{ "cm:azerty",                  { "fx", 30026 }  }, // Cameroon
	{ "cm:french",                  { "fx", 30026 }  },
	{ "ma:french",                  { "fx", 30025 }  }, // Morocco
	{ "ml",                         { "fx", 30025 }  }, // Bambara, Mali
	// TODO: Is 30024 or 30025 a better one for the Wolof language?
	{ "sn",                         { "fx", 30025 }  }, // Wolof
	{ "tg",                         { "fx", 30025 }  }, // Togo
	// Greek (319, QWERTY/national)
	{ "gr",	                        { "gk" }         },
	// Croatian (QWERTZ/national)
	{ "hr",                         { "hr" }         },
	// Hungarian (101-key, QWERTY)
	{ "hu",                         { "hu" }         },
	{ "hu:101_qwertz_comma_dead",   { "hu" }         },
	{ "hu:101_qwertz_comma_nodead", { "hu" }         },
	{ "hu:101_qwertz_dot_dead",     { "hu" }         },
	{ "hu:101_qwertz_dot_nodead",   { "hu" }         },
	// Hungarian (102-key, QWERTZ)
	{ "hu:102_qwertz_comma_dead",   { "hu208" }      },
	{ "hu:102_qwertz_comma_nodead", { "hu208" }      },
	{ "hu:102_qwertz_dot_dead",     { "hu208" }      },
	{ "hu:102_qwertz_dot_nodead",   { "hu208" }      },
	// Armenian (QWERTY/national)
	{ "am",                         { "hy" }         },
	// Hebrew (QWERTY/national)
	{ "il",                         { "il" }         },
	// Icelandic (101-key, QWERTY)
	{ "is",                         { "is" }         },
	// Italian (standard, QWERTY/national)
	{ "it",                         { "it" }         },
	{ "it:lld",                     { "it", 30007 }  }, // Ladin
	{ "fr:oci ",                    { "it", 30007 }  }, // Occitan
	// Italian (142, QWERTY/national)
	{ "it:ibm",                     { "it142" }      },
	{ "it:mac",                     { "it142" }      },
	// Georgian (QWERTY/national)
	{ "ge",                         { "ka" }         },
	{ "ge:os",                      { "ka", 30008 }  }, // Ossetian
	{ "ru:ab",                      { "ka", 30008 }  }, // Abkhazian
	// Kazakh (QWERTY/national)
	{ "kz:kazrus",                  { "kk" }         },
	{ "kz:ruskaz",                  { "kk" }         },
	// Kazakh (476, QWERTY/national)
	{ "kz",                         { "kk476" }      },
	// Kyrgyz (QWERTY/national)
	{ "kg",                         { "ky" }         },
	// Latin American (QWERTY)
	{ "latam",                      { "la" }         },
	// Lithuanian (Baltic, QWERTY/phonetic)
	{ "lt",                         { "lt" }         },
	// Lithuanian (programmers, QWERTY/phonetic)
	{ "lt:us",                      { "lt210" }      },
	{ "lt:lekp",                    { "lt210" }      },
	{ "lt:lekpa",                   { "lt210" }      },
	{ "lt:ratise",                  { "lt210" }      },
	// Lithuanian (AZERTY/phonetic)
	{ "lt:ibm",                     { "lt211" }      },
	// Lithuanian (LST 1582, AZERTY/phonetic)
	{ "lt:std",                     { "lt221" }      },
	// Latvian (standard, QWERTY/phonetic)
	{ "lv",                         { "lv" }         },
	// Latvian (QWERTY/UGJRMV/phonetic)
	{ "lv:ergonomic",               { "lv455" }      },
	// Macedonian (QWERTZ/national)
	{ "mk",                         { "mk" }         },
	// Mongolian (QWERTY/national)
	{ "mn",                         { "mn" }         },
	// Maltese (UK layout, QWERTY)
	{ "mt",                         { "mt" }         },
	// Maltese (US layout, QWERTY)
	{ "mt:us",                      { "mt103" }      },
	{ "mt:alt-us",                  { "mt103" }      },
	// Nigerian (QWERTY)
	{ "ng",                         { "ng" }         },
	{ "gh:hausa",                   { "ng" }         }, // Hausa
	// Dutch (QWERTY)
	{ "nl",                         { "nl" }         },
	// Norwegian (QWERTY/ASERTT)
	{ "no",                         { "no" }         },
	{ "no:smi",                     { "no", 30000 }  }, // Saami
	{ "no:smi_nodeadkeys",          { "no", 30000 }  },
	// Filipino (QWERTY)
	{ "ph",                         { "ph" }         },
	// Polish (programmers, QWERTY/phonetic)
        { "pl",                         { "pl" }         },
        { "pl:legacy",                  { "pl", 852 }    },
        { "pl:csb",                     { "pl", 58335 }  }, // Kashubian
        { "pl:szl",                     { "pl", 852 }    }, // Silesian
        { "gb:pl",                      { "pl" }         }, // British keyboard
	// Polish (typewriter, QWERTZ/phonetic)
	{ "pl:qwertz",                  { "pl214" }      },
	// Portuguese (QWERTY)
	{ "pt",	                        { "po" }         },
	// Romanian (standard, QWERTZ/phonetic)
	{ "ro",                         { "ro446" }      },
	// Romanian (QWERTY/phonetic)
	{ "ro:winkeys",                 { "ro" }         },
	{ "md:gag",                     { "ro", 30009 }  }, // Gaugaz (Latin)
	// Russian (standard, QWERTY/national)
	{ "ru",                         { "ru" }         },
	{ "us:ru",                      { "ru" }         },
	// Russian (typewriter, QWERTY/national)
	{ "ru:typewriter",              { "ru443" }      },
	{ "ru:typewriter-legacy",       { "ru443" }      },
	// Russian (extended standard, QWERTY/national)
	{ "ru:bak",                     { "rx", 30013 }  }, // Bashkirian
	{ "ru:chm",                     { "rx", 30014 }  }, // Mari
	{ "ru:cv",                      { "rx", 30013 }  }, // Chuvash
	{ "ru:cv_latin",                { "rx", 30013 }  },
	// TODO: Is 30017 or 30014 a better one for the Komi language?
	{ "ru:kom",                     { "rx", 30017 }  }, // Komi
	{ "ru:os_legacy",               { "rx", 30011 }  }, // Ossetian
	{ "ru:os_winkeys",              { "rx", 30011 }  },
	{ "ru:sah",                     { "rx", 30012 }  }, // Yakut
	{ "ru:udm",                     { "rx", 30014 }  }, // Udmurt
	{ "ru:xal",                     { "rx", 30011 }  }, // Kalmyk
	// Swiss (German, QWERTZ)
	{ "ch",                         { "sd" }         },
	// Swiss (French, QWERTZ)
	{ "ch:fr",                      { "sf" }         },
	{ "ch:fr_nodeadkeys",           { "sf" }         },
	{ "ch:sun_type6_f",             { "sf" }         },
	// Slovenian (QWERTZ)
	{ "si",                         { "si" }         },
	// Slovak (QWERTZ)
        { "sk",                         { "sk" }         },
        // Albanian (no deadkeys, QWERTY)
	{ "al:plisi",                   { "sq" }         }, // Plisi
        // Albanian (deadkeys, QWERTZ)
	{ "al",                         { "sq448" }      },
	// Swedish (QWERTY/ASERTT)
	{ "se",                         { "sv" }         },
	{ "se:smi",                     { "sv", 30000 }  }, // Saami
	// Tajik (QWERTY/national)
	{ "tj",                         { "tj" }         },
	// Turkmen (QWERTY/phonetic)
	{ "tm",                         { "tm" }         },
	// Turkish (QWERTY)
	{ "tr",                         { "tr" }         },
	{ "ua:crh",                     { "tr" }         }, // Crimean Tatar
	// Turkish (non-standard)
	{ "tr:f",                       { "tr440" }      },
	{ "tr:ku_f",                    { "tr440" }      },
	{ "ua:crh_f",                   { "tr440" }      }, // Crimean Tatar
	// Tatar (standard, QWERTY/national)
	{ "ru:tt",                      { "tt" }         },
	{ "ua:crh_alt",                 { "tt" }         }, // Crimean Tatar
	// Ukrainian (101-key, QWERTY/national)
	{ "ua",                         { "ua" }         },
	// Ukrainian (101-key, 1996, QWERTY/national)
	{ "ua:typewriter",              { "ur1996" }     },
	// Uzbek (QWERTY/national)
	{ "uz",                         { "uz" }         },
	// Vietnamese (QWERTY)
	{ "vn",                         { "vi" }         },
	// Serbian (deadkey, QWERTZ/national)
	{ "rs",                         { "yc" }         },
	// Serbian (no deadkey, QWERTZ/national)
	{ "rs:combiningkeys",           { "yc450" }      },
	// For some keyboard families we don't have code pages, but in the
	// corresponding states the QWERTY layout is typically used
	{ "brai",                       { "us" }         }, // Braille
	{ "bd",                         { "us" }         }, // Bangladesh
	{ "bt",                         { "us" }         }, // Buthan (Dzongkha)
	{ "cn",                         { "us" }         }, // China
	{ "et",                         { "us" }         }, // Ethiopia (Amharic)
	{ "gh",                         { "us" }         }, // Ghana
	{ "id",                         { "us" }         }, // Indonesia
	{ "in",                         { "us" }         }, // India
	{ "kh",                         { "us" }         }, // Khmer
	{ "kr",                         { "us" }         }, // Korea
	{ "jp",                         { "us" }         }, // Japan
	{ "la",                         { "us" }         }, // Laos
	{ "lk",                         { "us" }         }, // Sinhala
	{ "md",                         { "us" }         }, // Moldavia
	{ "mm",                         { "us" }         }, // Myanmar
	{ "mv",                         { "us" }         }, // Maldives (Dhivehi)
	{ "np",                         { "us" }         }, // Nepal
	{ "th",                         { "us" }         }, // Thailand
	{ "tw",                         { "us" }         }, // Taiwan
	// In some cases we do not have a matching QWERTY layout; if so, use
	// the US International keyboard with the best available code page
	{ "ba:us",                      { "ux", 437 }    }, // Bosnia
	{ "de:us",                      { "ux", 850 }    }, // Germany
	{ "de:qwerty",                  { "ux", 850 }    },
	{ "de:dsb",                     { "ux", 850 }    }, // Sorbian
	{ "fr:us",                      { "ux", 850 }    }, // France
	{ "hr:us",                      { "ux", 437 }    }, // Croatia
	{ "it:us",                      { "ux", 850 }    }, // Italy
	{ "me:cyrillicyz",              { "ux", 850 }    }, // Montenegro
	{ "me:latinunicodeyz",          { "ux", 850 }    },
	{ "me:latinyz",                 { "ux", 850 }    },
	{ "si:us",                      { "ux", 437 }    }, // Slovenia
        { "sk:qwerty",                  { "ux", 437 }    }, // Slovakia
        { "sk:qwerty_bksl",             { "ux", 437 }    },
	{ "tm:alt",                     { "ux", 437 }    }, // Turkmenistan
	{ "us:hbs",                     { "us", 437 }    }, // Serbo-Croatian
	{ "vn:us",                      { "ux", 850 }    }, // Vietnam
	// In some cases we do not have a matching QWERTZ layout; if so, use
	// the German keyboard with the best available code page
	{ "it:lldde",                   { "de", 850 }    }, // Ladin
	// For some keyboard families we don't have code pages, but in the
	// corresponding states the AZERTY layout is typically used
	{ "gn",                         { "fr", 437 }    }, // Gwinea, N'Ko
	// In some cases we do not have a matching AZERTY layout; if so, use
	// the French keyboard with the best available code page
	{ "vn:fr",                      { "fr", 850 }    }, // Vietnam

	// Certain DOS keyboard layouts are never detected by this mechanism,
	// due to various reason:
        // - 'bx', 'px' - layouts for African languages, not sure when they are
        //                really suitable and better than French, TODO
        // - 'jp' - requires code page 932 (a DBCS one, which are not supported)
        // - 'lt456' - no idea how to use it, TODO
        // - 'uk168' - no idea what is the difference from 'uk', TODO
        // - there seems to be no X11 keyboard layout matching the following
	//   DOS layouts: 'bg241', 'br274', 'bn', 'ce', 'ce443', 'cz', 'sx',
        //   'gk459', 'gk220', 'gr453', 'ix', 'ne', 'rx443', 'tt443', 'ur465',
        //   'ur2001', 'yu'
};
// clang-format on

// Mapping as above, but for certain 102-key keyboard layouts only
// clang-format off
static const std::map<std::string, KeyboardLayoutMaybeCodepage> X11ToDosKeyboard102 = {
	// Icelandic (102-key, QWERTY)
	{ "is",                         { "is161" }      },
        // Ukrainian (102-key, 2007, QWERTY/national)
	{ "ua",                         { "ur2007" }     },
};
// clang-format on

// Mapping from Linux console to DOS keyboard layout. Reference:
// - /usr/share/keymaps
// - 'localectl list-keymaps' command
// clang-format off
static const std::map<std::string, KeyboardLayoutMaybeCodepage> TtyToDosKeyboard = {
	// US (standard, QWERTY/national)
	{ "us",                                  { "us" }        },
	{ "us1",                                 { "us" }        },
	{ "carpalx",                             { "us" }        },
	{ "carpalx-full",                        { "us" }        },
	{ "emacs",                               { "us" }        },
	{ "emacs2",                              { "us" }        },
	{ "en",                                  { "us" }        },
	{ "jp106",                               { "us" }        },
	{ "pc110",                               { "us" }        },
	{ "atari-us",                            { "us" }        },
	{ "amiga-us",                            { "us" }        },
	{ "mac-us",                              { "us" }        },
	{ "sunkeymap",                           { "us" }        },
	// US (international, QWERTY)
	{ "us-acentos",                          { "ux" }        },
	{ "defkeymap",                           { "ux" }        },
	{ "defkeymap_V1.0",                      { "ux" }        },
	// US (Colemak)
	{ "en-latin9",                           { "co", 850 }   },
	{ "mod-dh-ansi-us",                      { "co" }        },
	{ "mod-dh-ansi-us-awing",                { "co" }        },
	{ "mod-dh-ansi-us-fatz",                 { "co" }        },
	{ "mod-dh-ansi-us-fatz-wid",             { "co" }        },
	{ "mod-dh-ansi-us-wide",                 { "co" }        },
	{ "mod-dh-iso-uk",                       { "co" }        },
	{ "mod-dh-iso-uk-wide",                  { "co" }        },
	{ "mod-dh-iso-us",                       { "co" }        },
	{ "mod-dh-iso-us-wide",                  { "co" }        },
	{ "mod-dh-matrix-us",                    { "co" }        },
	// US (Dvorak)
	{ "ANSI-dvorak",                         { "dv" }        },
	{ "dvorak",                              { "dv" }        },
	{ "dvorak-ca-fr",                        { "dv", 850 }   },
	{ "dvorak-de",                           { "dv", 850 }   },
	{ "dvorak-es",                           { "dv" }        },
	{ "dvorak-fr",                           { "dv", 850 }   },
	{ "dvorak-la",                           { "dv", 850 }   },
	{ "dvorak-no",                           { "dv" }        },
	{ "dvorak-programmer",                   { "dv" }        },
	{ "dvorak-ru",                           { "dv", 850 }   },
	{ "dvorak-sv-a1",                        { "dv", 850 }   },
	{ "dvorak-sv-a5",                        { "dv", 850 }   },
	{ "dvorak-uk",                           { "dv" }        },
	{ "dvorak-ukp",                          { "dv" }        },
	{ "mac-dvorak",                          { "dv" }        },
	{ "sundvorak",                           { "dv" }        },
	// US (left-hand Dvorak)
	{ "dvorak-l",                            { "lh" }        },	
	// US (right-hand Dvorak)
	{ "dvorak-r",                            { "rh" }        },
	// UK (standard, QWERTY)
	{ "uk",                                  { "uk" }        },
	{ "ie",                                  { "uk" }        },
	{ "atari-uk-falcon",                     { "uk" }        },	
	{ "mac-uk",                              { "uk" }        },
	{ "sunt5-uk",                            { "uk" }        },
	{ "sunt6-uk",                            { "uk" }        },
	// Arabic (QWERTY/Arabic)
	{ "fa",                                  { "ar470" }     },
	// Belgian (AZERTY)
	{ "be-latin1",                           { "be" }        },
	{ "mac-be",                              { "be" }        },
	// Bulgarian (QWERTY/national)
	{ "bg-cp1251",                           { "bg" }        },
	{ "bg-cp855",                            { "bg" }        },
	{ "bg_bds-cp1251",                       { "bg" }        },
	{ "bg_bds-utf8",                         { "bg" }        },
	// Bulgarian (QWERTY/phonetic)
	{ "bg_pho-cp1251",                       { "bg103" }     },
	{ "bg_pho-utf8",                         { "bg103" }     },
	// Brazilian (ABNT layout, QWERTY)
	{ "br-abnt",                             { "br" }        },
	{ "br-abnt2",                            { "br" }        },
	{ "br-latin1-abnt2",                     { "br" }        },
	// Brazilian (US layout, QWERTY)
	{ "br-latin1-us",                        { "br274" }     },
	// Belarusian (QWERTY/national)
	{ "by",                                  { "by" }        },
	{ "by-cp1251",                           { "by" }        },
	{ "bywin-cp1251",                        { "by" }        },
	// Canadian (standard, QWERTY)
	{ "ca",                                  { "cf"}         },
	{ "cf",                                  { "cf"}         },
	// Czech (standard, QWERTZ)
	{ "cz",                                  { "cz243" }     },
	{ "cz-us-qwertz",                        { "cz243" }     },
	// Czech (programmers, QWERTY)
	{ "cz-cp1250",                           { "cz489" }     },
	{ "cz-lat2",                             { "cz489" }     },
	{ "cz-lat2-prog",                        { "cz489" }     },
	{ "cz-qwerty",                           { "cz489" }     },
	{ "sunt5-cz-us",                         { "cz489" }     },
	{ "sunt5-us-cz",                         { "cz489" }     },
	// German (standard, QWERTZ)
	{ "de",                                  { "de" }        },
	{ "de_alt_UTF-8",                        { "de" }        },
	{ "de-latin1",                           { "de" }        },
	{ "de-latin1-nodeadkeys",                { "de" }        },
	{ "de-mobii",                            { "de" }        },	
	{ "atari-de",                            { "de" }        },
	{ "amiga-de",                            { "de" }        },
	{ "mac-de-latin1",                       { "de" }        },
	{ "mac-de-latin1-nodeadkeys",            { "de" }        },
	{ "sunt5-de-latin1",                     { "de" }        },
	// Neo German layouts
	{ "3l",                                  { "de" }        },
	{ "adnw",                                { "de" }        },
	{ "bone",                                { "de" }        },
	{ "koy",                                 { "de" }        },
	{ "neo",                                 { "de" }        },
	{ "neoqwertz",                           { "de" }        },
	// Danish (QWERTY)
	{ "dk",                                  { "dk" }        },
	{ "dk-latin1",                           { "dk" }        },
	{ "mac-dk-latin1",                       { "dk" }        },
	// Estonian (QWERTY)
	{ "et",                                  { "ee" }        },
	{ "et-nodeadkeys",                       { "ee" }        },
	// Spanish (QWERTY)
	{ "es",                                  { "es" }        },
	{ "es-cp850",                            { "es" }        },
	{ "es-olpc",                             { "es" }        },
	{ "mac-es",                              { "es" }        },
	{ "sunt4-es",                            { "es" }        },
	{ "sunt5-es",                            { "es" }        },
	// Finnish (QWERTY/ASERTT)
	{ "fi",                                  { "fi" }        },
	{ "mac-fi-latin1",                       { "fi" }        },
	{ "sunt4-fi-latin1",                     { "fi" }        },
	{ "sunt5-fi-latin1",                     { "fi" }        },
	// French (standard, AZERTY)
	{ "fr",                                  { "fr" }        },
	{ "fr-latin1",                           { "fr" }        },
	{ "fr-latin9",                           { "fr" }        },
	{ "fr-pc",                               { "fr" }        },
	{ "fr-bepo",                             { "fr" }        },
	{ "fr-bepo-latin9",                      { "fr" }        },
	{ "mac-fr",                              { "fr" }        },
	{ "mac-fr-legacy",                       { "fr" }        },
	{ "sunt5-fr-latin1",                     { "fr" }        },
	{ "azerty",                              { "fr" }        },
	{ "wangbe",                              { "fr" }        },
	{ "wangbe2",                             { "fr" }        },
	// Greek (319, QWERTY/national)
	{ "gr",                                  { "gk" }        },
	{ "gr-pc",                               { "gk" }        },
	// Croatian (QWERTZ/national)
	{ "croat",                               { "hr" }        },
	// Hungarian (101-key, QWERTY)
	{ "hu101",                               { "hu" }        },
	// Hungarian (102-key, QWERTZ)
	{ "hu",                                  { "hu208" }     },
	// Hebrew (QWERTY/national)
	{ "il",                                  { "il" }        },
	{ "il-heb",                              { "il" }        },
	{ "il-phonetic",                         { "il" }        },
	// Icelandic (101-key, QWERTY)
	{ "is-latin1",                           { "is" }        },
	{ "is-latin1-us",                        { "is" }        },
	// Italian (standard, QWERTY/national)
	{ "it",                                  { "it" }        },
	{ "it2",                                 { "it" }        },
	// Italian (142, QWERTY/national)
	{ "it-ibm",                              { "it142" }     },	
	{ "mac-it",                              { "it142" }     },
	// Kazakh (QWERTY/national)
	{ "kazakh",                              { "kk" }        },
	// Kyrgyz (QWERTY/national)
	{ "kyrgyz",                              { "ky" }        },
	{ "ky_alt_sh-UTF-8",                     { "ky" }        },
	// Latin American (QWERTY)
	{ "la-latin1",                           { "la" }        },
	// Lithuanian (Baltic, QWERTY/phonetic)
	{ "lt.baltic",                           { "lt" }        },
	// Lithuanian (programmers, QWERTY/phonetic)
	{ "lt",                                  { "lt210" }     },
	{ "lt.l4",                               { "lt210" }     },
	// Latvian (standard, QWERTY/phonetic)
	{ "lv",                                  { "lv" }        },
	{ "lv-tilde",                            { "lv" }        },
	// Macedonian (QWERTZ/national)
	{ "mk",                                  { "mk" }        },
	{ "mk-cp1251",                           { "mk" }        },
	{ "mk-utf",                              { "mk" }        },
	{ "mk0",                                 { "mk" }        },
	// Dutch (QWERTY)
	{ "nl",                                  { "nl" }        },
	{ "nl2",                                 { "nl" }        },
	// Norwegian (QWERTY/ASERTT)
	{ "no",                                  { "no" }        },
	{ "no-latin1",                           { "no" }        },
	{ "mac-no-latin1",                       { "no" }        },
	{ "sunt4-no-latin1",                     { "no" }        },
	// Polish (programmers, QWERTY/phonetic)
	{ "pl",                                  { "pl" }        },
	{ "pl1",                                 { "pl" }        },
	{ "pl2",                                 { "pl" }        },
	{ "pl3",                                 { "pl" }        },
	{ "pl4",                                 { "pl" }        },
	{ "mac-pl",                              { "pl" }        },
	{ "sun-pl",                              { "pl" }        },
	{ "sun-pl-altgraph",                     { "pl" }        },
	// Portuguese (QWERTY)
	{ "pt-latin1",                           { "po" }        },
	{ "pt-latin9",                           { "po" }        },
	{ "pt-olpc",                             { "po" }        },
	{ "mac-pt-latin1",                       { "po" }        },
	// Romanian (QWERTY/phonetic)
	{ "ro",                                  { "ro446" }     },
	{ "ro_std",                              { "ro446" }     },
	{ "ro_win",                              { "ro446" }     },
	// Russian (standard, QWERTY/national)
	{ "ru",                                  { "ru" }        },
	{ "ru-cp1251",                           { "ru" }        },
	{ "ru-ms",                               { "ru" }        },
	{ "ru-yawerty",                          { "ru" }        },
	{ "ru1",                                 { "ru" }        },
	{ "ru2",                                 { "ru" }        },
	{ "ru3",                                 { "ru" }        },
	{ "ru4",                                 { "ru" }        },
	{ "ru_win",                              { "ru" }        },
	{ "ruwin_alt-CP1251",                    { "ru" }        },
	{ "ruwin_alt-KOI8-R",                    { "ru" }        },
	{ "ruwin_alt-UTF-8",                     { "ru" }        },
	{ "ruwin_alt_sh-UTF-8",                  { "ru" }        },
	{ "ruwin_cplk-CP1251",                   { "ru" }        },
	{ "ruwin_cplk-KOI8-R",                   { "ru" }        },
	{ "ruwin_cplk-UTF-8",                    { "ru" }        },
	{ "ruwin_ct_sh-CP1251",                  { "ru" }        },
	{ "ruwin_ct_sh-KOI8-R",                  { "ru" }        },
	{ "ruwin_ct_sh-UTF-8",                   { "ru" }        },
	{ "ruwin_ctrl-CP1251",                   { "ru" }        },
	{ "ruwin_ctrl-KOI8-R",                   { "ru" }        },
	{ "ruwin_ctrl-UTF-8",                    { "ru" }        },
	{ "sunt5-ru",                            { "ru" }        },
	// Russian (extended standard, QWERTY/national)
	{ "bashkir",                             { "rx", 30013 } },
	// Swiss (German, QWERTZ)
	{ "sg",                                  { "sd" }        },
	{ "sg-latin1",                           { "sd" }        },
	{ "sg-latin1-lk450",                     { "sd" }        },
	{ "de_CH-latin1",                        { "sd" }        },
	{ "mac-de_CH",                           { "sd" }        },
	// Swiss (French, QWERTZ)
	{ "fr_CH",                               { "sf" }        },
	{ "fr_CH-latin1",                        { "sf" }        },
	{ "mac-fr_CH-latin1",                    { "sf" }        },
	// Slovenian (QWERTZ)
	{ "slovene",                             { "si" }        },
	// Slovak (QWERTZ)
	{ "sk-prog-qwertz",                      { "sk" }        },
	{ "sk-qwerty",                           { "sk" }        },
	{ "sk-qwertz",                           { "sk" }        },
	// Swedish (QWERTY/ASERTT)
	{ "se-fi-ir209",                         { "sv" }        },	
	{ "se-fi-lat6",                          { "sv" }        },	
	{ "se-ir209",                            { "sv" }        },
	{ "se-lat6",                             { "sv" }        },
	{ "sv-latin1",                           { "sv" }        },
	{ "apple-a1048-sv",                      { "sv" }        },
	{ "apple-a1243-sv",                      { "sv" }        },
	{ "apple-a1243-sv-fn-reverse",           { "sv" }        },
	{ "apple-internal-0x0253-sv",            { "sv" }        },
	{ "apple-internal-0x0253-sv-fn-reverse", { "sv" }        },
	{ "atari-se",                            { "sv" }        },
	{ "mac-se",                              { "sv" }        },
	// Tajik (QWERTY/national)
	{ "tj_alt-UTF8",                         { "tj" }        },
	// Turkish (QWERTY)
	{ "trq",                                 { "tr" }        },
	{ "tr_q-latin5",                         { "tr" }        },
	{ "tralt",                               { "tr" }        },
	// Turkish (non-standard)
	{ "trf",                                 { "tr440" }     },
	{ "trf-fgGIod",                          { "tr440" }     },
	{ "tr_f-latin5",                         { "tr440" }     },
	// Tatar (standard, QWERTY/national)
	{ "ttwin_alt-UTF-8",                     { "tt" }        },
	{ "ttwin_cplk-UTF-8",                    { "tt" }        },
	{ "ttwin_ct_sh-UTF-8",                   { "tt" }        },
	{ "ttwin_ctrl-UTF-8",                    { "tt" }        },
	// Ukrainian (101-key, QWERTY/national)
	{ "ua",                                  { "ua" }        },
	{ "ua-cp1251",                           { "ua" }        },
	{ "ua-utf",                              { "ua" }        },
	{ "ua-utf-ws",                           { "ua" }        },
	{ "ua-ws",                               { "ua" }        },
	// Serbian (deadkey, QWERTZ/national)
	{ "sr-latin",                            { "yc" }        },
	// In some cases we do not have a matching QWERTY layout; if so, use
	// the US International keyboard with the best available code page
        { "sk-prog-qwerty",                      { "ux", 437 }   }, // Slovakia
        { "sr-cy",                               { "us", 437 }   }, // Serbia
};
// clang-format on

// ***************************************************************************
// Generic helper routines
// ***************************************************************************

static std::vector<std::string> get_command_output(const std::string& command)
{
	const std::string PrefixNoLocale = "LC_ALL= LC_MESSAGES= LANG= LANGUAGE= ";
	const std::string SuffixNoErrors = " 2>/dev/null";

	const auto full_command = PrefixNoLocale + command + SuffixNoErrors;

	auto file_pointer = popen(full_command.c_str(), "r");
	if (!file_pointer) {
		return {};
	}

	std::vector<std::string> result = {};

	char* buffer       = nullptr;
	size_t buffer_size = {};
	while (getline(&buffer, &buffer_size, file_pointer) >= 0) {
		result.push_back(&buffer[0]);
                free(buffer);
                buffer = nullptr;
	}

	free(buffer);
	pclose(file_pointer);
	return result;
}

static std::string get_env_variable(const std::string& variable)
{
	const auto result = getenv(variable.c_str());
	return result ? result : "";
}

static std::pair<std::string, std::string> get_env_variable_from_list(
        const std::vector<std::string>& list)
{
	for (const auto& variable : list) {
		const auto value = get_env_variable(variable);
		if (!value.empty()) {
			return {variable, value};
		}
	}

	return {};
}

enum class XdgDesktopSession {
	Kde,
	Gnome,
	Wayfire, // used by Raspberry Pi OS
};

static bool is_xdg_desktop_session(const XdgDesktopSession session)
{
	// New mechanism to detect desktop environment, it seems it isn't
	// universally available yet (mid 2024)
	auto get_xdg_current_desktop = []() {
		auto variable = get_env_variable(XdgCurrentDesktop);
		upcase(variable);
		const auto tmp = split(variable, ":");
		return std::set<std::string>(tmp.begin(), tmp.end());
	};

	// Older mechanism to detect desktop environment
	auto get_xdg_session_desktop = []() {
		auto variable = get_env_variable(XdgSessionDesktop);
		upcase(variable);
		return variable;
	};

	static const auto xdg_current_desktop = get_xdg_current_desktop();
	static const auto xdg_session_desktop = get_xdg_session_desktop();

	auto check_desktop_string = [&](const std::string& desktop_string) {
		return xdg_current_desktop.contains(desktop_string) ||
		       xdg_session_desktop == desktop_string;
	};

	auto check_desktop_wayfire = [&]() {
		return xdg_session_desktop.find("WAYFIRE") != std::string::npos;
	};

	// KDE detection should be very reliable. GNOME might be worse - there
	// are many desktops being a GNOME fork, or just utilizing some GNOME
	// components, some not advertising themselves in the XDG environment
	// variables - but plain GNOME and Ubuntu desktop should work.
	static const bool is_ubuntu  = check_desktop_string("UBUNTU");
	static const bool is_kde     = check_desktop_string("KDE");
	static const bool is_gnome   = check_desktop_string("GNOME") ||
	                               (is_ubuntu && !is_kde);
        static const bool is_wayfire = check_desktop_wayfire();

	switch (session) {
	case XdgDesktopSession::Kde:     return is_kde;
	case XdgDesktopSession::Gnome:   return is_gnome;
	case XdgDesktopSession::Wayfire: return is_wayfire;
	default: assert(false); return false;
	}
}

// ***************************************************************************
// Detection routines
// ***************************************************************************

struct DesktopKeyboardLayoutEntry {
	std::string layout  = {};
	std::string variant = {};
};

struct DesktopKeyboardLayouts {
	std::string model                            = {};
	std::vector<DesktopKeyboardLayoutEntry> list = {};
};

static bool is_language_generic(const std::string& language)
{
	return iequals(language, "C") || iequals(language, "POSIX");
}

// Split locale string into language and territory, drop the rest
static std::pair<std::string, std::string> split_posix_locale(const std::string& value)
{
	std::string tmp = value; // language[_TERRITORY][.codeset][@modifier]

	tmp = tmp.substr(0, tmp.rfind('@')); // strip the modifier
	tmp = tmp.substr(0, tmp.rfind('.')); // strip the codeset

	std::pair<std::string, std::string> result = {};

	result.first = tmp.substr(0, tmp.find('_')); // get the language
	lowcase(result.first);

	const auto position = tmp.rfind('_');
	if (position != std::string::npos) {
		result.second = tmp.substr(position + 1); // get the territory
		upcase(result.second);
	}

	return result;
}

static std::optional<DosCountry> get_dos_country(const std::string& category,
                                                 std::string& log_info)
{
	const std::vector<std::string> Variables = {LcAll, category};

	const auto [variable, value] = get_env_variable_from_list(Variables);
	if (value.empty()) {
		return {};
	}
	log_info = variable + "=" + value;

	const auto [language, teritory] = split_posix_locale(value);
	if (is_language_generic(language)) {
		return {};
	}

	return IsoToDosCountry(language, teritory);
}

static std::string get_language_file(std::string& log_info)
{
	const std::vector<std::string> Variables = {
	        VariableLanguage,
	        LcAll,
	        LcMessages,
	        VariableLang,
	};

	const auto [variable, value] = get_env_variable_from_list(Variables);
	if (value.empty()) {
		return {};
	}
	log_info = variable + "=" + value;

	const auto [language, teritory] = split_posix_locale(value);

	if (language == "pt" && teritory == "BR") {
		return "br"; // we have a dedicated Brazilian translation
	}
	if (language == "c" || language == "posix") {
		return "en";
	}

	return language;
}

static DesktopKeyboardLayouts consolidate_layouts_variants(
        const std::string& model, const std::vector<std::string>& layouts,
        const std::vector<std::string>& variants)
{
	DesktopKeyboardLayouts result = {};

	result.model = model;

	if (layouts.size() == variants.size()) {
		for (size_t i = 0; i < layouts.size(); ++i) {
			DesktopKeyboardLayoutEntry result_entry = {};
			result_entry.layout                     = layouts[i];
			result_entry.variant                    = variants[i];
			result.list.push_back(result_entry);
		}
	} else {
		for (const auto& layout : layouts) {
			DesktopKeyboardLayoutEntry result_entry = {};
			result_entry.layout                     = layout;
			result.list.push_back(result_entry);
		}
	}

	return result;
}

static DesktopKeyboardLayouts get_keyboard_layouts_ini(
        const std_fs::path file_path, const std::string& section_start,
        const std::string& model_key, const std::string& layouts_key,
        const std::string& variants_key)
{
	std::string keyboard_model        = {};
	std::vector<std::string> layouts  = {};
	std::vector<std::string> variants = {};

	std::ifstream config_stream(file_path);
	std::string input_line = {};
	while (getline(config_stream, input_line)) {
		trim(input_line);
		if (input_line != section_start) {
			continue;
		}

		// Found appropriate file section
		while (getline(config_stream, input_line)) {
			trim(input_line);
			if (input_line.starts_with('[')) {
				break;
			}

			auto tokens = split(input_line, "=");
			if (tokens.size() < 2) {
				continue;
			}

			trim(tokens[0]);
			trim(tokens[1]);

			if (tokens[0] == model_key) {
				keyboard_model = tokens[1];
			} else if (tokens[0] == layouts_key) {
				layouts = split(tokens[1], ",");
			} else if (tokens[0] == variants_key) {
				variants = split(tokens[1], ",");
			}
		}
	}

	return consolidate_layouts_variants(keyboard_model, layouts, variants);
}

static DesktopKeyboardLayouts get_keyboard_layouts_kde()
{
	return get_keyboard_layouts_ini(get_xdg_config_home() / "kxkbrc",
	                                "[Layout]",
	                                "Model",
	                                "LayoutList",
	                                "VariantList");
}

static DesktopKeyboardLayouts get_keyboard_layouts_wayfire()
{
	auto config_file = get_env_variable(WayfireConfigFile);
	if (config_file.empty()) {
		// I have seen the Raspberry Pi OS sometimes do not populate
		// the config file location to the environment variable; in such
		// case use XDG to get the config file location.
		config_file = (get_xdg_config_home() / "wayfire.ini").string();
	}

	return get_keyboard_layouts_ini(get_env_variable(WayfireConfigFile),
	                                "[input]",
	                                "xkb_model",
	                                "xkb_layout",
	                                "xkb_variant");
}

static DesktopKeyboardLayouts get_keyboard_layouts_gnome()
{
	const std::string Command = "gsettings get org.gnome.desktop.input-sources ";

	DesktopKeyboardLayouts results = {};

	// Retrieve keyboard model
	auto model_output = get_command_output(Command + "xkb-model");
	if (model_output.size() == 1) {
		trim(model_output[0]);
		if (model_output[0].starts_with('\'') &&
		    model_output[0].ends_with('\'') && model_output[0].size() > 2) {
			results.model = model_output[0].substr(
			        1, model_output[0].size() - 2);
		}
	}

	// Retrieve keyboard layouts
	auto sources_output = get_command_output(Command + "sources");
	if (sources_output.size() != 1) {
		// The command should normally return just one line
		return {};
	}

	auto trim_strip_leading =
	        [](const std::string& input,
	           const char delimiter_leading) -> std::optional<std::string> {
		std::string tmp = input;
		trim(tmp);

		if (tmp.size() < 1 || !tmp.starts_with(delimiter_leading)) {
			return {};
		}

		return tmp.substr(1);
	};

	auto trim_strip_both =
	        [](const std::string& input,
	           const char delimiter_leading,
	           const char delimiter_trailing) -> std::optional<std::string> {
		std::string tmp = input;
		trim(tmp);

		if (tmp.size() < 2 || !tmp.starts_with(delimiter_leading) ||
		    !tmp.ends_with(delimiter_trailing)) {
			return {};
		}

		return tmp.substr(1, tmp.length() - 2);
	};

	// Example valid command output:
	// [('xkb', 'us'), ('xkb', 'us+alt-intl')]

	std::string buffer = sources_output[0];
	trim(buffer);

	// Specification means 'array of tuples containing two strings' - strip
	// it away if present
	const std::string specification = "@a(ss)";
	if (buffer.starts_with(specification)) {
		buffer = buffer.substr(specification.length());
		trim(buffer);
	}

	// Strip leading/trailing square brackets
	const auto stripped = trim_strip_both(buffer, '[', ']');
	if (!stripped) {
		return {};
	}

	for (auto& entry : split(*stripped, ")")) {
                trim(entry);

                // Strip possible leading comma
                if (entry.starts_with(',')) {
                        entry = entry.substr(1);
                        trim(entry);
                }

                // Strip leading '('; trailing ')' should be already stripped
		auto tmp = trim_strip_leading(entry, '(');
		if (!tmp) {
			return {};
		}

		// Split into type and layout
		auto tokens = split(*tmp, ",");
		if (tokens.size() != 2) {
			return {};
		}

		const auto type   = trim_strip_both(tokens[0], '\'', '\'');
		const auto layout = trim_strip_both(tokens[1], '\'', '\'');
		if (!type || type->empty() || !layout || layout->empty()) {
			return {};
		}

		if (*type != "xkb") {
			// We can only handle 'xkb' type keyboard layouts (these
			// are the well-known X11 layouts); if GNOME introduces
			// other types, we need to add support for these
			continue;
		}

		// Get layout and (possibly) variant
		tokens = split(*layout, "+");

		DesktopKeyboardLayoutEntry result_entry = {};
		result_entry.layout = tokens[0];
		if (tokens.size() == 2) {
			result_entry.variant = tokens[1];
		}
		results.list.push_back(result_entry);
	}

	return results;
}

#if defined(__linux__)
static std::vector<std::string> get_keyboard_layouts_tty()
{
	const std::string ConsoleConfigFile = "/etc/vconsole.conf";

	const std::string ConsoleKey1 = "KEYMAP";
	const std::string ConsoleKey2 = "KEYMAP_TOGGLE";

	const std::string LinuxKernalParamsFile = "/proc/cmdline";

	const std::string LinuxKernalKey1 = "vconsole.keymap=";
	const std::string LinuxKernalKey2 = "vconsole.keymap_toggle=";

	std::string result1 = {};
	std::string result2 = {};

	// First try to get values from the console configuration file
	std::ifstream console_params_stream(ConsoleConfigFile);
	std::string input_line = {};
	while (getline(console_params_stream, input_line)) {
		trim(input_line);
		auto tokens = split(input_line, "=");
		if (tokens.size() < 2) {
			continue;
		}

		trim(tokens[0]);
		trim(tokens[1]);

		if (tokens[0] == ConsoleKey1) {
			result1 = tokens[1];
		} else if (tokens[0] == ConsoleKey2) {
			result2 = tokens[1];
		}
	}

	// Check if Linux Kernel overrides the parameters
	std::ifstream kernel_params_stream(LinuxKernalParamsFile);
	if (getline(kernel_params_stream, input_line)) {
		for (const auto& parameter : split(input_line)) {
			if (parameter.starts_with(LinuxKernalKey1)) {
				result1 = parameter.substr(LinuxKernalKey1.length());
			} else if (parameter.starts_with(LinuxKernalKey2)) {
				result2 = parameter.substr(LinuxKernalKey2.length());
			}
		}
	}

	// Return the results
	if (result1.empty() && result2.empty()) {
		return {};
	} else if (result1.empty()) {
		return {result2};
	} else if (result2.empty()) {
		return {result1};
	} else {
		return {result1, result2};
	}
}
#endif // __linux__

static std::vector<KeyboardLayoutMaybeCodepage> get_layouts_maybe_codepages_desktop(
        std::string& log_info)
{
	std::string source_desktop = {};

	log_info = {};

	DesktopKeyboardLayouts results_desktop = {};

	// On Wayland there is no standard way to get the configured keyboard
	// layouts, therefore we need (possibly a little bit hacky) desktop
	// environment specific methods.
	// Universal X11 detection method is possible, but problematic; we would
	// either need to directly link against X11 libraries, or try to call
	// command line utilities, which might be installed or not, and which
	// can return dummy values on Wayland.

	if (is_xdg_desktop_session(XdgDesktopSession::Kde)) {
		results_desktop = get_keyboard_layouts_kde();
		if (!results_desktop.list.empty()) {
			source_desktop = SourceKde;
		}
	}
	if (results_desktop.list.empty() &&
            is_xdg_desktop_session(XdgDesktopSession::Gnome)) {
		results_desktop = get_keyboard_layouts_gnome();
		if (!results_desktop.list.empty()) {
			source_desktop = SourceGnome;
		}
	}
	if (results_desktop.list.empty() &&
            is_xdg_desktop_session(XdgDesktopSession::Wayfire)) {
		results_desktop = get_keyboard_layouts_wayfire();
		if (!results_desktop.list.empty()) {
			source_desktop = SourceWayfire;
		}
	}

	const bool is_keyboard_102 = KeyboardModels102.contains(results_desktop.model);

	// Map the detected keyboard layouts to the matching FreeDOS layouts
	std::vector<KeyboardLayoutMaybeCodepage> results = {};
	for (const auto& entry : results_desktop.list) {
		std::string key1 = {};
		std::string key2 = {};

		if (!entry.variant.empty()) {
			// First - try to match layout+variant
			key1 = entry.layout + ":" + entry.variant;
			// Second - try to match layout only
			key2 = entry.layout;
		} else {
			key1 = entry.layout;
		}

		if (!log_info.empty()) {
			log_info += " ";
		} else if (!results_desktop.model.empty()) {
			log_info += "[";
			log_info += results_desktop.model + "] ";
		}
		log_info += key1;

		if (is_keyboard_102 && X11ToDosKeyboard102.contains(key1)) {
			results.push_back(X11ToDosKeyboard102.at(key1));
			continue;
		}

		if (is_keyboard_102 && X11ToDosKeyboard102.contains(key2)) {
			results.push_back(X11ToDosKeyboard102.at(key2));
			continue;
		}

		if (X11ToDosKeyboard.contains(key1)) {
			results.push_back(X11ToDosKeyboard.at(key1));
			continue;
		}

		if (X11ToDosKeyboard.contains(key2)) {
			results.push_back(X11ToDosKeyboard.at(key2));
			continue;
		}
	}

	if (!log_info.empty()) {
		log_info = source_desktop + log_info;
	}
	return results;
}

#ifdef __linux__
static std::vector<KeyboardLayoutMaybeCodepage> get_layouts_maybe_codepages_tty(
        std::string& log_info)
{
	log_info = {};

	const auto results_tty = get_keyboard_layouts_tty();
	if (results_tty.empty()) {
		return {};
	}

	// Map the detected keyboard layouts to the matching FreeDOS layouts
	std::vector<KeyboardLayoutMaybeCodepage> results = {};
	for (const auto& entry : results_tty) {
		if (!log_info.empty()) {
			log_info += ";";
		}
		log_info += entry;

		if (TtyToDosKeyboard.contains(entry)) {
			results.push_back(TtyToDosKeyboard.at(entry));
			continue;
		}
	}

	if (!log_info.empty()) {
		log_info = SourceTty + log_info;
	}
	return results;
}
#endif // __linux__

static std::vector<KeyboardLayoutMaybeCodepage> get_layouts_maybe_codepages(std::string& log_info)
{
	// Try to get keyboard layouts from the desktop session
	const auto results_x11 = get_layouts_maybe_codepages_desktop(log_info);
	if (!results_x11.empty()) {
		return results_x11;
	}

#ifdef __linux__
	// Try to get keyboard layouts from the text console settings
	const auto results_tty = get_layouts_maybe_codepages_tty(log_info);
	if (!results_tty.empty()) {
		return results_tty;
	}
#endif // __linux__

	return {};
}

const HostLocale& GetHostLocale()
{
	static std::optional<HostLocale> locale = {};

	if (!locale) {
		locale = HostLocale();

		auto& log_info = locale->log_info;

		// There is no "LC_*" variable specifying a concrete country,
		// so we are using a telephone format - as MS-DOS locale is
		// telephone-code based
		locale->country   = get_dos_country(LcTelephone, log_info.country);
		locale->numeric   = get_dos_country(LcNumeric, log_info.numeric);
		locale->time_date = get_dos_country(LcTime, log_info.time_date);
		locale->currency  = get_dos_country(LcMonetary, log_info.currency);

		locale->keyboard_layout_list = get_layouts_maybe_codepages(
		        log_info.keyboard);
	}

	return *locale;
}

const HostLanguage& GetHostLanguage()
{
	static std::optional<HostLanguage> locale = {};

	if (!locale) {
		locale = HostLanguage();

		locale->language_file = get_language_file(locale->log_info);
	}

	return *locale;
}

#endif
#endif
