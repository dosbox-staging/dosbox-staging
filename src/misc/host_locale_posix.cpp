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

#include "host_locale.h"

#include "checks.h"
#include "dosbox.h"
#include "string_utils.h"

#include <clocale>
#include <cstdio>
#include <string>

CHECK_NARROWING();

// ***************************************************************************
// Detection data
// ***************************************************************************

// Mapping based on data from https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2,
// also contains several historic states/teritories
static const std::map<std::string, DosCountry> PosixToDosCountry = {
	// List ordered by DOS country code, primary ISO country code first
	{ "AQ",    DosCountry::International  }, // Antarctica
	{ "EU",    DosCountry::International  }, // European Union
	{ "EZ",    DosCountry::International  }, // Eurozone
	{ "QO",    DosCountry::International  }, // Outlying Oceania
	{ "UN",    DosCountry::International  }, // United Nations
	{ "XX",    DosCountry::International  }, // unknown state
	{ "XZ",    DosCountry::International  }, // international waters
	{ "US",    DosCountry::UnitedStates   },
	{ "AS",    DosCountry::UnitedStates   }, // American Samoa
	{ "GU",    DosCountry::UnitedStates   }, // Guam
	{ "JT",    DosCountry::UnitedStates   }, // Johnston Island
	{ "MI",    DosCountry::UnitedStates   }, // Midway Islands
	{ "PU",    DosCountry::UnitedStates   }, // United States Miscellaneous Pacific Islands
	{ "QM",    DosCountry::UnitedStates   }, // used by ISRC
	{ "UM",    DosCountry::UnitedStates   }, // United States Minor Outlying Islands
	{ "VI",    DosCountry::UnitedStates   }, // Virgin Islands (US)
	{ "WK",    DosCountry::UnitedStates   }, // Wake Island
	{ "fr_CA", DosCountry::CanadaFrench   },
	{ "AG",    DosCountry::LatinAmerica   }, // Antigua and Barbuda	
	{ "AI",    DosCountry::LatinAmerica   }, // Anguilla
	{ "AW",    DosCountry::LatinAmerica   }, // Aruba
	{ "BB",    DosCountry::LatinAmerica   }, // Barbados
	{ "BM",    DosCountry::LatinAmerica   }, // Bermuda
	{ "BS",    DosCountry::LatinAmerica   }, // Bahamas
	{ "BZ",    DosCountry::LatinAmerica   }, // Belize
	{ "CU",    DosCountry::LatinAmerica   }, // Cuba
	{ "CW",    DosCountry::LatinAmerica   }, // Curaçao
	{ "DM",    DosCountry::LatinAmerica   }, // Dominica
	{ "DO",    DosCountry::LatinAmerica   }, // Dominican Republic
	{ "FK",    DosCountry::LatinAmerica   }, // Falkland Islands
	{ "FM",    DosCountry::LatinAmerica   }, // Micronesia
	{ "GD",    DosCountry::LatinAmerica   }, // Grenada
	{ "GP",    DosCountry::LatinAmerica   }, // Guadeloupe
	{ "GY",    DosCountry::LatinAmerica   }, // Guyana
	{ "HT",    DosCountry::LatinAmerica   }, // Haiti
	{ "JM",    DosCountry::LatinAmerica   }, // Jamaica
	{ "KN",    DosCountry::LatinAmerica   }, // St. Kitts and Nevis
	{ "KY",    DosCountry::LatinAmerica   }, // Cayman Islands
	{ "LC",    DosCountry::LatinAmerica   }, // St. Lucia
	{ "MS",    DosCountry::LatinAmerica   }, // Montserrat
	{ "PE",    DosCountry::LatinAmerica   }, // Peru
	{ "PG",    DosCountry::LatinAmerica   }, // Papua New Guinea
	{ "PR",    DosCountry::LatinAmerica   }, // Puerto Rico
	{ "SR",    DosCountry::LatinAmerica   }, // Suriname
	{ "TC",    DosCountry::LatinAmerica   }, // Turks and Caicos
	{ "TT",    DosCountry::LatinAmerica   }, // Trinidad and Tobago
	{ "VC",    DosCountry::LatinAmerica   }, // St. Vincent and Grenadines
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
	{ "GF",    DosCountry::France         }, // French Guiana
	{ "FQ",    DosCountry::France         }, // French Southern and Antarctic Territories
	{ "FX",    DosCountry::France         }, // France, Metropolitan
	{ "MF",    DosCountry::France         }, // Saint Martin (French part)
	{ "MQ",    DosCountry::France         }, // Martinique / French Antilles
	{ "NC",    DosCountry::France         }, // New Caledonia	
	{ "PF",	   DosCountry::France         }, // French Polynesia
	{ "PM",    DosCountry::France         }, // St. Pierre and Miquelon
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
	{ "TK",    DosCountry::NewZealand     }, // Tokealu
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
	{ "en_AE", DosCountry::AsiaEnglish    }, // United Arab Emirates (English)
	{ "en_AM", DosCountry::AsiaEnglish    }, // Armenia (English)
	{ "en_AZ", DosCountry::AsiaEnglish    }, // Azerbaijan (English)
	{ "en_BH", DosCountry::AsiaEnglish    }, // Bahrain (English)
	{ "en_BD", DosCountry::AsiaEnglish    }, // Bangladesh (English)
	{ "en_BN", DosCountry::AsiaEnglish    }, // Brunei (English)
	{ "en_BT", DosCountry::AsiaEnglish    }, // Bhutan (English)
	{ "en_BU", DosCountry::AsiaEnglish    }, // Burma (English)
	{ "en_CN", DosCountry::AsiaEnglish    }, // China (English)
	{ "en_CY", DosCountry::AsiaEnglish    }, // Cyprus (English)
	{ "en_GE", DosCountry::AsiaEnglish    }, // Georgia (English)
	{ "en_ID", DosCountry::AsiaEnglish    }, // Indonesia (English)
	{ "en_IL", DosCountry::AsiaEnglish    }, // Israel (English)
	{ "en_IN", DosCountry::AsiaEnglish    }, // India (English)
	{ "en_IR", DosCountry::AsiaEnglish    }, // Iran (English)
	{ "en_IQ", DosCountry::AsiaEnglish    }, // Iraq (English)
	{ "en_JO", DosCountry::AsiaEnglish    }, // Jordan (English)
	{ "en_JP", DosCountry::AsiaEnglish    }, // Japan (English)
	{ "en_KG", DosCountry::AsiaEnglish    }, // Kyrgyzstan (English)
	{ "en_KH", DosCountry::AsiaEnglish    }, // Cambodia (English)
	{ "en_KP", DosCountry::AsiaEnglish    }, // North Korea (English)
	{ "en_KR", DosCountry::AsiaEnglish    }, // South Korea (English)
	{ "en_KW", DosCountry::AsiaEnglish    }, // Kuwait (English)
	{ "en_KZ", DosCountry::AsiaEnglish    }, // Kazakhstan (English)
	{ "en_LA", DosCountry::AsiaEnglish    }, // Laos (English)
	{ "en_LB", DosCountry::AsiaEnglish    }, // Lebanon (English)
	{ "en_LK", DosCountry::AsiaEnglish    }, // Sri Lanka (English)
	{ "en_MM", DosCountry::AsiaEnglish    }, // Myanmar (English)
	{ "en_MN", DosCountry::AsiaEnglish    }, // Mongolia (English)
	{ "en_MO", DosCountry::AsiaEnglish    }, // Macao (English)
	{ "en_MV", DosCountry::AsiaEnglish    }, // Maldives (English)
	{ "en_MY", DosCountry::AsiaEnglish    }, // Malaysia (English)
	{ "en_NP", DosCountry::AsiaEnglish    }, // Nepal (English)
	{ "en_OM", DosCountry::AsiaEnglish    }, // Oman (English)
	{ "en_PH", DosCountry::AsiaEnglish    }, // Philippines (English)
	{ "en_PK", DosCountry::AsiaEnglish    }, // Pakistan (English)
	{ "en_PS", DosCountry::AsiaEnglish    }, // Palestine (English)
	{ "en_QA", DosCountry::AsiaEnglish    }, // Qatar (English)
	{ "en_RU", DosCountry::AsiaEnglish    }, // Russia (English)
	{ "en_SA", DosCountry::AsiaEnglish    }, // Saudi Arabia (English)
	{ "en_SG", DosCountry::AsiaEnglish    }, // Singapore (English)
	{ "en_SU", DosCountry::AsiaEnglish    }, // Soviet Union (English)
	{ "en_SY", DosCountry::AsiaEnglish    }, // Syria (English)
	{ "en_TH", DosCountry::AsiaEnglish    }, // Thailand (English)
	{ "en_TJ", DosCountry::AsiaEnglish    }, // Tajikistan (English)
	{ "en_TL", DosCountry::AsiaEnglish    }, // Timor-Leste (English)
	{ "en_TM", DosCountry::AsiaEnglish    }, // Turkmenistan (English)
	{ "en_TP", DosCountry::AsiaEnglish    }, // East Timor (English)
	{ "en_TR", DosCountry::AsiaEnglish    }, // Turkey (English)
	{ "en_TW", DosCountry::AsiaEnglish    }, // Taiwan (English)
	{ "en_UZ", DosCountry::AsiaEnglish    }, // Uzbekistan (English)
	{ "en_VD", DosCountry::AsiaEnglish    }, // North Vietnam (English)
	{ "en_VN", DosCountry::AsiaEnglish    }, // South Vietnam (English)
	{ "en_YD", DosCountry::AsiaEnglish    }, // South Yemen (English)
	{ "en_YE", DosCountry::AsiaEnglish    }, // Yemen (English)
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
// XXX keyboard layout mappings, check command 'localectl list-x11-keymap-variants <layout>'
// XXX also from /usr/share/X11/xkb/rules/evdev.lst
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
	// Czech (QWERTZ)
	{ "cz",                         { "cz" }         },
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
	// Hungarian (102-key, QWERTZ)
	{ "hu:101_qwertz_comma_dead",   { "hu208" }      },
	{ "hu:101_qwertz_comma_nodead", { "hu208" }      },
	{ "hu:101_qwertz_dot_dead",     { "hu208" }      },
	{ "hu:101_qwertz_dot_nodead",   { "hu208" }      },
	{ "hu:102_qwertz_comma_dead",   { "hu208" }      },
	{ "hu:102_qwertz_comma_nodead", { "hu208" }      },
	{ "hu:102_qwertz_dot_dead",     { "hu208" }      },
	{ "hu:102_qwertz_dot_nodead",   { "hu208" }      },
	// Armenian (QWERTY/national)
	{ "am",                         { "hy" }         },
	// Hebrew (QWERTY/national)
	{ "il",                         { "il" }         },
	// Icelandic (102-key, QWERTY)
	{ "is",                         { "is161" }      },
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

	// XXX better descriptions
	// Lack of suitable X11 layout:
	// - bg241, br274, bn, ce, ce443, sx, gk459, gk220, gr453, is, ix, ne, rx443, tt443, ur465, ur2001, ur2007
	// Not sure what's the difference: cz243
	// Not sure how to use it: lt456

	// XXX re-check these
	// px    - Portuguese (international)
	// uk168 - UK (Alternate), Irish (Alternate)
	// yu    - Yugoslavian

	// Other layouts which are never detected:  XXX re-check these
	// bx    - Belgian (international), not clear what's the difference from 'be'
	// jp    - Japan, requires code page 932 (DBCS, not supported)
	// ua / ur2007 - Ukrainian
	//           (101-key, 2001&2007 are 102-key), not clear what's the
	//           difference from 'ua'
};

