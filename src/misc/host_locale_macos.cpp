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

#include <CoreFoundation/CoreFoundation.h>

CHECK_NARROWING();

// clang-format off

/* TODO: We still need to find all the configured keyboard layouts and publish
         them (after converting to DOS keyboard layouts) in 'GetHostLocale'.

Most likely the code to fetch the current layout should look more-or-less like
example below. but we need all the user's preferred layouts, not just the
current one.

  char layout[128];
  TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
  CFStringRef layout = TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
  CFStringGetCString(layout, layout, sizeof(layout), kCFStringEncodingASCII);
  printf("%s\n", layout);

Based on the content from:
- https://keyshorts.com/blogs/blog/37615873-how-to-identify-macbook-keyboard-localization
here is the proposed list of host to guest keyboard layout (and sometimes
concrete code page) translation table:

        US English                 -> "us"
        Cherokee                   -> "us", 30034
        US English International   -> "ux"
        Colemak                    -> "co"
        Dvorak                     -> "dv"
        UK (British) English       -> "uk"
        Arabic                     -> "ar470"
        Kurdish (Sorani)           -> "ar470"
        Malay (Jawi)               -> "ar470"
        Pashto                     -> "ar470"
        Persian/Farsi              -> "ar470"
        Uyghur                     -> "ar470"
        Azeri/Azerbaijani          -> "az"
        Bosnian                    -> "ba"
        Belgian                    -> "be"
        Bulgarian                  -> "bg"
        Portuguese (Brazilian)     -> "br"
        French (Canadian)          -> "cf"
        Inuktitut                  -> "cf", 30022
        Czech                      -> "cz"
        German                     -> "de"
        Danish                     -> "dk"
        Estonian                   -> "ee"
        Spanish                    -> "es"
        Finnish                    -> "fi"
        French                     -> "fr"
        Greek                      -> "gk"
        Greek (Polytonic)          -> "gk"
        Croatian                   -> "hr"
        Hungarian                  -> "hu208"
        Armenian                   -> "hy"
        Hebrew                     -> "il"
        Icelandic                  -> "is161"
        Italian                    -> "it142"
        Georgian                   -> "ka"
        Spanish (Latin America)    -> "la"
        Lithuanian                 -> "lt"
        Latvian                    -> "lv"
        Macedonian                 -> "mk"
        Maltese                    -> "mt"
        Dutch                      -> "nl"
        Norwegian                  -> "no"
        Northern Sami              -> "no", 30000
        Polish                     -> "pl214""
        Polish Pro                 -> "pl"
        Portuguese                 -> "po"
        Romanian                   -> "ro446"
        Russian                    -> "ru"
        Russian (Phonetic)         -> "ru"
        Swiss                      -> "sd"
        Slovene/Slovenian          -> "si"
        Slovak                     -> "sk"
        Swedish                    -> "sv"
        Turkish Q                  -> "tr"
        Turkish F                  -> "tr440"
        Ukrainian                  -> "ua"
        Uzbek                      -> "uz"
        Vietnamese                 -> "vi"
        Serbian                    -> "yc"
        Serbian (Latin)            -> "yc"
        // For some keyboard families we don't have code pages, but in the
        // corresponding states the QWERTY layout is typically used
        Bengali                    -> "us"
        Burmese                    -> "us"
        Chinese                    -> "us"
        Gujarati                   -> "us"
        Hindi                      -> "us"
        Japanese                   -> "us"
        Kannada                    -> "us"
        Khmer                      -> "us"
        Korean                     -> "us"
        Malayalam                  -> "us"
        Nepali                     -> "us"
        Odia/Oriya                 -> "us"
        Punjabi (Gurmukhi)         -> "us"
        Sinhala                    -> "us"
        Tamil                      -> "us"
        Telugu                     -> "us"
        Thai                       -> "us"
        Tibetan                    -> "us"
        Urdu                       -> "us"
*/

// clang-format on

static std::string get_locale(const CFLocaleKey key)
{
	std::string result = {};

	CFLocaleRef locale = CFLocaleCopyCurrent();
	const auto value   = CFLocaleGetValue(locale, key);

	result = CFStringGetCStringPtr(static_cast<CFStringRef>(value),
	                               kCFStringEncodingASCII);
	CFRelease(locale);

	return result;
}

static std::optional<DosCountry> get_dos_country(std::string& log_info)
{
	const auto language = get_locale(kCFLocaleLanguageCode);
	const auto country  = get_locale(kCFLocaleCountryCode);

	log_info = language + "_" + country;

	return IsoToDosCountry(language, country);
}

static std::string get_language_file(std::string& log_info)
{
	const auto language = get_locale(kCFLocaleLanguageCode);
	const auto country  = get_locale(kCFLocaleCountryCode);

	log_info = language + "_" + country;

	if (language == "pt" && country == "BR") {
		return "br"; // we have a dedicated Brazilian translation
	}

	return language;
}

const HostLocale& GetHostLocale()
{
	static std::optional<HostLocale> locale = {};

	if (!locale) {
		locale = HostLocale();

		locale->country = get_dos_country(locale->log_info.country);
		// TODO: Fill in:
		// - keyboard_layout_list
		// - log_info.log_info
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
