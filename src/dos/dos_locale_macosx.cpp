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

/* XXX
	#include <CoreFoundation/CoreFoundation.h>

	CFLocaleRef cflocale = CFLocaleCopyCurrent();
	auto value = (CFStringRef)CFLocaleGetValue(cflocale, kCFLocaleLanguageCode);    
	std::string str(CFStringGetCStringPtr(value, kCFStringEncodingUTF8));
	CFRelease(cflocale);

   // keyboard layouts:

    US English
    US English International
    UK (British) English
    Arabic
    Armenian
    Azeri/Azerbaijani
    Belgian
    Bengali
    Bosnian
    Bulgarian
    Burmese
    Cherokee
    Chinese
    Colemak
    Croatian
    Czech
    Danish
    Dvorak
    Dutch
    Estonian
    Finnish
    French
    French (Canadian)
    Georgian
    German
    Greek
    Greek (Polytonic)
    Gujarati
    Hebrew
    Hindi
    Hungarian
    Icelandic
    Inuktitut
    Italian
    Japanese
    Kannada
    Kazakh
    Khmer
    Korean
    Kurdish (Sorani)
    Latvian
    Lithuanian
    Macedonian
    Malay (Jawi)
    Malayalam
    Maltese
    Nepali
    Northern Sami
    Norwegian
    Odia/Oriya
    Pashto
    Persian/Farsi
    Polish
    Polish Pro
    Portuguese
    Portuguese (Brazilian)
    Punjabi (Gurmukhi)
    Romanian
    Russian
    Russian (Phonetic)
    Serbian
    Serbian (Latin)
    Sinhala
    Slovak
    Slovene/Slovenian
    Spanish
    Spanish (Latin America)
    Swedish
    Swiss
    Tamil
    Telugu
    Thai
    Tibetan
    Turkish F
    Turkish Q
    Ukrainian
    Urdu
    Uyghur
    Uzbek
    Vietnamese




 DOS keyboard layouts:

ar462         // Algeria, Morocco, Tunisia
ar470         // Algeria, Bahrain, Egypt, Jordan, Kuwait, Lebanon, Morocco, Oman, Qatar, Saudi Arabia, Syria, Tunisia, UAE, Yemen
az            // Azerbaijan
ba            // Bosnia & Herzegovina
be            // Belgium
bg            // Bulgaria (101-key)
bg103         // Bulgaria (101 phonetic)
bg241         // Bulgaria (102-key)
bl            // Belarus
bn            // Benin
br            // Brazil (ABNT layout)
br274         // Brazil (US layout)
bx            // Belgium (international)
by            // Belarus
ca            // Canada (Standard)
ce            // Russia (Chechnya Standard)
ce443         // Russia (Chechnya Typewriter)
cf            // Canada (Standard)
cf445         // Canada (Dual-layer)
cg            // Montenegro
co            // US (Colemak)
cz            // Czech Republic (QWERTY) 
cz243         // Czech Republic (Standard)
cz489         // Czech Republic (Programmers)
de            // Austria (Standard), Germany (Standard)
dk            // Denmark
dv            // US (Dvorak)
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
il            // Israel
is            // Iceland (101-key)
is161         // Iceland (102-key)
it            // Italy (Standard)
it142         // Italy (Comma on Numeric Pad)
ix            // Italy (international)
jp            // Japan
ka            // Georgia
kk            // Kazakhstan
kk476         // Kazakhstan
kx            // UK (International)
ky            // Kyrgyzstan
la            // Argentina, Chile, Colombia, Ecuador, Latin America, Mexico, Venezuela
lh            // US (Left-Hand Dvorak)
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
no            // Norway
ph            // Philippines
pl            // Poland (101-key, prog.)
pl214         // Poland (102-key)
po            // Portugal
px            // Portugal (international)
rh            // US (Right-Hand Dvorak)
ro            // Romania (Standard)
ro446         // Romania (QWERTY)
ru            // Russia (Standard)
ru443         // Russia (Typewriter)
rx            // Russia (Extended Standard)
rx443         // Russia (Extended Typewriter)
sd            // Switzerland (German)
sf            // Switzerland (French)
sg            // Switzerland (German)
si            // Slovenia
sk            // Slovakia
sp            // Spain
sq            // Albania (No deadkeys)
sq448         // Albania (Deadkeys)
sr            // Serbia (Deadkey)
su            // Finland
sv            // Sweden
sx            // Spain (international)
tj            // Tadjikistan
tm            // Turkmenistan
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
us            // Australia, New Zealand, South Africa, US (Standard)
ux            // US (International)
uz            // Uzbekistan
vi            // Vietnam
yc            // Serbia (Deadkey)
yc450         // Serbia (No deadkey)
yu            // Bosnia & Herzegovina, Croatia, Montenegro, Slovenia


*/