// XXX describe, use it
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
	// Czech (QWERTZ)
	{ "cz",                                  { "cz" }        },
	{ "cz-us-qwertz",                        { "cz" }        },
	// Czech (programmers, QWERTY)
	{ "cz-cp1250",                           { "cz489" }     },
	{ "cz-lat2",                             { "cz489" }     },
	{ "cz-lat2-prog",                        { "cz489" }     },
	{ "cz-qwerty",                           { "cz489" }     },
	{ "sunt5-cz-us",                         { "cz489" }     },
	{ "sunt5-us-cz",                         { "cz489" }     },
	

	// German (standard, QWERTZ) // XXX de or gr453?
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
	// Icelandic (102-key, QWERTY)
	{ "is-latin1",                           { "is161" }     },	
	{ "is-latin1-us",                        { "is161" }     },
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





/* XXX /usr/share/keymaps  /  localectl list-keymaps



*/


// XXX also check /usr/share/keymaps for console keymaps





// ***************************************************************************
// Detection routines
// ***************************************************************************

static bool is_language_generic(const std::string language)
{
	return iequals(language, "C") || iequals(language, "POSIX");
}

// Split locale string into language and territory, drop the rest
static std::pair<std::string, std::string> split_posix_locale(const char *value)
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

static std::optional<DosCountry> get_dos_country(const int category,
                                                 std::string &log_info)
{
	const auto value_ptr = setlocale(category, "");
	if (!value_ptr) {
		return {};
	}

	log_info = value_ptr;
	const auto [language, teritory] = split_posix_locale(value_ptr);

	if (is_language_generic(language)) {
		return {};
	}

	const auto language_teritory = language + "_" + teritory;
	if (PosixToDosCountry.contains(language_teritory)) {
		return PosixToDosCountry.at(language_teritory);
	}
	if (PosixToDosCountry.contains(teritory)) {
		return PosixToDosCountry.at(teritory);
	}

	return {};
}

