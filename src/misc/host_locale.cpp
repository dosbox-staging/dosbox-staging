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

#include "host_locale.h"

#include "checks.h"
#include "string_utils.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

CHECK_NARROWING();

// ***************************************************************************
// ISO country/territory conversion to DOS country code
// ***************************************************************************

// Mapping from the 'ISO 3166-1 alpha-2' norm to DOS country code. Also contains
// several historic countries and territories. Reference:
// - https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2
// clang-format off
static const std::map<std::string, DosCountry> IsoToDosCountryMap = {
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
	{ "UM",    DosCountry::UnitedStates   }, // United States Minor Outlying  Islands
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
	{ "PF",    DosCountry::France         }, // French Polynesia
	{ "PM",    DosCountry::France         }, // St. Pierre and Miquelon
	{ "TF",    DosCountry::France         }, // French Southern Territories
	{ "ES",    DosCountry::Spain          },
	{ "EA",    DosCountry::Spain          }, // Ceuta, Melilla
	{ "IC",    DosCountry::Spain          }, // Canary Islands
	{ "XA",    DosCountry::Spain          }, // Canary Islands, code used in Switzerland
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
	{ "MR",    DosCountry::Arabic         }, // Mauritania
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
// clang-format on

std::optional<DosCountry> IsoToDosCountry(const std::string& language,
                                          const std::string& territory)
{
	std::string key_language  = language;
	std::string key_territory = territory;

	lowcase(key_language);
	upcase(key_territory);

	const auto key = key_language + "_" + key_territory;

	if (IsoToDosCountryMap.contains(key)) {
		return IsoToDosCountryMap.at(key);
	}
	if (IsoToDosCountryMap.contains(key_territory)) {
		return IsoToDosCountryMap.at(key_territory);
	}

	return {};
}

// ***************************************************************************
// Generic locale fetch routines
// ***************************************************************************

StdLibLocale::StdLibLocale()
{
	std::locale locale = {};
	try {
		locale = std::locale("");
	} catch (...) {
		return; // unable to create locale, detection failed
	}

	// Detect numeric format
	const auto& std_numeric = std::use_facet<std::numpunct<char>>(locale);

	thousands_separator = std_numeric.thousands_sep();
	decimal_separator   = std_numeric.decimal_point();

	// Detect monetary format
	const auto& std_symbol = std::use_facet<std::moneypunct<char, true>>(locale);
	const auto& std_currency = std::use_facet<std::moneypunct<char>>(locale);

	currency_code = std_symbol.curr_symbol();
	currency_utf8 = std_currency.curr_symbol();
	trim(currency_code);
	trim(currency_utf8);

	const auto src_precision = std_currency.frac_digits();
	if (src_precision > 0 && src_precision <= UINT8_MAX) {
		currency_precision = static_cast<uint8_t>(src_precision);
	}

	// Detect time/date format - this can fail, C++ library is allowed
	// to just return 'std::time_base::no_order'
	const auto& std_time = std::use_facet<std::time_get<char>>(locale);

	switch (std_time.date_order()) {
	case std::time_base::dmy:
		date_format = DosDateFormat::DayMonthYear;
		break;
	case std::time_base::mdy:
		date_format = DosDateFormat::MonthDayYear;
		break;
	case std::time_base::ymd:
		date_format = DosDateFormat::YearMonthDay;
		break;
	case std::time_base::ydm: // DOS does not support this
	default:  // no order
                break;
	}

	// Remaining ones has to be detected by examining test conversions

	std::tm test_time_date = {};

	test_time_date.tm_isdst = 0;   // no DST in effect
	test_time_date.tm_year  = 111; // 2011 (years since 1900)
	test_time_date.tm_mon   = 11;  // 12th month (months since January)
	test_time_date.tm_mday  = 13;  // 13th day
	test_time_date.tm_hour  = 22;
	test_time_date.tm_min   = 14;
	test_time_date.tm_sec   = 15;

	const double TestMoney = 123.45;

	std::stringstream test_time_stream       = {};
	std::stringstream test_date_stream       = {};
	std::stringstream test_money_code_stream = {};
	std::stringstream test_money_utf8_stream = {};

	test_time_stream.imbue(locale);
	test_date_stream.imbue(locale);
	test_money_code_stream.imbue(locale);
	test_money_utf8_stream.imbue(locale);

	test_time_stream << std::put_time(&test_time_date, "%X");
	test_date_stream << std::put_time(&test_time_date, "%x");
	test_money_code_stream << std::showbase << std::put_money(TestMoney, true);
	test_money_utf8_stream << std::showbase << std::put_money(TestMoney, false);

	auto time_example       = test_time_stream.str();
	auto date_example       = test_date_stream.str();
	auto money_code_example = test_money_code_stream.str();
	auto money_utf8_example = test_money_utf8_stream.str();

	trim(time_example);
	trim(date_example);
	trim(money_code_example);
	trim(money_utf8_example);

	// Examine rendered strings for time format and separator

	const auto position_hours_24h = time_example.find("22");
	const auto position_hours_12h = time_example.find("10");
	const auto position_minutes   = time_example.find("14");

	if (position_hours_24h != std::string::npos &&
	    position_hours_12h == std::string::npos) {
		time_format = DosTimeFormat::Time24H;
		if (position_minutes != std::string::npos &&
		    position_minutes == position_hours_24h + 3) {
			time_separator = time_example[position_hours_24h + 2];
		}
	} else if (position_hours_24h == std::string::npos &&
	           position_hours_12h != std::string::npos) {
		time_format = DosTimeFormat::Time12H;
		if (position_minutes != std::string::npos &&
		    position_minutes == position_hours_12h + 3) {
			time_separator = time_example[position_hours_12h + 2];
		}
	}

	// Examine rendered strings for date format and separator

	const auto position_year  = date_example.find("11");
	const auto position_month = date_example.find("12");
	const auto position_day   = date_example.find("13");

	if (position_year != std::string::npos &&
            position_month != std::string::npos &&
	    position_day != std::string::npos) {

		if (position_day + 2 < position_month &&
		    position_month + 2 < position_year) {
			date_format = DosDateFormat::DayMonthYear;
		} else if (position_month + 2 < position_day &&
		           position_day + 2 < position_year) {
			date_format = DosDateFormat::MonthDayYear;
		} else if (position_year + 2 < position_month &&
		           position_month + 2 < position_day) {
			date_format = DosDateFormat::YearMonthDay;
		}

		if (position_day + 3 == position_month) {
			date_separator = date_example[position_day + 2];
		} else if (position_month + 3 == position_day) {
			date_separator = date_example[position_month + 2];
		}
	}

	// Examine rendered strings for currency format

	auto detect_format =
	        [](const std::string& example,
	           const std::string& currency) -> std::optional<DosCurrencyFormat> {
		if (example.empty() || currency.empty()) {
			return {};
		}

		if (example.starts_with(currency + " ")) {
			return DosCurrencyFormat::SymbolSpaceAmount;
		} else if (example.starts_with(currency)) {
			return DosCurrencyFormat::SymbolAmount;
		} else if (example.ends_with(std::string(" ") + currency)) {
			return DosCurrencyFormat::AmountSpaceSymbol;
		} else if (example.ends_with(currency)) {
			return DosCurrencyFormat::AmountSymbol;
		}

		return {};
	};

	currency_code_format = detect_format(money_code_example, currency_code);
	currency_utf8_format = detect_format(money_code_example, currency_utf8);
}
