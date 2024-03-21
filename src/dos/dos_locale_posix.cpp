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

#if !defined(WIN32)

#include "dos_locale.h"

#include "checks.h"
#include "dosbox.h"
#include "string_utils.h"

#include <clocale>
#include <string>

CHECK_NARROWING();

static bool is_language_generic(const std::string language)
{
	// XXX - should it go into the table?
	return iequals(language, "C") || iequals(language, "POSIX");
}

// Split locale string into language and territory, drop the rest
static std::pair<std::string, std::string> split_locale(const char *value)
{
	std::string tmp = value; // language[_TERRITORY][.codeset][@modifier]

	tmp = tmp.substr(0, tmp.rfind('@')); // strip the modifier
	tmp = tmp.substr(0, tmp.rfind('.')); // strip the codeset
	
	std::pair<std::string, std::string> result = {};

	result.first = tmp.substr(0, tmp.find('_')); // get the language
	lowcase(result.first);

	const auto position = tmp.rfind('_');
	if (position != std::string::npos) {
		result.second = tmp.substr(position + 1); // get the teritory
		upcase(result.second);
	}

	return result;
}

// Mapping based on data from https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2,
// also contains several historic states/teritories
static const std::map<std::string, DosCountry> PosixToDosCountry = {
	// XXX describe sorting order and search order
	{ "AQ",    DosCountry::International  }, // Antarctica
	{ "EU",    DosCountry::International  }, // European Union
	{ "EZ",    DosCountry::International  }, // Eurozone
	{ "QO",    DosCountry::International  }, // Outlying Oceania
	{ "UN",    DosCountry::International  }, // United Nations
	{ "XX",    DosCountry::International  }, // unknown state
	{ "XZ",    DosCountry::International  }, // international waters
	{ "US",    DosCountry::UnitedStates   },
	{ "GU",    DosCountry::UnitedStates   }, // Guam
	{ "JT",    DosCountry::UnitedStates   }, // Johnston Island
	{ "MI",    DosCountry::UnitedStates   }, // Midway Islands
	{ "PU",    DosCountry::UnitedStates   }, // United States Miscellaneous Pacific Islands
	{ "QM",    DosCountry::UnitedStates   }, // used by ISRC
	{ "UM",    DosCountry::UnitedStates   }, // United States Minor Outlying Islands
	{ "VI",    DosCountry::UnitedStates   }, // Virgin Islands (US)
	{ "WK",    DosCountry::UnitedStates   }, // Wake Island
	{ "fr_CA", DosCountry::CanadaFrench   },
	// XXX DosCountry::LatinAmerica
	{ "CA",    DosCountry::CanadaEnglish  },
	{ "RU",    DosCountry::Russia         },
	{ "SU",    DosCountry::Russia         }, // Soviet Union
	{ "EG",    DosCountry::Egypt          },
	{ "ZA",    DosCountry::SouthAfrica    },
	{ "GR",    DosCountry::Greece         },
	{ "NL",    DosCountry::Netherlands    },
	{ "AN",    DosCountry::Netherlands    }, // Netherlands Antilles
        { "BQ",    DosCountry::Netherlands    }, // Bonaire, Sint Eustatius and Saba
        { "SX",    DosCountry::Netherlands    }, // Sint Maarten (Dutch part)
	{ "BE",    DosCountry::Belgium        },
	{ "FR",    DosCountry::France         },
	{ "BL",    DosCountry::France         }, // Saint Barthélemy
	{ "CP",    DosCountry::France         }, // Clipperton Island
	{ "FQ",    DosCountry::France         }, // French Southern and Antarctic Territories
	{ "FX",    DosCountry::France         }, // France, Metropolitan
	{ "MF",    DosCountry::France         }, // Saint Martin (French part)
	{ "TF",    DosCountry::France         }, // French Southern Territories
	{ "ES",    DosCountry::Spain          },
	{ "EA",    DosCountry::Spain          }, // Ceuta, Melilla
	{ "IC",    DosCountry::Spain          }, // Canary Islands
	{ "XA",    DosCountry::Spain          }, // Canary Islands, used by Switzerland
	{ "HU",    DosCountry::Hungary        },
	{ "YU",    DosCountry::Yugoslavia     },
	{ "IT",    DosCountry::Italy          },
	{ "SM",    DosCountry::Italy          }, // San Marino
	{ "VA",    DosCountry::Italy          }, // Vatican City
	{ "RO",    DosCountry::Romania        },
	{ "CH",    DosCountry::Switzerland    },
	{ "CZ",    DosCountry::Czechia        },
	{ "CS",    DosCountry::Czechia        }, // Czechoslovakia
	{ "AT",    DosCountry::Austria        },
	{ "GB",    DosCountry::UnitedKingdom  },
	{ "UK",    DosCountry::UnitedKingdom  },
	{ "AC",    DosCountry::UnitedKingdom  }, // Ascension Island 
	{ "CQ",    DosCountry::UnitedKingdom  }, // Island of Sark
	{ "DG",    DosCountry::UnitedKingdom  }, // Diego Garcia
	{ "GG",    DosCountry::UnitedKingdom  }, // Guernsey
	{ "GS",    DosCountry::UnitedKingdom  }, // South Georgia and the South Sandwich Islands
	{ "IM",    DosCountry::UnitedKingdom  }, // Isle of Man
	{ "IO",    DosCountry::UnitedKingdom  }, // British Indian Ocean Territory
	{ "JE",    DosCountry::UnitedKingdom  }, // Jersey
	{ "SH",    DosCountry::UnitedKingdom  }, // Saint Helena
	{ "TA",    DosCountry::UnitedKingdom  }, // Tristan da Cunha
	{ "VG",    DosCountry::UnitedKingdom  }, // Virgin Islands (British)
	{ "XI",    DosCountry::UnitedKingdom  }, // Northern Ireland
	{ "DK",    DosCountry::Denmark        },
	{ "GL",    DosCountry::Denmark        }, // Greenland
	{ "SE",    DosCountry::Sweden         },
	{ "NO",    DosCountry::Norway         },
	{ "BV",    DosCountry::Norway         }, // Bouvet Island
	{ "NQ",    DosCountry::Norway         }, // Dronning Maud Land
	{ "SJ",    DosCountry::Norway         }, // Svalbard and Jan Mayen
	{ "PL",    DosCountry::Poland         },
	{ "DE",    DosCountry::Germany        },
	{ "DD",    DosCountry::Germany        }, // German Democratic Republic
	{ "MX",    DosCountry::Mexico         },
	{ "AR",    DosCountry::Argentina      },
	{ "BR",    DosCountry::Brazil         },
	{ "CL",    DosCountry::Chile          },
	{ "CO",    DosCountry::Colombia       },
	{ "VE",    DosCountry::Venezuela      },
	{ "MY",    DosCountry::Malaysia       },
	{ "AU",    DosCountry::Australia      },
	{ "CC",    DosCountry::Australia      }, // Cocos (Keeling) Islands
	{ "CX",    DosCountry::Australia      }, // Christmas Island
	{ "HM",    DosCountry::Australia      }, // Heard Island and McDonald Islands
	{ "NF",    DosCountry::Australia      }, // Norfolk Island
	{ "ID",    DosCountry::Indonesia      },
	{ "PH",    DosCountry::Philippines    },
	{ "NZ",    DosCountry::NewZealand     },
	{ "PN",    DosCountry::NewZealand     }, // Pitcairn
	{ "SG",    DosCountry::Singapore      },
	{ "TH",    DosCountry::Thailand       },
	{ "KZ",    DosCountry::Kazakhstan     },
	{ "JP",    DosCountry::Japan          },
	{ "KR",    DosCountry::SouthKorea     },
	{ "VN",    DosCountry::Vietnam        },
	{ "VD",    DosCountry::Vietnam        }, // North Vietnam
	{ "CN",    DosCountry::China          },
	{ "MO",    DosCountry::China          }, // Macao
	{ "TR",    DosCountry::Turkey         },
	{ "IN",    DosCountry::India          },
	{ "PK",    DosCountry::Pakistan       },
	// XXX and en_?? rules for AsiaEnglish detection, consider countrees:
	// - Armenia, Azerbaijan, Bahrain, Bangladesh, Bhutan,
	//   Brunei, Cambodia, China, Cyprus, East Timor, Georgia, India,
	//   Indonesia, Iran, Iraq, Israel, Japan, Jordan, Kazakhstan,
	//   North Korea, South Korea, Kuwait, Kyrgystan, Laos, Lebanon,
	//   Malaysia, Maldives, Mongolia, Myanmar, Nepal, Oman, Pakistan,
	//   Philippines, Quatar, Russia, Saudi Arabia, Singapore, Sri Lanka,
	//   Syria, Tajikistan, Thailand, Turkey, Turkmenistan, United Arab Emirates,
	//   Uzbekistan, Vietnam, Yemen, Palestine, Taiwan, Cyprus
	{ "BD",    DosCountry::AsiaEnglish    }, // Bangladesh
	{ "BT",    DosCountry::AsiaEnglish    }, // Bhutan
	{ "BU",    DosCountry::AsiaEnglish    }, // Burma
	{ "KH",    DosCountry::AsiaEnglish    }, // Cambodia
	{ "LA",    DosCountry::AsiaEnglish    }, // Laos
	{ "LK",    DosCountry::AsiaEnglish    }, // Sri Lanka
	{ "MM",    DosCountry::AsiaEnglish    }, // Myanmar
	{ "MV",    DosCountry::AsiaEnglish    }, // Maldives
	{ "NP",    DosCountry::AsiaEnglish    }, // Nepal
	{ "MA",    DosCountry::Morocco        },
	{ "DZ",    DosCountry::Algeria        },
	{ "TN",    DosCountry::Tunisia        },
	{ "NE",    DosCountry::Niger          },
	{ "BJ",    DosCountry::Benin          },
	{ "DY",    DosCountry::Benin          }, // Dahomey
	{ "NG",    DosCountry::Nigeria        },
	{ "FO",    DosCountry::FaroeIslands   },
	{ "PT",    DosCountry::Portugal       },
	{ "LU",    DosCountry::Luxembourg     },
	{ "IE",    DosCountry::Ireland        },
	{ "IS",    DosCountry::Iceland        },
	{ "AL",    DosCountry::Albania        },
	{ "MT",    DosCountry::Malta          },
	{ "FI",    DosCountry::Finland        },
	{ "AX",    DosCountry::Finland        }, // Åland Islands
	{ "BG",    DosCountry::Bulgaria       },
	{ "LT",    DosCountry::Lithuania      },
	{ "LV",    DosCountry::Latvia         },
	{ "EE",    DosCountry::Estonia        },
	{ "AM",    DosCountry::Armenia        },
	{ "BY",    DosCountry::Belarus        },
	{ "UA",    DosCountry::Ukraine        },
	{ "RS",    DosCountry::Serbia         },
	{ "ME",    DosCountry::Montenegro     },
	{ "SI",    DosCountry::Slovenia       },
	{ "BA",    DosCountry::BosniaLatin    },
	// TODO: Find a way to detect DosCountry::BosniaCyrillic
	{ "MK",    DosCountry::NorthMacedonia },
	{ "SK",    DosCountry::Slovakia       },
        { "GT",    DosCountry::Guatemala      },
        { "SV",    DosCountry::ElSalvador     },
        { "HN",    DosCountry::Honduras       },
        { "NI",    DosCountry::Nicaragua      },
        { "CR",    DosCountry::CostaRica      },
        { "PA",    DosCountry::Panama         },
        { "PZ",    DosCountry::Panama         }, // Panama Canal Zone
        { "BO",    DosCountry::Bolivia        },
	{ "EC",    DosCountry::Ecuador        },
        { "PY",    DosCountry::Paraguay       },
        { "UY",    DosCountry::Uruguay        },
	{ "AF",    DosCountry::Arabic         }, // Afghanistan
	{ "DJ",    DosCountry::Arabic         }, // Djibouti
	{ "EH",    DosCountry::Arabic         }, // Western Sahara
	{ "IR",    DosCountry::Arabic         }, // Iran
	{ "IQ",    DosCountry::Arabic         }, // Iraq
	{ "LY",    DosCountry::Arabic         }, // Libya
	{ "MR",    DosCountry::Arabic         }, // Maruitania
	{ "NT",    DosCountry::Arabic         }, // Neutral Zone
	{ "PS",    DosCountry::Arabic         }, // Palestine 
	{ "SD",    DosCountry::Arabic         }, // Sudan 
	{ "SO",    DosCountry::Arabic         }, // Somalia 
	{ "TD",    DosCountry::Arabic         }, // Chad
	{ "YD",    DosCountry::Arabic         }, // South Yemen 
	{ "HK",    DosCountry::HongKong       },
	{ "TW",    DosCountry::Taiwan         },
	{ "LB",    DosCountry::Lebanon        },
	{ "JO",    DosCountry::Jordan         },
        { "SY",    DosCountry::Syria          },
        { "KW",    DosCountry::Kuwait         },
        { "SA",    DosCountry::SaudiArabia    },
        { "YE",    DosCountry::Yemen          },
        { "OM",    DosCountry::Oman           },
        { "AE",    DosCountry::Emirates       },
        { "IL",    DosCountry::Israel         },
        { "BH",    DosCountry::Bahrain        },
        { "QA",    DosCountry::Qatar          },
        { "MN",    DosCountry::Mongolia       },
        { "TJ",    DosCountry::Tajikistan     },
        { "TM",    DosCountry::Turkmenistan   },
        { "AZ",    DosCountry::Azerbaijan     },
        { "GE",    DosCountry::Georgia        },
        { "KG",    DosCountry::Kyrgyzstan     },
        { "UZ",    DosCountry::Uzbekistan     },
};