static std::string get_language_file(std::string &log_info)
{
	const auto value_ptr = setlocale(LC_MESSAGES, "");
	if (!value_ptr) {
		return {};
	}

	log_info = value_ptr;
	const auto [language, teritory] = split_posix_locale(value_ptr);

	if (language == "pt" && teritory == "BR") {
		return "br"; // we have a dedicated Brazilian translation
	}

	if (language == "c" || language == "posix") {
		return "en";
	}

	return language;
}

static std::pair<std::string, std::string> get_x11_keyboard()
{
	const std::string no_locale = "LC_ALL= LC_MESSAGES= LANG= LANGUAGE= ";

	auto try_using_command = [&](const std::string &command)
		-> std::pair<std::string, std::string> {

		const std::string full_command = no_locale + command;

		auto file_pointer = popen(full_command.c_str(), "r");
		if (file_pointer == nullptr) {
			return {};
		}

		std::string layout  = {};
		std::string variant = {};

		char buffer[128];
		while(fgets(buffer, sizeof(buffer), file_pointer))
		{
			lowcase(buffer);
			strreplace(buffer, ',', '\0');

			const auto tokens = split(buffer);
			size_t idx_key   = 0;
			size_t idx_value = 1;			
			if (tokens.size() >= 1 && tokens[0] == "x11") {
				++idx_key;
				++idx_value;
			}
			if (tokens.size() <= idx_value) {
				continue;
			}

			if (tokens[idx_key] == "layout:" &&
			    tokens[idx_value] != "(unset)") {
				layout = tokens[idx_value];
			} else if (tokens[idx_key] == "variant:") {
				variant = tokens[idx_value];
			}
		}

		pclose(file_pointer);
		return { layout, variant };
	};

	// XXX try to extract all the layouts, afterwards select a dual-script one
	// XXX retest, both commands

	const auto result1 = try_using_command("setxkbmap -query");
	if (!result1.first.empty()) {
		return result1;
	}

	const auto result2 = try_using_command("localectl");
	if (!result2.first.empty()) {
		return result2;
	}

	return {};
}

