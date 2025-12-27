// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_ISO_LOCALE_CODES_H
#define DOSBOX_ISO_LOCALE_CODES_H

#include <string>

// ***************************************************************************
// ISO country/territory definitions
// ***************************************************************************

// Two-letter ISO 3166 country/territory codes, also contains several historic
// and unofficial codes. References:
// - https://en.wikipedia.org/wiki/ISO_3166-1
// - https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2

namespace Iso3166 {

// International codes
extern const std::string UnknownState;
extern const std::string InternationalWaters;
extern const std::string OutlyingOceania;
extern const std::string UnitedNations;
extern const std::string Antarctica;
extern const std::string EuropeanUnion;
extern const std::string EuroZone;

// United States of America and associated
extern const std::string UnitedStates;
extern const std::string UnitedStates_ISRC; // code only used by ISRC
extern const std::string AmericanSamoa;
extern const std::string Guam;
extern const std::string JohnstonIsland;
extern const std::string MidwayIslands;
extern const std::string MinorOutlyingIslands;
extern const std::string MiscPacificIslands;
extern const std::string NorthernMarianaIslands;
extern const std::string PuertoRico;
extern const std::string VirginIslandsUS;
extern const std::string WakeIsland;

// United Kingdom of Great Britain and Northern Ireland and associated
extern const std::string UnitedKingdom;
extern const std::string UnitedKingdom_Alternative; // alternative code
extern const std::string Anguilla;
extern const std::string AscensionIsland;
extern const std::string Bermuda;
extern const std::string BritishIndianOceanTerritory;
extern const std::string CaymanIslands;
extern const std::string DiegoGarcia;
extern const std::string FalklandIslands;
extern const std::string Gibraltar;
extern const std::string Guernsey;
extern const std::string IslandOfSark;
extern const std::string IsleOfMan;
extern const std::string Jersey;
extern const std::string Montserrat;
extern const std::string NorthernIreland;
extern const std::string Pitcairn;
extern const std::string SaintHelena;
extern const std::string SouthGeorgia;
extern const std::string TristanDaCunha;
extern const std::string TurksAndCaicosIslands;
extern const std::string VirginIslandsUK;

// France and associated
extern const std::string France;
extern const std::string France_Metropolitan;
extern const std::string ClippertonIsland;
extern const std::string FrenchAntarcticTerritories;
extern const std::string FrenchGuiana;
extern const std::string FrenchPolynesia;
extern const std::string FrenchSouthernTerritories;
extern const std::string Guadeloupe;
extern const std::string Martinique;
extern const std::string Mayotte;
extern const std::string NewCaledonia;
extern const std::string Reunion;
extern const std::string SaintBarthelemy;
extern const std::string SaintMartin;
extern const std::string SaintPierreAndMiquelon;
extern const std::string WallisAndFutuna;

// Spain and associated
extern const std::string Spain;
extern const std::string CanaryIslands;
extern const std::string CanaryIslands_Alternative; // code used in Switzerland
extern const std::string CeutaAndMelilla;

// Netherlands and associated
extern const std::string Netherlands;
extern const std::string Aruba;
// Bonaire, Sint Eustatius and Saba
extern const std::string Bonaire;
extern const std::string Curacao;
extern const std::string DutchAntilles;
extern const std::string SintMaarten;

// Denmark and associated
extern const std::string Denmark;
extern const std::string FaroeIslands;
extern const std::string Greenland;

// Finland and associated
extern const std::string Finland;
extern const std::string AlandIslands;

// Norway and associated
extern const std::string Norway;
extern const std::string BouvetIsland;
extern const std::string DronningMaudLand;
extern const std::string SvalbardAndJanMayen;

// Australia and associated
extern const std::string Australia;
extern const std::string ChristmasIsland;
extern const std::string CocosIslands;
extern const std::string HeardAndMcDonaldIslands;
extern const std::string NorfolkIsland;

// New Zealand and associated
extern const std::string NewZealand;
extern const std::string CookIslands;
extern const std::string Niue;
extern const std::string Tokelau;

// China and associated
extern const std::string China;
extern const std::string HongKong;
extern const std::string Macao;

// Europe
extern const std::string Albania;
extern const std::string Andorra;
extern const std::string Armenia;
extern const std::string Austria;
extern const std::string Azerbaijan;
extern const std::string Belarus;
extern const std::string Belgium;
extern const std::string BosniaAndHerzegovina;
extern const std::string Bulgaria;
extern const std::string Croatia;
extern const std::string Cyprus;
extern const std::string Czechia;
extern const std::string Czechoslovakia;
extern const std::string EastGermany;
extern const std::string Estonia;
extern const std::string Georgia;
extern const std::string Germany;
extern const std::string Greece;
extern const std::string Hungary;
extern const std::string Iceland;
extern const std::string Ireland;
extern const std::string Italy;
extern const std::string Kazakhstan;
extern const std::string Kosovo;
extern const std::string Latvia;
extern const std::string Liechtenstein;
extern const std::string Lithuania;
extern const std::string Luxembourg;
extern const std::string Malta;
extern const std::string Moldova;
extern const std::string Monaco;
extern const std::string Montenegro;
extern const std::string NorthMacedonia;
extern const std::string Poland;
extern const std::string Portugal;
extern const std::string Romania;
extern const std::string Russia;
extern const std::string SanMarino;
extern const std::string Serbia;
extern const std::string Slovakia;
extern const std::string Slovenia;
extern const std::string SovietUnion;
extern const std::string Sweden;
extern const std::string Switzerland;
extern const std::string Turkey;
extern const std::string Ukraine;
extern const std::string VaticanCity;
extern const std::string Yugoslavia;

// Asia
extern const std::string Afghanistan;
extern const std::string Bahrain;
extern const std::string Bangladesh;
extern const std::string Bhutan;
extern const std::string Brunei;
extern const std::string Burma;
extern const std::string Cambodia;
extern const std::string Emirates; // United Arab Emirates
extern const std::string India;
extern const std::string Indonesia;
extern const std::string Iran;
extern const std::string Iraq;
extern const std::string Israel;
extern const std::string Japan;
extern const std::string Jordan;
extern const std::string Kuwait;
extern const std::string Kyrgyzstan;
extern const std::string Laos;
extern const std::string Lebanon;
extern const std::string Malaysia;
extern const std::string Maldives;
extern const std::string Mongolia;
extern const std::string Myanmar;
extern const std::string Nepal;
extern const std::string NeutralZone;
extern const std::string NorthKorea;
extern const std::string NorthVietnam;
extern const std::string Oman;
extern const std::string Pakistan;
extern const std::string Palestine;
extern const std::string Philippines;
extern const std::string Qatar;
extern const std::string SaudiArabia;
extern const std::string Singapore;
extern const std::string SouthKorea;
extern const std::string SouthYemen;
extern const std::string SriLanka;
extern const std::string Syria;
extern const std::string Taiwan;
extern const std::string Tajikistan;
extern const std::string Thailand;
extern const std::string Turkmenistan;
extern const std::string Uzbekistan;
extern const std::string Yemen;
extern const std::string Vietnam;

// Africa
extern const std::string Algeria;
extern const std::string Angola;
extern const std::string Benin;
extern const std::string Botswana;
extern const std::string BurkinaFaso;
extern const std::string Burundi;
extern const std::string CaboVerde;
extern const std::string Cameroon;
extern const std::string CentralAfricanRepublic;
extern const std::string Chad;
extern const std::string Comoros;
extern const std::string Congo;
extern const std::string CongoKinshasa;
extern const std::string Dahomey;
extern const std::string Djibouti;
extern const std::string Egypt;
extern const std::string EquatorialGuinea;
extern const std::string Eritrea;
extern const std::string Eswatini;
extern const std::string Ethiopia;
extern const std::string Gabon;
extern const std::string Gambia;
extern const std::string Ghana;
extern const std::string Guinea;
extern const std::string GuineaBissau;
extern const std::string IvoryCoast;
extern const std::string Kenya;
extern const std::string Lesotho;
extern const std::string Liberia;
extern const std::string Libya;
extern const std::string Mali;
extern const std::string Madagascar;
extern const std::string Malawi;
extern const std::string Mauritania;
extern const std::string Mauritius;
extern const std::string Morocco;
extern const std::string Mozambique;
extern const std::string Namibia;
extern const std::string Niger;
extern const std::string Nigeria;
extern const std::string Rwanda;
extern const std::string SaoTomeAndPrincipe;
extern const std::string Senegal;
extern const std::string Seychelles;
extern const std::string SierraLeone;
extern const std::string Somalia;
extern const std::string SouthAfrica;
extern const std::string SouthSudan;
extern const std::string SouthernRhodesia;
extern const std::string Sudan;
extern const std::string Tanzania;
extern const std::string Togo;
extern const std::string Tunisia;
extern const std::string Uganda;
extern const std::string UpperVolta;
extern const std::string WesternSahara;
extern const std::string Zaire;
extern const std::string Zambia;
extern const std::string Zimbabwe;

// Americas
extern const std::string AntiguaAndBarbuda;
extern const std::string Argentina;
extern const std::string Bahamas;
extern const std::string Barbados;
extern const std::string Belize;
extern const std::string Bolivia;
extern const std::string Brazil;
extern const std::string Canada;
extern const std::string Chile;
extern const std::string Colombia;
extern const std::string CostaRica;
extern const std::string Cuba;
extern const std::string Dominica;
extern const std::string Dominicana;
extern const std::string Ecuador;
extern const std::string ElSalvador;
extern const std::string Grenada;
extern const std::string Guatemala;
extern const std::string Guyana;
extern const std::string Haiti;
extern const std::string Honduras;
extern const std::string Jamaica;
extern const std::string Mexico;
extern const std::string Nicaragua;
extern const std::string Panama;
extern const std::string PanamaCanalZone;
extern const std::string Paraguay;
extern const std::string Peru;
extern const std::string SaintKittsAndNevis;
extern const std::string SaintLucia;
extern const std::string SaintVincentAndGrenadines;
extern const std::string Suriname;
extern const std::string TrinidadAndTobago;
extern const std::string Uruguay;
extern const std::string Venezuela;

// Oceania
extern const std::string CantonAndEnderburyIslands;
extern const std::string EastTimor;
extern const std::string Fiji;
extern const std::string Kiribati;
extern const std::string MarshallIslands;
extern const std::string Micronesia;
extern const std::string Nauru;
extern const std::string NewHebrides;
extern const std::string Palau;
extern const std::string PapuaNewGuinea;
extern const std::string Samoa;
extern const std::string SolomonIslands;
extern const std::string TimorLeste;
extern const std::string Tonga;
extern const std::string Tuvalu;
extern const std::string Vanuatu;

} // namespace Iso3166