// keyboard layouts map XXX
static const std::map<std::string, DosCountry> X11ToDosKeybaord = {
	{ "az",                     "az"    }, // Azerbaijani
	{ "br",                     "br"    }, // Portuguese (Brazil, ABNT layout)
	{ "ch",                     "sd"    }, // Swiss (German), alternatives: sg	
	{ "ch:fr",                  "sf"    }, // Swiss (French)
	{ "ch:fr_nodeadkeys",       "sf"    },
	{ "ch:sun_type6_f",         "sf"    },
	{ "dk",                     "dk"    }, // Danish
	{ "gb:intl",                "kx"    }, // UK (International)	
	{ "gb:mac_intl",            "kx"    },
	{ "il",                     "il"    }, // Hebrew	
	{ "no",                     "no"    }, // Norwegian
	{ "si",                     "si"    }, // Slovenian
	{ "si:us",                  "us"    }, // (NOT similar to FreeDOS 'si')
        { "pl",                     "pl"    }, // Polish (Programmers)
	{ "pl:qwertz",              "pl214" }, // Polish (Typewriter)
	{ "tj",                     "tj"    }, // Tajik
	{ "tm",                     "tm"    }, // Turkmen
	{ "tm:alt",                 "us"    }, // (NOT similar to FreeDOS 'tm')	
	{ "us",                     "us"    }, // US (Standard)
	{ "us:colemak",             "co"    }, // US (Colemak)
	{ "us:colemak_dh",          "co"    },
	{ "us:colemak_dh_iso",      "co"    },
	{ "us:colemak_dh_ortho",    "co"    },
	{ "us:colemak_dh_wide",     "co"    },
	{ "us:colemak_dh_wide_iso", "co"    },
	{ "us:dvorak",              "dv"    }, // US (Dvorak)
	{ "us:dvorak-alt-intl",     "dv"    },
	{ "us:dvorak-classic",      "dv"    },
	{ "us:dvorak-intl",         "dv"    },
	{ "us:dvorak-mac",          "dv"    },
	{ "us:dvorak-l",            "lh"    }, // US (Left-Hand Dvorak)
	{ "us:dvorak-r",            "rh"    }, // US (Right-Hand Dvorak)
	{ "us:intl",                "ux"    }, // US (International)
	{ "us:alt-intl",            "ux"    },
	{ "us:altgr-intl",          "ux"    },
};

