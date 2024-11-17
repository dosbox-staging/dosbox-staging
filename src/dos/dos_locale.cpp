/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "dos_locale.h"

#include <cstring>
#include <map>
#include <vector>

#if defined(WIN32)
#include <windows.h>
#endif

#include "bitops.h"
#include "checks.h"
#include "dos_inc.h"
#include "dos_keyboard_layout.h"
#include "logging.h"
#include "string_utils.h"
#include "unicode.h"

CHECK_NARROWING();

// ***************************************************************************
// Handling DOS country info structure
// ***************************************************************************

static struct {
	LocalePeriod locale_period = LocalePeriod::Modern;

	DosCountry country        = DosCountry::UnitedStates;
	uint16_t country_dos_code = enum_val(country);

	// If the config file settings were read
	bool is_config_loaded = false;

	// If the locale has been generated to DOS table
	bool is_locale_generated = false;

	// If country in the configuration was set to 'auto'
	bool auto_detect_country = false;
} config;

// Offsets to data in DOS country info structure
constexpr size_t InfoOffsetDateFormat         = 0x00;
constexpr size_t InfoOffsetCurrencySymbol     = 0x02;
constexpr size_t InfoOffsetThousandsSeparator = 0x07;
constexpr size_t InfoOffsetDecimalSeparator   = 0x09;
constexpr size_t InfoOffsetDateSeparator      = 0x0b;
constexpr size_t InfoOffsetTimeSeparator      = 0x0d;
constexpr size_t InfoOffsetCurrencyFormat     = 0x0f;
constexpr size_t InfoOffsetCurrencyPrecision  = 0x10;
constexpr size_t InfoOffsetTimeFormat         = 0x11;
// TODO: support constexpr size_t InfoOffsetCasemap = 0x12;
//       move the implementation here from dos_tables.cpp
constexpr size_t InfoOffsetListSeparator = 0x16;
constexpr size_t InfoOffsetReserved      = 0x18;

constexpr size_t MaxCurrencySymbolLength = 4;
constexpr size_t ReservedAreaSize        = 10;

static DosCountry correct_country(const DosCountry country)
{
	// Correct country code to handle duplicates in DOS country numbers
	if (LocaleData::CodeToCountryCorrectionMap.count(enum_val(country))) {
		return LocaleData::CodeToCountryCorrectionMap.at(enum_val(country));
	} else {
		return country;
	}
}

static std::string get_country_name(const DosCountry country)
{
	const auto country_corrected = correct_country(country);

	if (LocaleData::CountryInfo.count(country_corrected)) {
		return LocaleData::CountryInfo.at(country_corrected).country_name;
	}

	return "<unknown country id>";
}

std::string DOS_GenerateListCountriesMessage()
{
	std::string message = "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_COUNTRIES_1");
	message += "\n\n";

	for (auto it = LocaleData::CountryInfo.begin();
	     it != LocaleData::CountryInfo.end(); ++it) {
		message += format_str("  %5d - %s\n",
		                         enum_val(it->first),
		                         MSG_GetRaw(it->second.GetMsgName().c_str()));
	}

	message += "\n";
	message += MSG_GetRaw("DOSBOX_HELP_LIST_COUNTRIES_2");
	message += "\n";

	return message;
}

static void set_country_dos_code()
{
	if (config.country == DosCountry::International) {
		// MS-DOS uses the same country code for International English
		// and Australia - we don't, as we have different settings for
		// these. Let's imitate MS-DOS behavior.
		config.country_dos_code = enum_val(DosCountry::Australia);
	} else {
		config.country_dos_code = enum_val(config.country);
	}
}

static void auto_detect_country()
{
	const auto layout = DOS_GetLoadedLayout();
	if (!layout) {
		return;
	}

	uint16_t country_id = {};
	if (!DOS_GetCountryFromLayout(layout, country_id)) {
		LOG_WARNING("DOS: The keyboard layout's country code '%s' "
		            "does not have a corresponding country",
		            layout);
		return;
	}

	config.country = static_cast<DosCountry>(country_id);
}