// ***************************************************************************
// ISO language definitions
// ***************************************************************************

// Two-letter ISO 639 language codes. Also contains some three-letter codes,
// for the languages supported by the FreeDOS code pages and keyboard layouts,
// which do not have two-letter codes. Languages not having any ISO 639 code and
// huge groups of languages (sometimes over 50) were omitted.
//
// References:
// - https://en.wikipedia.org/wiki/ISO_639
// - https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes
// - https://en.wikipedia.org/wiki/List_of_ISO_639-2_codes
// - https://en.wikipedia.org/wiki/List_of_ISO_639-3_codes

namespace Iso639 {

extern const std::string English;

// Two-letter codes
extern const std::string Abkhazian;
extern const std::string Afar;
extern const std::string Afrikaans;
extern const std::string Akan;
extern const std::string Albanian;
extern const std::string Amharic;
extern const std::string Arabic;
extern const std::string Aragonese;
extern const std::string Armenian;
extern const std::string Assamese;
extern const std::string Avaric;
extern const std::string Avestan;
extern const std::string Aymara;
extern const std::string Azerbaijani; // a.k.a. Azeri
extern const std::string Bambara;
extern const std::string Bashkir;
extern const std::string Basque;
extern const std::string Belarusian;
extern const std::string Bengali;
extern const std::string Bislama;
extern const std::string Bosnian;
extern const std::string Breton;
extern const std::string Bulgarian;
extern const std::string Burmese;
extern const std::string Catalan;
extern const std::string Chamorro;
extern const std::string Chechen;
extern const std::string Chichewa; // a.k.a. Nyanja
extern const std::string Chinese;
extern const std::string ChurchSlavonic;
extern const std::string Chuvash;
extern const std::string Cornish;
extern const std::string Corsican;
extern const std::string Cree;
extern const std::string Croatian;
extern const std::string Czech;
extern const std::string Danish;
extern const std::string Divehi;
extern const std::string Dutch;
extern const std::string Dzongkha;
extern const std::string Esperanto;
extern const std::string Estonian;
extern const std::string Ewe;
extern const std::string Faroese;
extern const std::string Fijian;
extern const std::string Finnish;
extern const std::string French;
extern const std::string WesternFrisian;
extern const std::string Fulah;  // a.k.a. Fula
extern const std::string Gaelic; // a.k.a. Scottish Gaelic
extern const std::string Galician;
extern const std::string Ganda; // a.k.a. Luganda
extern const std::string Georgian;
extern const std::string German;
extern const std::string Greek;
extern const std::string Greenlandic;
extern const std::string Guarani;
extern const std::string Gujarati;
extern const std::string Haitian;
extern const std::string Hausa;
extern const std::string Hebrew;
extern const std::string Herero;
extern const std::string Hindi;
extern const std::string HiriMotu;
extern const std::string Hungarian;
extern const std::string Icelandic;
extern const std::string Ido;
extern const std::string Igbo;
extern const std::string Indonesian;
extern const std::string Interlingua;
extern const std::string Interlingue;
extern const std::string Inuktitut;
extern const std::string Inupiaq;
extern const std::string Irish; // a.k.a Irish Gaelic
extern const std::string Italian;
extern const std::string Japanese;
extern const std::string Javanese;
extern const std::string Kannada;
extern const std::string Kanuri;
extern const std::string Kashmiri;
extern const std::string Kazakh;
extern const std::string Khmer;
extern const std::string Kikuyu;
extern const std::string Kinyarwanda;
extern const std::string Kyrgyz;
extern const std::string Komi;
extern const std::string Kongo; // a.k.a. Kikongo
extern const std::string Korean;
extern const std::string Kuanyama;
extern const std::string Kurdish;
extern const std::string Lao;
extern const std::string Latin;
extern const std::string Latvian;
extern const std::string Limburgan;
extern const std::string Lingala;
extern const std::string Lithuanian;
extern const std::string LubaKatanga;
extern const std::string Luxembourgish;
extern const std::string Macedonian;
extern const std::string Malagasy;
extern const std::string Malay;
extern const std::string Malayalam;
extern const std::string Maltese;
extern const std::string Manx;
extern const std::string Maori;
extern const std::string Marathi;
extern const std::string Marshallese;
extern const std::string Mongolian;
extern const std::string Nauruan;
extern const std::string Navajo;
extern const std::string NorthNdebele;
extern const std::string SouthNdebele;
extern const std::string Ndonga;
extern const std::string Nepali;
extern const std::string Norwegian;
extern const std::string NorwegianBokmal;
extern const std::string NorwegianNynorsk;
extern const std::string Occitan;
extern const std::string Ojibwa;
extern const std::string Oriya;
extern const std::string Oromo;
extern const std::string Ossetian;
extern const std::string Pali;
extern const std::string Pashto;
extern const std::string Persian;
extern const std::string Polish;
extern const std::string Portuguese;
extern const std::string Punjabi;
extern const std::string Quechua;
extern const std::string Romanian;
extern const std::string Romansh;
extern const std::string Rundi;
extern const std::string Russian;
extern const std::string NorthSami;
extern const std::string Samoan;
extern const std::string Sango;
extern const std::string Sanskrit;
extern const std::string Sardinian;
extern const std::string Serbian;
extern const std::string Shona;
extern const std::string Sindhi;
extern const std::string Sinhala;
extern const std::string Slovak;
extern const std::string Slovenian;
extern const std::string Somali;
extern const std::string SouthSotho;
extern const std::string Spanish;
extern const std::string Sundanese;
extern const std::string Swahili;
extern const std::string Swati;
extern const std::string Swedish;
extern const std::string Tagalog;
extern const std::string Tahitian;
extern const std::string Tajik;
extern const std::string Tamil;
extern const std::string Tatar;
extern const std::string Telugu;
extern const std::string Thai;
extern const std::string Tibetan;
extern const std::string Tigrinya;
extern const std::string Tonga;
extern const std::string Tsonga;
extern const std::string Tswana;
extern const std::string Turkish;
extern const std::string Turkmen;
extern const std::string Twi;
extern const std::string Uighur;
extern const std::string Ukrainian;
extern const std::string Urdu;
extern const std::string Uzbek;
extern const std::string Venda;
extern const std::string Vietnamese;
extern const std::string Volapuk;
extern const std::string Walloon;
extern const std::string Welsh;
extern const std::string Wolof;
extern const std::string Xhosa;
extern const std::string SichuanYi;
extern const std::string Yiddish;
extern const std::string Yoruba;
extern const std::string Zhuang;
extern const std::string Zulu;

// Three-letter codes - Europe languages
extern const std::string Frankish;
extern const std::string Friulian;
extern const std::string Gagauz;
extern const std::string LowGerman; // also Low Saxon
extern const std::string Kashubian;
extern const std::string Picard;
extern const std::string Provencal;
extern const std::string BalkanRomani;
extern const std::string FinnishRomani;
extern const std::string Scots;
extern const std::string Sorbian;

// Three-letter codes - North America languages
extern const std::string Cherokee;
extern const std::string Chipewyan;
extern const std::string Dogrib;
extern const std::string Gwichin;
extern const std::string NorthSlavey;
extern const std::string SouthSlavey;

// Three-letter codes - Latin America languages
extern const std::string Achi;
extern const std::string Akateko;
extern const std::string Awakateko;
extern const std::string ChichimecaJonaz;
extern const std::string Chochoteco;
extern const std::string Chol;
extern const std::string Chontal;
extern const std::string Chorti;
extern const std::string Chuj;
extern const std::string Cocopa;
extern const std::string Diegueno; // a.k.a. Kumiai
extern const std::string ElNayarCora;
extern const std::string SantaTeresaCora;
extern const std::string Garifuna;
extern const std::string Huarijio;
extern const std::string Huasteco;
extern const std::string Huichol;
extern const std::string Itza;
extern const std::string Ixcateco;
extern const std::string Ixil;
extern const std::string Jakalteko;
extern const std::string Kanjobal;
extern const std::string Kaqchiquel;
extern const std::string Kekchi;
extern const std::string Kiche;
extern const std::string Kickapoo; // a.k.a. Kikapu
extern const std::string Kiliwa;
extern const std::string Lacandon;
extern const std::string Mam;
extern const std::string Matlatzinca;
extern const std::string Mayo;
extern const std::string CentralMazahua;
extern const std::string TolucaMazahua;
extern const std::string Mopan;
extern const std::string Motozintleco;
extern const std::string Ocuilteco;
extern const std::string Oodham; // a.k.a. Papago
extern const std::string Otomi;
extern const std::string Paipai;
extern const std::string Plautdietsch; // a.k.a. Low German Mennonite
extern const std::string Pokomam;
extern const std::string Pokomchi;
extern const std::string EastPurepecha;
extern const std::string WestPurepecha;
extern const std::string Sakapulteko;
extern const std::string Seri;
extern const std::string Sipakapense;
extern const std::string Tektiteko;
extern const std::string Tepeuxila; // dialect of Cuicateco
extern const std::string Teutila;   // dialect of Cuicateco
extern const std::string Tojolabal;
extern const std::string Totonaco;
extern const std::string Tzeltal;
extern const std::string Tzotzil;
extern const std::string Tzutujil;
extern const std::string Uspanteko;
extern const std::string Xinka;
extern const std::string Yaqui;
extern const std::string Yucatec; // a.k.a. Maya Yucatan
extern const std::string Zapoteco;

// Three-letter codes - Russian Federation minority languages
extern const std::string Adyghe;
extern const std::string Altai;
extern const std::string Buryat;
extern const std::string Cheremiss; // a.k.a. Mari
extern const std::string Chukchi;
extern const std::string Dolgan;
extern const std::string Erzya;
extern const std::string Evenki;
extern const std::string Ingush;
extern const std::string Kabardian;
extern const std::string Kalmyk;
extern const std::string KarachayBalkar;
extern const std::string Karelian;
extern const std::string Khakas;
extern const std::string Khanty;
extern const std::string Koryak;
extern const std::string Mansi;
extern const std::string Moksha;
extern const std::string Sakha; // a.k.a Yakut
extern const std::string Tuvin;
extern const std::string Udmurt;
extern const std::string Yurak; // a.k.a. Nenets

// Three-letter codes - Africa languages
extern const std::string Angolar;
extern const std::string Dagaare; // a.k.a. Dagaari
extern const std::string Dagbani;
extern const std::string Dangme;
extern const std::string Dinka;
extern const std::string Dyula;
extern const std::string Fanagalo;
extern const std::string Forro;
extern const std::string Ga;
extern const std::string Gonja;
extern const std::string Kabuverdianu; // (see note below)
extern const std::string Kasem;
extern const std::string Khwe;         // a.k.a. Khoe
extern const std::string Krio;
extern const std::string LubaKasai;    // a.k.a. Tshiluba
extern const std::string Mandinka;
extern const std::string Moore;
extern const std::string NorthSotho;   // also covers Lobedu
extern const std::string Nama;         // a.k.a. Khoekhoe
extern const std::string Neuer;        // a.k.a Nuer
extern const std::string Nzema;
extern const std::string Principense;
extern const std::string Soninke;
extern const std::string Zarma;

// NOTE: The ISO-639 Kabuverdianu covers the following Cape Verde languages
// specified in the FreeDOS coumentation:
// - Badiu
// - Boavista
// - Brava
// - Criol d'Soncente
// - Fogo
// - Maio
// - Santo Anto
// - So Nicolau

// Three-letter codes - Africa languages, miscelaneous
extern const std::string Fon;      // a.k.a. Dahomean
extern const std::string Khoisan;  // a.k.a. San
extern const std::string Maore;    // dialect of Shikomor (Comorian)
extern const std::string Mwali;    // dialect of Shikomor (Comorian)
extern const std::string Ndzwani;  // a.k.a. Anjouani, dialect of Shikomor (Comorian)
extern const std::string Ngazidja; // dialect of Shikomor (Comorian)
extern const std::string Tamajeq;

// Three-letter codes - Oceania languages
extern const std::string Chuukese;   // a.k.a Chuuk
extern const std::string Gilbertese; // a.k.a. Kiribati
extern const std::string Hawaiian;
extern const std::string Kosraean;
extern const std::string Niuean;
extern const std::string Palauan;   // also covers Angaur
extern const std::string Pohnpeian; // a.k.a. Ponapean
extern const std::string Sonsoralese;
extern const std::string Tobian;
extern const std::string Tokelauan;
extern const std::string Tuvaluan;
extern const std::string Ulithian;
extern const std::string Yapese;

} // namespace Iso639

#endif // DOSBOX_ISO_LOCALE_CODES_H
