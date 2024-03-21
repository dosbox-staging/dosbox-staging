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

// XXX use:
// - CFLocaleCopyCurrent / CFLocaleGetValue
// - kCFLocaleCountryCode / kCFLocaleLanguageCode

// XXX see: https://keyshorts.com/blogs/blog/37615873-how-to-identify-macbook-keyboard-localization

/* XXX
	#include <CoreFoundation/CoreFoundation.h>

	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	auto value = (CFStringRef)CFLocaleGetValue(cflocale, kCFLocaleLanguageCode);    
	std::string str(CFStringGetCStringPtr(value, kCFStringEncodingASCII));
	CFRelease(cflocale);


	char layout[128];
	TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    	CFStringRef layoutID = TISGetInputSourceProperty(source, kTISPropertyInputSourceID);
    	CFStringGetCString(layoutID, layout, sizeof(layout), kCFStringEncodingASCII);
    	printf("%s\n", layout);


	// keyboard layouts (with proposed FreeDOS layouts):

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

/* XXX
	// Lamda helper to extract the language from macOS locale
#if C_COREFOUNDATION
	auto get_lang_from_macos = []() {
		const auto lc_array = CFLocaleCopyPreferredLanguages();
		const auto locale_ref = CFArrayGetValueAtIndex(lc_array, 0);
		const auto lc_cfstr = reinterpret_cast<CFStringRef>(locale_ref);
		auto lang = cfstr_to_string(lc_cfstr);
		clear_language_if_default(lang);
		return lang;
	};
#endif
*/