static void maybe_log_changed_country(const std::string &country_name,
                                      const LocalePeriod actual_period)
{
	static bool already_logged = false;

	static DosCountry logged_country         = {};
	static LocalePeriod logged_real_period   = {};
	static LocalePeriod logged_config_period = {};

	if ((logged_country == config.country) && already_logged &&
	    (logged_real_period == actual_period) &&
	    (logged_config_period == config.locale_period)) {
		return;
	}

	auto period_to_string = [](const LocalePeriod period) {
		if (period == LocalePeriod::Modern) {
			return "modern";
		} else {
			return "historic";
		}
	};

	std::string additional_comment = {};
	if (actual_period != config.locale_period) {
		additional_comment = " (";
		additional_comment += period_to_string(config.locale_period);
		additional_comment += " locale not known)";
	}

	LOG_MSG("DOS: Loaded %s locale for country %d, '%s'%s",
	        period_to_string(actual_period),
	        enum_val(config.country),
	        country_name.c_str(),
	        additional_comment.c_str());

	already_logged       = true;
	logged_country       = config.country;
	logged_real_period   = actual_period;
	logged_config_period = config.locale_period;
}

static LocaleSeparator get_default_list_separator(const LocaleSeparator thousands_separator,
                                                  const LocaleSeparator decimal_separator)
{
	// Current locale systems (like Unicode CLDR) do not seem to specify
        // list separators anymore.
	// On 'https://answers.microsoft.com' one can find a question:
	// "Why does Excel seem to use ; and , differently per localization?"
	// And the top answer is:
	// "In countries such as the USA and UK, the comma is used as list
        // separator. (...) In countries that use comma as decimal separator,
        // such as many continental European countries, using a comma both as
	// decimal separator and as list separator would be very
	// confusing: does 3,5 mean the numbers 3 and 5, or does it mean
	// 3 and 5/10? So, in such countries, the semi-colon ; is use
	// as a list separator".
	// - https://answers.microsoft.com/en-us/msoffice/forum/all/why-does-excel-seem-to-use-and-differently-per/6467032f-43a0-4343-bae7-af8853fec754
	//
	// Let's use this algorithm to determine default list separator.

	if (thousands_separator != LocaleSeparator::Comma &&
	    decimal_separator != LocaleSeparator::Comma) {
		return LocaleSeparator::Comma;
	} else {
		return LocaleSeparator::Semicolon;
	}
}