// There seems to be no X11 keyboard layout similar to the following DOS layouts:
// ph     // Filipino
// br274  // Portuguese (Brazil, US layout)





/* XXX keyboard layout mappings, check command 'localectl list-x11-keymap-variants':

X11 layouts -> DOS layouts

af   // Dari
al   // Albanian
am   // Armenian
ara  // Arabic
at   // German (Austria)
au   // English (Australia)
ba   // Bosnian
bd   // Bangla
be   // Belgian
bg   // Bulgarian
brai // Braille
bt   // Dzongkha
bw   // Tswana
by   // Belarusian
ca   // French (Canada)
cd   // French (Democratic Republic of the Congo)
cm   // English (Cameroon)
cn   // Chinese
cz   // Czech
de   // German
dz   // Berber (Algeria)
ee   // Estonian
eg   // Arabic (Egypt)
epo  // Esperanto
es   // Spanish
et   // Amharic
fi   // Finnish
fo   // Faroese
fr   // French
gb   // English (UK)
ge   // Georgian
gh   // English (Ghana)
gn   // N'Ko (AZERTY)
gr   // Greek
hr   // Croatian
hu   // Hungarian
id   // Indonesian (Latin)
ie   // Irish
in   // Indian
iq   // Arabic (Iraq)
ir   // Persian
is   // Icelandic
it   // Italian
jp   // Japanese
ke   // Kenya
kg   // Kyrgyz
kh   // Khmer (Cambodia)
kr   // Korean
kz   // Kazakh
la   // Lao
latam // Spanish (Latin American)
lk   // Sinhala (phonetic)
lt   // Lithuanian
lv   // Latvian
ma   // Morocco
md   // Moldavian
me   // Montenegrin
mk   // Macedonian
ml   // Bambara
mm   // Burmese
mn   // Mongolian
mt   // Maltese
mv   // Dhivehi
my   // Malay
ng   // Nigeria
nl   // Dutch
np   // Nepali
nz   // English (New Zealand)
pk   // Urdu
pt   // Portuguese
ro   // Romanian
rs   // Serbian
ru   // Russian
se   // Swedish
sk   // Slovak
sn   // Wolof
sy   // Arabic (Syria)
tg   // French (Togo)
th   // Thai
tr   // Turkish
tw   // Taiwanese
tz   // Swahili (Tanzania)
ua   // Ukrainian
uz   // Uzbek
vn   // Vietnamese
za   // English (South Africa)

DOS layouts:

ar462         // Algeria, Morocco, Tunisia
ar470         // Algeria, Bahrain, Egypt, Jordan, Kuwait, Lebanon, Morocco, Oman, Qatar, Saudi Arabia, Syria, Tunisia, UAE, Yemen
ba            // Bosnia & Herzegovina
be            // Belgium
bg            // Bulgaria (101-key)
bg103         // Bulgaria (101 phonetic)
bg241         // Bulgaria (102-key)
bl            // Belarus
bn            // Benin
bx            // Belgium (international)
by            // Belarus
ca            // Canada (Standard)
ce            // Russia (Chechnya Standard)
ce443         // Russia (Chechnya Typewriter)
cf            // Canada (Standard)
cf445         // Canada (Dual-layer)
cg            // Montenegro
cz            // Czech Republic (QWERTY) 
cz243         // Czech Republic (Standard)
cz489         // Czech Republic (Programmers)
de            // Austria (Standard), Germany (Standard)
ee            // Estonia
el            // Greece (319)
es            // Spain
et            // Estonia
fi            // Finland
fo            // Faeroe Islands
fr            // France (Standard)
fx            // France (international)
gk            // Greece (319)
gk220         // Greece (220)
gk459         // Greece (101-key)
gr            // Austria (Standard), Germany (Standard)
gr453         // Austria (Dual-layer), Germany (Dual-layer)
hr            // Croatia
hu            // Hungary (101-key)
hu208         // Hungary (102-key)
hy            // Armenia
is            // Iceland (101-key)
is161         // Iceland (102-key)
it            // Italy (Standard)
it142         // Italy (Comma on Numeric Pad)
ix            // Italy (international)
jp            // Japan
ka            // Georgia
kk            // Kazakhstan
kk476         // Kazakhstan
ky            // Kyrgyzstan
la            // Argentina, Chile, Colombia, Ecuador, Latin America, Mexico, Venezuela
lt            // Lithuania (Baltic)
lt210         // Lithuania (101-key, prog.)
lt211         // Lithuania (Azerty) 
lt221         // Lithuania (Standard, LST 1582)
lt456         // Lithuania (Dual-layout)
lv            // Latvia (Standard)
lv455         // Latvia (Dual-layout)
mk            // Macedonia
ml            // Malta (UK-based)
mn            // Mongolia
mo            // Mongolia
mt            // Malta (UK-based)
mt103         // Malta (US-based) 
ne            // Niger
ng            // Nigeria
nl            // Netherlands (102-key)
po            // Portugal
px            // Portugal (international)
ro            // Romania (Standard)
ro446         // Romania (QWERTY)
ru            // Russia (Standard)
ru443         // Russia (Typewriter)
rx            // Russia (Extended Standard)
rx443         // Russia (Extended Typewriter)
sk            // Slovakia
sp            // Spain
sq            // Albania (No deadkeys)
sq448         // Albania (Deadkeys)
sr            // Serbia (Deadkey)
su            // Finland
sv            // Sweden
sx            // Spain (international)
tr            // Turkey (QWERTY)
tr440         // Turkey (Non-standard)
tt            // Russia (Tatarstan Standard)
tt443         // Russia (Tatarstan Typewriter)
ua            // Ukraine (101-key)
uk            // Ireland (Standard), UK (Standard)
uk168         // Ireland (Alternate), UK (Alternate)
ur            // Ukraine (101-key)
ur1996        // Ukraine (101-key, 1996)
ur2001        // Ukraine (102-key, 2001)
ur2007        // Ukraine (102-key, 2007)
ur465         // Ukraine (101-key, 465)
uz            // Uzbekistan
vi            // Vietnam
yc            // Serbia (Deadkey)
yc450         // Serbia (No deadkey)
yu            // Bosnia & Herzegovina, Croatia, Montenegro, Slovenia

*/



static DosCountry get_dos_country(const int category, const DosCountry fallback)
{
	const auto value_ptr = setlocale(category, "");
	if (!value_ptr) {
		return fallback;
	}

	const auto [language, teritory] = split_locale(value_ptr);

	if (is_language_generic(language)) {
		return DosCountry::International;
	}




/* XXX

*/






	return fallback; // XXX
}

HostLocale DOS_DetectHostLocale()
{
	HostLocale locale = {};

	locale.country   = get_dos_country(LC_ALL,      locale.country);
	locale.numeric   = get_dos_country(LC_NUMERIC,  locale.country);
	locale.time_date = get_dos_country(LC_TIME,     locale.country);
	locale.currency  = get_dos_country(LC_MONETARY, locale.country);

	// XXX locale.currency



	// XXX detect the following:
	// XXX locale.messages_language // XXX LC_MESSAGES
	// XXX locale.keyboard_layout // XXX use popen with 'setxkbmap -query'

	return locale;
}

#endif