static KeyboardLayoutMaybeCodepage get_layout_maybe_codepage(std::string &log_info) // XXX should return vector
{
	const std::string source_x11 = "[X11] ";

	const auto [layout, variant] = get_x11_keyboard();

	if (!variant.empty()) {
		const auto key = layout + ":" + variant;
		log_info = source_x11 + key;

		if (X11ToDosKeyboard.contains(key)) {
			return X11ToDosKeyboard.at(key);
		}
	} else {
		log_info = source_x11 + layout;
	}

	if (X11ToDosKeyboard.contains(layout)) {
		return X11ToDosKeyboard.at(layout);
	}

	// XXX fallback: try to get keyboard layout from TTY
	return {};
}

HostLocale DetectHostLocale()
{
	HostLocale locale = {};
	auto &log_info = locale.log_info;

	locale.country   = get_dos_country(LC_ALL,      log_info.country);
	locale.numeric   = get_dos_country(LC_NUMERIC,  log_info.numeric);
	locale.time_date = get_dos_country(LC_TIME,     log_info.time_date);
	locale.currency  = get_dos_country(LC_MONETARY, log_info.currency);
	
	const auto result = get_layout_maybe_codepage(log_info.keyboard);
	if (!result.keyboard_layout.empty()) {
		locale.keyboard_layout_list.push_back(result);
	}

	return locale;
}

HostLanguage DetectHostLanguage()
{
	HostLanguage locale = {};

	const auto language_file = get_language_file(locale.log_info);
	if (!language_file.empty()) {
		locale.language_file = language_file;
	}

	return locale;
}

#endif