static void refresh_time_date_format(const LocaleInfoEntry &source)
{
	const auto& destination = dos.tables.country;

	size_t offset = InfoOffsetTimeFormat;
	destination[offset] = enum_val(source.time_format);

	offset = InfoOffsetTimeSeparator;
	destination[offset + 0] = enum_val(source.time_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetDateFormat;
	destination[offset + 0] = enum_val(source.date_format);
	destination[offset + 1] = 0;

	offset = InfoOffsetDateSeparator;
	destination[offset + 0] = enum_val(source.date_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetThousandsSeparator;
	destination[offset + 0] = enum_val(source.thousands_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetDecimalSeparator;
	destination[offset + 0] = enum_val(source.decimal_separator);
	destination[offset + 1] = 0;

	offset = InfoOffsetListSeparator;
	if (source.list_separator) {
		destination[offset + 0] = enum_val(*source.list_separator);
	} else {
		const auto list_separator = get_default_list_separator(
		        source.thousands_separator, source.decimal_separator);
		destination[offset + 0] = enum_val(list_separator);
	}

	destination[offset + 1] = 0;
}

static void refresh_currency_format(const LocaleInfoEntry &source)
{
	const auto& destination = dos.tables.country;

	assert(source.currency_code.size() < MaxCurrencySymbolLength);
	memset(&destination[InfoOffsetCurrencySymbol], 0, MaxCurrencySymbolLength + 1);

	bool found = false;
	for (const auto& candidate_utf8 : source.currency_symbols_utf8) {

		// Check if the currency can be converted to current code page
		const auto candidate = utf8_to_dos(candidate_utf8,
		                                   DosStringConvertMode::NoSpecialCharacters,
		                                   UnicodeFallback::EmptyString);

		if (candidate.empty() || candidate.length() > MaxCurrencySymbolLength) {
			continue;
		}

		found = true;
		memcpy(&destination[InfoOffsetCurrencySymbol],
		       candidate.c_str(),
		       candidate.length());
		break;
	}

	size_t offset = InfoOffsetCurrencyFormat;
	destination[offset] = enum_val(source.currency_format);

	if (!found) {
		// Fallback - use the currency code instead
		memcpy(&destination[InfoOffsetCurrencySymbol],
		       source.currency_code.c_str(),
		       source.currency_code.length());

		// Force separation between symbol and amount
		bit::set(destination[offset], bit::literals::b0);
	}

	offset = InfoOffsetCurrencyPrecision;
	destination[offset] = source.currency_precision;
}

void DOS_RefreshCountryInfo(const bool keyboard_layout_changed)
{
	if (!config.is_config_loaded) {
		return;
	}

	if (config.auto_detect_country && !config.is_locale_generated &&
	    !keyboard_layout_changed) {
		return;
	}

	if (config.auto_detect_country) {
		auto_detect_country();
	}

	set_country_dos_code();
	const auto country_corrected = correct_country(config.country);

	assert(LocaleData::CountryInfo.count(country_corrected));
	const auto& country_info = LocaleData::CountryInfo.at(country_corrected);

	// Select locale period

	LocalePeriod locale_period = LocalePeriod::Modern;
	if (config.locale_period == LocalePeriod::Historic &&
	    country_info.locale_info.count(config.locale_period)) {
		locale_period = config.locale_period;
	}

	assert(country_info.locale_info.count(locale_period));
	const auto& source      = country_info.locale_info.at(locale_period);
	const auto& destination = dos.tables.country;

	// Set reserved/undocumented values to 0's
	memset(&destination[InfoOffsetReserved], 0, ReservedAreaSize);

	// Set time/date/number/list/currency formats
	refresh_time_date_format(source);
	refresh_currency_format(source);

	// Mark locale as generated
	config.is_locale_generated = true;

	// If locale changed, log it
	maybe_log_changed_country(country_info.country_name, locale_period);
}

static bool set_country(const DosCountry country, const bool no_fallback = false)
{
	if (!dos.tables.country) {
		assert(false);
		return false;
	}

	// Validate country ID
	const auto country_corrected = correct_country(country);
	if (!LocaleData::CountryInfo.count(country_corrected)) {
		if (no_fallback) {
			return false;
		}

		const auto default_country = static_cast<DosCountry>(
		        DOS_GetDefaultCountry());
		LOG_WARNING("DOS: No locale info for country %d, using default %d",
		            enum_val(country),
		            enum_val(default_country));

		if (!LocaleData::CountryInfo.count(default_country)) {
			assert(false);
			return false;
		}
		config.country = default_country;
	} else {
		config.country = country;
	}

	// Generate country information
	DOS_RefreshCountryInfo();
	return true;
}

bool DOS_SetCountry(const uint16_t country_id)
{
	if (country_id == 0) {
		return false; // for DOS int 21h call this is not valid
	}

	constexpr bool no_fallback = true;
	return set_country(static_cast<DosCountry>(country_id), no_fallback);
}

uint16_t DOS_GetCountry()
{
	return config.country_dos_code;
}

uint16_t DOS_GetDefaultCountry()
{
	if (config.locale_period == LocalePeriod::Historic) {
		return enum_val(DosCountry::UnitedStates);
	} else {
		return enum_val(DosCountry::International);
	}
}

// ***************************************************************************
// Autodetection code
// ***************************************************************************

static const std::map<std::string, DosCountry> LayoutToCountryMap = {
        // clang-format off
	// reference: https://gitlab.com/FreeDOS/base/keyb_lay/-/blob/master/DOC/KEYB/LAYOUTS/LAYOUTS.TXT
	{ "ar462",  DosCountry::Arabic             },
	{ "ar470",  DosCountry::Arabic             },
	{ "az",     DosCountry::Azerbaijan         },
	{ "ba",     DosCountry::BosniaLatin        },
	{ "be",     DosCountry::Belgium            },
	{ "bg",     DosCountry::Bulgaria           }, // 101-key
	{ "bg103",  DosCountry::Bulgaria           }, // 101-key, Phonetic
	{ "bg241",  DosCountry::Bulgaria           }, // 102-key
	{ "bl",     DosCountry::Belarus            },
	{ "bn",     DosCountry::Benin              },
	{ "br",     DosCountry::Brazil             }, // ABNT layout
	{ "br274",  DosCountry::Brazil             }, // US layout
	{ "bx",     DosCountry::Belgium            }, // International
	{ "by",     DosCountry::Belarus            },
	{ "ca",     DosCountry::CanadaEnglish      }, // Standard
	{ "ce",     DosCountry::Russia             }, // Chechnya Standard
	{ "ce443",  DosCountry::Russia             }, // Chechnya Typewriter
	{ "cg",     DosCountry::Montenegro         },
	{ "cf",     DosCountry::CanadaFrench       }, // Standard
	{ "cf445",  DosCountry::CanadaFrench       }, // Dual-layer
	{ "co",     DosCountry::UnitedStates       }, // Colemak
	{ "cz",     DosCountry::Czechia            }, // QWERTY
	{ "cz243",  DosCountry::Czechia            }, // Standard
	{ "cz489",  DosCountry::Czechia            }, // Programmers
	{ "de",     DosCountry::Germany            }, // Standard
	{ "dk",     DosCountry::Denmark            },
	{ "dv",     DosCountry::UnitedStates       }, // Dvorak
	{ "ee",     DosCountry::Estonia            },
	{ "el",     DosCountry::Greece             }, // 319
	{ "es",     DosCountry::Spain              },
	{ "et",     DosCountry::Estonia            },
	{ "fi",     DosCountry::Finland            },
	{ "fo",     DosCountry::FaroeIslands       },
	{ "fr",     DosCountry::France             }, // Standard
	{ "fx",     DosCountry::France             }, // International
	{ "gk",     DosCountry::Greece             }, // 319
	{ "gk220",  DosCountry::Greece             }, // 220
	{ "gk459",  DosCountry::Greece             }, // 101-key
	{ "gr",     DosCountry::Germany            }, // Standard
	{ "gr453",  DosCountry::Germany            }, // Dual-layer
	{ "hr",     DosCountry::Croatia            },
	{ "hu",     DosCountry::Hungary            }, // 101-key
	{ "hu208",  DosCountry::Hungary            }, // 102-key
	{ "hy",     DosCountry::Armenia            },
	{ "il",     DosCountry::Israel             },
	{ "is",     DosCountry::Iceland            }, // 101-key
	{ "is161",  DosCountry::Iceland            }, // 102-key
	{ "it",     DosCountry::Italy              }, // Standard
	{ "it142",  DosCountry::Italy              }, // Comma on Numeric Pad
	{ "ix",     DosCountry::Italy              }, // International
	{ "jp",     DosCountry::Japan              },
	{ "ka",     DosCountry::Georgia            },
	{ "kk",     DosCountry::Kazakhstan         },
	{ "kk476",  DosCountry::Kazakhstan         },
	{ "kx",     DosCountry::UnitedKingdom      }, // International
	{ "ky",     DosCountry::Kyrgyzstan         },
	{ "la",     DosCountry::LatinAmerica       },
	{ "lh",     DosCountry::UnitedStates       }, // Left-Hand Dvorak
	{ "lt",     DosCountry::Lithuania          }, // Baltic
	{ "lt210",  DosCountry::Lithuania          }, // 101-key, Programmers
	{ "lt211",  DosCountry::Lithuania          }, // AZERTY
	{ "lt221",  DosCountry::Lithuania          }, // Standard
	{ "lt456",  DosCountry::Lithuania          }, // Dual-layout
	{ "lv",     DosCountry::Latvia             }, // Standard
	{ "lv455",  DosCountry::Latvia             }, // Dual-layout
	{ "ml",     DosCountry::Malta              }, // UK-based
	{ "mk",     DosCountry::NorthMacedonia     },
	{ "mn",     DosCountry::Mongolia           },
	{ "mo",     DosCountry::Mongolia           },
	{ "mt",     DosCountry::Malta              }, // UK-based
	{ "mt103",  DosCountry::Malta              }, // US-based
	{ "ne",     DosCountry::Niger              },
	{ "ng",     DosCountry::Nigeria            },
	{ "nl",     DosCountry::Netherlands        }, // 102-key
	{ "no",     DosCountry::Norway             },
	{ "ph",     DosCountry::Philippines        },
	{ "pl",     DosCountry::Poland             }, // 101-key, Programmers
	{ "pl214",  DosCountry::Poland             }, // 102-key
	{ "po",     DosCountry::Portugal           },
	{ "px",     DosCountry::Portugal           }, // International
	{ "ro",     DosCountry::Romania            }, // Standard
	{ "ro446",  DosCountry::Romania            }, // QWERTY
	{ "rh",     DosCountry::UnitedStates       }, // Right-Hand Dvorak
	{ "ru",     DosCountry::Russia             }, // Standard
	{ "ru443",  DosCountry::Russia             }, // Typewriter
	{ "rx",     DosCountry::Russia             }, // Extended Standard
	{ "rx443",  DosCountry::Russia             }, // Extended Typewriter
	{ "sd",     DosCountry::Switzerland        }, // German
	{ "sf",     DosCountry::Switzerland        }, // French
	{ "sg",     DosCountry::Switzerland        }, // German
	{ "si",     DosCountry::Slovenia           },
	{ "sk",     DosCountry::Slovakia           },
	{ "sp",     DosCountry::Spain              },
	{ "sq",     DosCountry::Albania            }, // No-deadkeys
	{ "sq448",  DosCountry::Albania            }, // Deadkeys
	{ "sr",     DosCountry::Serbia             }, // Deadkey
	{ "su",     DosCountry::Finland            },
	{ "sv",     DosCountry::Sweden             },
	{ "sx",     DosCountry::Spain              }, // International
	{ "tj",     DosCountry::Tajikistan         },
	{ "tm",     DosCountry::Turkmenistan       },
	{ "tr",     DosCountry::Turkey             }, // QWERTY
	{ "tr440",  DosCountry::Turkey             }, // Non-standard
	{ "tt",     DosCountry::Russia             }, // Tatarstan Standard
	{ "tt443",  DosCountry::Russia             }, // Tatarstan Typewriter
	{ "ua",     DosCountry::Ukraine            }, // 101-key
	{ "uk",     DosCountry::UnitedKingdom      }, // Standard
	{ "uk168",  DosCountry::UnitedKingdom      }, // Alternate
	{ "ur",     DosCountry::Ukraine            }, // 101-key
	{ "ur465",  DosCountry::Ukraine            }, // 101-key
	{ "ur1996", DosCountry::Ukraine            }, // 101-key
	{ "ur2001", DosCountry::Ukraine            }, // 102-key
	{ "ur2007", DosCountry::Ukraine            }, // 102-key
	{ "us",     DosCountry::UnitedStates       }, // Standard
	{ "ux",     DosCountry::UnitedStates       }, // International
	{ "uz",     DosCountry::Uzbekistan         },
	{ "vi",     DosCountry::Vietnam            },
	{ "yc",     DosCountry::Serbia             }, // Deadkey
	{ "yc450",  DosCountry::Serbia             }, // No-deadkey
	{ "yu",     DosCountry::Yugoslavia         },
        // clang-format on
};

static const std::map<std::string, std::string> LanguageToLayoutExceptionMap = {
        {"nl", "us"},
};

std::string DOS_CheckLanguageToLayoutException(const std::string& language_code)
{
	if (!language_code.empty()) {
		const auto it = LanguageToLayoutExceptionMap.find(language_code);
		if (it != LanguageToLayoutExceptionMap.end()) {
			return it->second;
		}
	}
	return language_code;
}

bool DOS_GetCountryFromLayout(const std::string& layout, uint16_t& country)
{
	const auto it = LayoutToCountryMap.find(layout);
	if (it != LayoutToCountryMap.end()) {
		country = enum_val(it->second);
		return true;
	}

	return false;
}

uint16_t DOS_GetCodePageFromCountry(const uint16_t country)
{
	auto assert_code_page = [](const uint16_t code_page) {
		assert(!DOS_GetBundledCodePageFileName(code_page).empty());
		return code_page;
	};

	const auto country_id = static_cast<DosCountry>(country);

	// Countries grouped in ascending order by code_page value
	switch (country_id) {
	case DosCountry::AsiaEnglish:
	case DosCountry::Australia:
	case DosCountry::China:
	case DosCountry::HongKong:
	case DosCountry::India:
	case DosCountry::Indonesia:
	case DosCountry::International:
	case DosCountry::Ireland:
	case DosCountry::Japan:
	case DosCountry::SouthKorea:
	case DosCountry::Malaysia:
	case DosCountry::NewZealand:
	case DosCountry::Singapore:
	case DosCountry::SouthAfrica:
	case DosCountry::Taiwan:
	case DosCountry::Thailand: // only because me are missing Thai CPI files!
	case DosCountry::UnitedKingdom:
	case DosCountry::UnitedStates: return assert_code_page(437);

	// Stripped down 852 variant, just for Polish language, preserves more
	// table drawing characters than 852 [confirmed by a native speaker]
	case DosCountry::Poland: return assert_code_page(668);

	// Note: there seems to be no 774 variant with EUR currency
	case DosCountry::Lithuania: return assert_code_page(774);

	case DosCountry::Argentina:
	case DosCountry::Bolivia:
	case DosCountry::CanadaEnglish:
	case DosCountry::Chile:
	case DosCountry::Colombia:
	case DosCountry::CostaRica:
	case DosCountry::Ecuador:
	case DosCountry::ElSalvador:
	case DosCountry::Guatemala:
	case DosCountry::Honduras:
	case DosCountry::LatinAmerica:
	case DosCountry::Mexico:
	case DosCountry::Nicaragua:
	case DosCountry::Panama:
	case DosCountry::Paraguay:
	case DosCountry::Philippines:
	case DosCountry::Sweden:
	case DosCountry::Switzerland:
	case DosCountry::Uruguay:
	case DosCountry::Venezuela: return assert_code_page(850);

	case DosCountry::Austria:
	case DosCountry::Belgium:
	case DosCountry::Finland:
	case DosCountry::France:
	case DosCountry::Germany:
	case DosCountry::Italy:
	case DosCountry::Luxembourg:
	case DosCountry::Netherlands:
	case DosCountry::Spain:
		if (config.locale_period == LocalePeriod::Modern) {
			return assert_code_page(858); // supports EUR
		} else {
			return assert_code_page(850);
		}

	case DosCountry::Albania:
	case DosCountry::Croatia:
	case DosCountry::Montenegro:
	case DosCountry::Romania:
	case DosCountry::Slovenia:
	case DosCountry::Turkmenistan: return assert_code_page(852);

	case DosCountry::Malta: return assert_code_page(853);

	case DosCountry::BosniaCyrillic:
	case DosCountry::BosniaLatin:
	case DosCountry::NorthMacedonia:
	case DosCountry::Serbia:
	case DosCountry::Yugoslavia: return assert_code_page(855);

	case DosCountry::Turkey: return assert_code_page(857);

	case DosCountry::Brazil:
	// Note: there seems to be no 860 variant with EUR currency
	case DosCountry::Portugal: return assert_code_page(860);

	case DosCountry::FaroeIslands:
	case DosCountry::Iceland: return assert_code_page(861);

	case DosCountry::Israel: return assert_code_page(862);

	case DosCountry::CanadaFrench: return assert_code_page(863);

	case DosCountry::Algeria:
	case DosCountry::Arabic:
	case DosCountry::Bahrain:
	case DosCountry::Egypt:
	case DosCountry::Emirates:
	case DosCountry::Jordan:
	case DosCountry::Kuwait:
	case DosCountry::Lebanon:
	case DosCountry::Morocco:
	case DosCountry::Oman:
	case DosCountry::SaudiArabia:
	case DosCountry::Syria:
	case DosCountry::Tunisia:
	case DosCountry::Pakistan:
	case DosCountry::Qatar:
	case DosCountry::Yemen: return assert_code_page(864);

	case DosCountry::Denmark:
	case DosCountry::Norway: return assert_code_page(865);

	case DosCountry::Russia: return assert_code_page(866);

	// Kamenický encoding
	// Note: there seems to be no 774 variant with EUR currency
	case DosCountry::Czechia:
	case DosCountry::Slovakia: return assert_code_page(867);

	case DosCountry::Greece: return assert_code_page(869); // supports EUR

	case DosCountry::Armenia: return assert_code_page(899);

	// Note: there seems to be no 1116 variant with EUR currency
	case DosCountry::Estonia: return assert_code_page(1116);

	// Note: there seems to be no 1117 variant with EUR currency
	case DosCountry::Latvia: return assert_code_page(1117);

	case DosCountry::Ukraine: return assert_code_page(1125);

	case DosCountry::Belarus: return assert_code_page(1131);

	// MIK encoding [confirmed by a native speaker]
	case DosCountry::Bulgaria: return assert_code_page(3021);

	// CWI-2 encoding
	case DosCountry::Hungary: return assert_code_page(3845);

	case DosCountry::Tajikistan: return assert_code_page(30002);

	case DosCountry::Nigeria: return assert_code_page(30005);

	case DosCountry::Vietnam: return assert_code_page(30006);

	case DosCountry::Benin: return assert_code_page(30027);

	case DosCountry::Niger: return assert_code_page(30028);

	case DosCountry::Kazakhstan:
	case DosCountry::Kyrgyzstan:
	case DosCountry::Mongolia: return assert_code_page(58152);

	case DosCountry::Azerbaijan: return assert_code_page(58210);

	case DosCountry::Georgia: return assert_code_page(59829);

	case DosCountry::Uzbekistan: return assert_code_page(62306);

	default:
		LOG_WARNING("DOS: No default code page for country %d, '%s'",
		            enum_val(country_id),
		            get_country_name(country_id).c_str());
		return assert_code_page(DefaultCodePage437);
	}
}

std::string DOS_GetLayoutFromHost()
{
#if defined(WIN32)
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
#endif
	// TODO: On POSIX systems try to check environment variables to get
	//       the language, in order: LANGUAGE, LC_ALL (and others), LANG.
	//       Check LC_MESSAGES, LC_NUMERIC, LC_TIME, LC_MONETARY, even
	//       LC_COLLATE - maybe we should create a 'mixed' locale?
	//       Also check 'get_language_from_os()' from 'cross.cpp'.

	return "";
}

// ***************************************************************************
// Locale retrieval functions
// ***************************************************************************

std::string DOS_GetBundledCodePageFileName(const uint16_t code_page)
{
	// reference:
	// https://gitlab.com/FreeDOS/base/cpidos/-/blob/master/DOC/CPIDOS/CODEPAGE.TXT
	switch (code_page) {
	case 437:
	case 850:
	case 852:
	case 853:
	case 857:
	case 858: return "EGA.CPI";
	case 775:
	case 859:
	case 1116:
	case 1117:
	case 1118:
	case 1119: return "EGA2.CPI";
	case 771:
	case 772:
	case 808:
	case 855:
	case 866:
	case 872: return "EGA3.CPI";
	case 848:
	case 849:
	case 1125:
	case 1131:
	case 3012:
	case 30010: return "EGA4.CPI";
	case 113:
	case 737:
	case 851:
	case 869: return "EGA5.CPI";
	case 899:
	case 30008:
	case 58210:
	case 59829:
	case 60258:
	case 60853: return "EGA6.CPI";
	case 30011:
	case 30013:
	case 30014:
	case 30017:
	case 30018:
	case 30019: return "EGA7.CPI";
	case 770:
	case 773:
	case 774:
	case 777:
	case 778: return "EGA8.CPI";
	case 860:
	case 861:
	case 863:
	case 865:
	case 867: return "EGA9.CPI";
	case 667:
	case 668:
	case 790:
	case 991:
	case 3845: return "EGA10.CPI";
	case 30000:
	case 30001:
	case 30004:
	case 30007:
	case 30009: return "EGA11.CPI";
	case 30003:
	case 30029:
	case 30030:
	case 58335: return "EGA12.CPI";
	case 895:
	case 30002:
	case 58152:
	case 59234:
	case 62306: return "EGA13.CPI";
	case 30006:
	case 30012:
	case 30015:
	case 30016:
	case 30020:
	case 30021: return "EGA14.CPI";
	case 30023:
	case 30024:
	case 30025:
	case 30026:
	case 30027:
	case 30028: return "EGA15.CPI";
	case 3021:
	case 30005:
	case 30022:
	case 30031:
	case 30032: return "EGA16.CPI";
	case 862:
	case 864:
	case 30034:
	case 30033:
	case 30039:
	case 30040: return "EGA17.CPI";
	case 856:
	case 3846:
	case 3848: return "EGA18.CPI";
	default: return {}; // none
	}
}

DosDateFormat DOS_GetLocaleDateFormat()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetDateFormat;
	return static_cast<DosDateFormat>(dos.tables.country[offset]);
}

DosTimeFormat DOS_GetLocaleTimeFormat()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetTimeFormat;
	return static_cast<DosTimeFormat>(dos.tables.country[offset]);
}

char DOS_GetLocaleDateSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetDateSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleTimeSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetTimeSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleThousandsSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetThousandsSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleDecimalSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetDecimalSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

char DOS_GetLocaleListSeparator()
{
	assert(dos.tables.country);

	constexpr auto offset = InfoOffsetListSeparator;
	return static_cast<char>(dos.tables.country[offset]);
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

class DOS_Locale final : public Module_base {
public:
	DOS_Locale(Section* configuration) : Module_base(configuration)
	{
		auto section = static_cast<Section_prop*>(configuration);
		assert(section);

		const auto locale_period = section->Get_string("locale_period");
		const auto country_code  = section->Get_string("country");

		if (locale_period == "modern") {
			config.locale_period = LocalePeriod::Modern;
		} else if (locale_period == "historic") {
			config.locale_period = LocalePeriod::Historic;
		} else {
			assert(false);
		}

		config.is_config_loaded = true;

		if (country_code == "auto") {
			config.auto_detect_country = true;
			// Autoselection is going to be performed once
			// keyboard layout is loaded
		} else {
			config.auto_detect_country = false;
			const auto property_value  = parse_int(country_code);
			DosCountry country         = {};
			if (!property_value || (*property_value < 0) ||
			    (*property_value > UINT16_MAX)) {
				LOG_WARNING("DOS: '%s' is not a valid country code, using default",
				            country_code.c_str());
				// NOTE: Real MS-DOS 6.22 uses modified locale
				// in this case, it uses country 1 with date
				// separator '-' instead of '/'.
				// This is not simulated, I believe it is due to
				// hardcoded locale being slightly different
				// than the one from COUNTRY.SYS (MS-DOS bug)
				country = static_cast<DosCountry>(DOS_GetDefaultCountry());
			} else {
				country = static_cast<DosCountry>(*property_value);
			}
			set_country(country);
		}
	}

	~DOS_Locale() {}
};

static std::unique_ptr<DOS_Locale> Locale = {};

void DOS_Locale_ShutDown(Section*)
{
	Locale.reset();
}

void DOS_Locale_Init(Section* sec)
{
	assert(sec);
	Locale = std::make_unique<DOS_Locale>(sec);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&DOS_Locale_ShutDown, changeable_at_runtime);
}

void DOS_Locale_AddMessages()
{
	MSG_Add("DOSBOX_HELP_LIST_COUNTRIES_1",
	        "List of country codes (mostly same as telephone call codes)\n"
	        "-----------------------------------------------------------");
	MSG_Add("DOSBOX_HELP_LIST_COUNTRIES_2",
	        "The above numeric country codes can be used exactly as listed\n"
	        "in the 'country' config setting.");

	// Add strings with country names
	for (auto it = LocaleData::CountryInfo.begin();
	     it != LocaleData::CountryInfo.end(); ++it) {
		MSG_Add(it->second.GetMsgName().c_str(),
		        it->second.country_name.c_str());
	}
}
