// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iso_locale_codes.h"
#include "utils/checks.h"

CHECK_NARROWING();

// ***************************************************************************
// ISO country/territory definitions
// ***************************************************************************

// clang-format off
namespace Iso3166 {

// International codes
const std::string UnknownState        = "XX";
const std::string InternationalWaters = "XZ";
const std::string OutlyingOceania     = "QO";
const std::string UnitedNations       = "UN";
const std::string Antarctica          = "AQ";
const std::string EuropeanUnion       = "EU";
const std::string EuroZone            = "EZ";

// United States of America and associated
const std::string UnitedStates           = "US";
const std::string UnitedStates_ISRC      = "QM"; // code only used by ISRC
const std::string AmericanSamoa          = "AS";
const std::string Guam                   = "GU";
const std::string JohnstonIsland         = "JT";
const std::string MidwayIslands          = "MI";
const std::string MinorOutlyingIslands   = "UM";
const std::string MiscPacificIslands     = "PU";
const std::string NorthernMarianaIslands = "MP";
const std::string PuertoRico             = "PR";
const std::string VirginIslandsUS        = "VI";
const std::string WakeIsland             = "WK";

// United Kingdom of Great Britain and Northern Ireland and associated
const std::string UnitedKingdom               = "GB";
const std::string UnitedKingdom_Alternative   = "UK"; // alternative code
const std::string Anguilla                    = "AI";
const std::string AscensionIsland             = "AC";
const std::string Bermuda                     = "BM";
const std::string BritishIndianOceanTerritory = "IO";
const std::string CaymanIslands               = "KY";
const std::string DiegoGarcia                 = "DG";
const std::string FalklandIslands             = "FK";
const std::string Gibraltar                   = "GI";
const std::string Guernsey                    = "GG";
const std::string IslandOfSark                = "CQ";
const std::string IsleOfMan                   = "IM";
const std::string Jersey                      = "JE";
const std::string Montserrat                  = "MS";
const std::string NorthernIreland             = "XI";
const std::string Pitcairn                    = "PN";
const std::string SaintHelena                 = "SH";
const std::string SouthGeorgia                = "GS";
const std::string TristanDaCunha              = "TA";
const std::string TurksAndCaicosIslands       = "TC";
const std::string VirginIslandsUK             = "VG";

// France and associated
const std::string France                      = "FR";
const std::string France_Metropolitan         = "FX";
const std::string ClippertonIsland            = "CP";
const std::string FrenchAntarcticTerritories  = "FQ";
const std::string FrenchGuiana                = "GF";
const std::string FrenchPolynesia             = "PF";
const std::string FrenchSouthernTerritories   = "TF";
const std::string Guadeloupe                  = "GP";
const std::string Martinique                  = "MQ";
const std::string Mayotte                     = "YT";
const std::string NewCaledonia                = "NC";
const std::string Reunion                     = "RE";
const std::string SaintBarthelemy             = "BL";
const std::string SaintMartin                 = "MF";
const std::string SaintPierreAndMiquelon      = "PM";
const std::string WallisAndFutuna             = "WF";

// Spain and associated
const std::string Spain                     = "ES";
const std::string CanaryIslands             = "IC";
const std::string CanaryIslands_Alternative = "XA"; // code used in Switzerland
const std::string CeutaAndMelilla           = "EA";

// Netherlands and associated
const std::string Netherlands   = "NL";
const std::string Aruba         = "AW";
// Bonaire, Sint Eustatius and Saba
const std::string Bonaire       = "BQ";
const std::string Curacao       = "CW";
const std::string DutchAntilles = "AN";
const std::string SintMaarten   = "SX";

// Denmark and associated
const std::string Denmark       = "DK";
const std::string FaroeIslands  = "FO";
const std::string Greenland     = "GL";

// Finland and associated
const std::string Finland       = "FI";
const std::string AlandIslands  = "AX";

// Norway and associated
const std::string Norway              = "NO";
const std::string BouvetIsland        = "BV";
const std::string DronningMaudLand    = "NQ";
const std::string SvalbardAndJanMayen = "SJ";

// Australia and associated
const std::string Australia               = "AU";
const std::string ChristmasIsland         = "CX";
const std::string CocosIslands            = "CC";
const std::string HeardAndMcDonaldIslands = "HM";
const std::string NorfolkIsland           = "NF";

// New Zealand and associated
const std::string NewZealand  = "NZ";
const std::string CookIslands = "CK";
const std::string Niue        = "NU";
const std::string Tokelau     = "TK";

// China and associated
const std::string China       = "CN";
const std::string HongKong    = "HK";
const std::string Macao       = "MO";

// Europe
const std::string Albania              = "AL";
const std::string Andorra              = "AD";
const std::string Armenia              = "AM";
const std::string Austria              = "AT";
const std::string Azerbaijan           = "AZ";
const std::string Belarus              = "BY";
const std::string Belgium              = "BE";
const std::string BosniaAndHerzegovina = "BA";
const std::string Bulgaria             = "BG";
const std::string Croatia              = "HR";
const std::string Cyprus               = "CY";
const std::string Czechia              = "CZ";
const std::string Czechoslovakia       = "CS";
const std::string EastGermany          = "DD";
const std::string Estonia              = "EE";
const std::string Georgia              = "GE";
const std::string Germany              = "DE";
const std::string Greece               = "GR";
const std::string Hungary              = "HU";
const std::string Iceland              = "IS";
const std::string Ireland              = "IE";
const std::string Italy                = "IT";
const std::string Kazakhstan           = "KZ";
const std::string Kosovo               = "XK";
const std::string Latvia               = "LV";
const std::string Liechtenstein        = "LI";
const std::string Lithuania            = "LT";
const std::string Luxembourg           = "LU";
const std::string Malta                = "MT";
const std::string Moldova              = "MD";
const std::string Monaco               = "MC";
const std::string Montenegro           = "ME";
const std::string NorthMacedonia       = "MK";
const std::string Poland               = "PL";
const std::string Portugal             = "PT";
const std::string Romania              = "RO";
const std::string Russia               = "RU";
const std::string SanMarino            = "SM";
const std::string Serbia               = "RS";
const std::string Slovakia             = "SK";
const std::string Slovenia             = "SI";
const std::string SovietUnion          = "SU";
const std::string Sweden               = "SE";
const std::string Switzerland          = "CH";
const std::string Turkey               = "TR";
const std::string Ukraine              = "UA";
const std::string VaticanCity          = "VA";
const std::string Yugoslavia           = "YU";

// Asia
const std::string Afghanistan        = "AF";
const std::string Bahrain            = "BH";
const std::string Bangladesh         = "BD";
const std::string Bhutan             = "BT";
const std::string Brunei             = "BN";
const std::string Burma              = "BU";
const std::string Cambodia           = "KH";
const std::string Emirates           = "AE";
const std::string India              = "IN";
const std::string Indonesia          = "ID";
const std::string Iran               = "IR";
const std::string Iraq               = "IQ";
const std::string Israel             = "IL";
const std::string Japan              = "JP";
const std::string Jordan             = "JO";
const std::string Kuwait             = "KW";
const std::string Kyrgyzstan         = "KG";
const std::string Laos               = "LA";
const std::string Lebanon            = "LB";
const std::string Malaysia           = "MY";
const std::string Maldives           = "MV";
const std::string Mongolia           = "MN";
const std::string Myanmar            = "MM";
const std::string Nepal              = "NP";
const std::string NeutralZone        = "NT";
const std::string NorthKorea         = "KP";
const std::string NorthVietnam       = "VD";
const std::string Oman               = "OM";
const std::string Pakistan           = "PK";
const std::string Palestine          = "PS";
const std::string Philippines        = "PH";
const std::string Qatar              = "QA";
const std::string SaudiArabia        = "SA";
const std::string Singapore          = "SG";
const std::string SouthKorea         = "KR";
const std::string SouthYemen         = "YD";
const std::string SriLanka           = "LK";
const std::string Syria              = "SY";
const std::string Taiwan             = "TW";
const std::string Tajikistan         = "TJ";
const std::string Thailand           = "TH";
const std::string Turkmenistan       = "TM";
const std::string Uzbekistan         = "UZ";
const std::string Yemen              = "YE";
const std::string Vietnam            = "VN";

// Africa
const std::string Algeria                = "DZ";
const std::string Angola                 = "AO";
const std::string Benin                  = "BJ";
const std::string Botswana               = "BW";
const std::string BurkinaFaso            = "BF";
const std::string Burundi                = "BI";
const std::string CaboVerde              = "CV";
const std::string Cameroon               = "CM";
const std::string CentralAfricanRepublic = "CF";
const std::string Chad                   = "TD";
const std::string Comoros                = "KM";
const std::string Congo                  = "CG";
const std::string CongoKinshasa          = "CD";
const std::string Dahomey                = "DY";
const std::string Djibouti               = "DJ";
const std::string Egypt                  = "EG";
const std::string EquatorialGuinea       = "GQ";
const std::string Eritrea                = "ER";
const std::string Eswatini               = "SZ";
const std::string Ethiopia               = "ET";
const std::string Gabon                  = "GA";
const std::string Gambia                 = "GM";
const std::string Ghana                  = "GH";
const std::string Guinea                 = "GN";
const std::string GuineaBissau           = "GW";
const std::string IvoryCoast             = "CI";
const std::string Kenya                  = "KE";
const std::string Lesotho                = "LS";
const std::string Liberia                = "LR";
const std::string Libya                  = "LY";
const std::string Mali                   = "ML";
const std::string Madagascar             = "MG";
const std::string Malawi                 = "MW";
const std::string Mauritania             = "MR";
const std::string Mauritius              = "MU";
const std::string Morocco                = "MA";
const std::string Mozambique             = "MZ";
const std::string Namibia                = "NA";
const std::string Niger                  = "NE";
const std::string Nigeria                = "NG";
const std::string Rwanda                 = "RW";
const std::string SaoTomeAndPrincipe     = "ST";
const std::string Senegal                = "SN";
const std::string Seychelles             = "SC";
const std::string SierraLeone            = "SL";
const std::string Somalia                = "SO";
const std::string SouthAfrica            = "ZA";
const std::string SouthSudan             = "SS";
const std::string SouthernRhodesia       = "RH";
const std::string Sudan                  = "SD";
const std::string Tanzania               = "TZ";
const std::string Togo                   = "TG";
const std::string Tunisia                = "TN";
const std::string Uganda                 = "UG";
const std::string UpperVolta             = "HV";
const std::string WesternSahara          = "EH";
const std::string Zaire                  = "ZR";
const std::string Zambia                 = "ZM";
const std::string Zimbabwe               = "ZW";

// Americas
const std::string AntiguaAndBarbuda         = "AG";
const std::string Argentina                 = "AR";
const std::string Bahamas                   = "BS";
const std::string Barbados                  = "BB";
const std::string Belize                    = "BZ";
const std::string Bolivia                   = "BO";
const std::string Brazil                    = "BR";
const std::string Canada                    = "CA";
const std::string Chile                     = "CL";
const std::string Colombia                  = "CO";
const std::string CostaRica                 = "CR";
const std::string Cuba                      = "CU";
const std::string Dominica                  = "DM";
const std::string Dominicana                = "DO";
const std::string Ecuador                   = "EC";
const std::string ElSalvador                = "SV";
const std::string Grenada                   = "GD";
const std::string Guatemala                 = "GT";
const std::string Guyana                    = "GY";
const std::string Haiti                     = "HT";
const std::string Honduras                  = "HN";
const std::string Jamaica                   = "JM";
const std::string Mexico                    = "MX";
const std::string Nicaragua                 = "NI";
const std::string Panama                    = "PA";
const std::string PanamaCanalZone           = "PZ";
const std::string Paraguay                  = "PY";
const std::string Peru                      = "PE";
const std::string SaintKittsAndNevis        = "KN";
const std::string SaintLucia                = "LC";
const std::string SaintVincentAndGrenadines = "VC";
const std::string Suriname                  = "SR";
const std::string TrinidadAndTobago         = "TT";
const std::string Uruguay                   = "UY";
const std::string Venezuela                 = "VE";

// Oceania
const std::string CantonAndEnderburyIslands = "CT";
const std::string EastTimor                 = "TP";
const std::string Fiji                      = "FJ";
const std::string Kiribati                  = "KI";
const std::string MarshallIslands           = "MH";
const std::string Micronesia                = "FM";
const std::string Nauru                     = "NR";
const std::string NewHebrides               = "NH";
const std::string Palau                     = "PW";
const std::string PapuaNewGuinea            = "PG";
const std::string Samoa                     = "WS";
const std::string SolomonIslands            = "SB";
const std::string TimorLeste                = "TL";
const std::string Tonga                     = "TO";
const std::string Tuvalu                    = "TV";
const std::string Vanuatu                   = "VU";

}
// clang-format on


// ***************************************************************************
// ISO language definitions
// ***************************************************************************

// clang-format off
namespace Iso639 {

const std::string English          = "en";

// Two-letter codes
const std::string Abkhazian        = "ab";
const std::string Afar             = "aa";
const std::string Afrikaans        = "af";
const std::string Akan             = "ak";
const std::string Albanian         = "sq";
const std::string Amharic          = "am";
const std::string Arabic           = "ar";
const std::string Aragonese        = "an";
const std::string Armenian         = "hy";
const std::string Assamese         = "as";
const std::string Avaric           = "av";
const std::string Avestan          = "ae";
const std::string Aymara           = "ay";
const std::string Azerbaijani      = "az";
const std::string Bambara          = "bm";
const std::string Bashkir          = "ba";
const std::string Basque           = "eu";
const std::string Belarusian       = "be";
const std::string Bengali          = "bn";
const std::string Bislama          = "bi";
const std::string Bosnian          = "bs";
const std::string Breton           = "br";
const std::string Bulgarian        = "bg";
const std::string Burmese          = "my";
const std::string Catalan          = "ca";
const std::string Chamorro         = "ch";
const std::string Chechen          = "ce";
const std::string Chichewa         = "ny";
const std::string Chinese          = "zh";
const std::string ChurchSlavonic   = "cu";
const std::string Chuvash          = "cv";
const std::string Cornish          = "kw";
const std::string Corsican         = "co";
const std::string Cree             = "cr";
const std::string Croatian         = "hr";
const std::string Czech            = "cs";
const std::string Danish           = "da";
const std::string Divehi           = "dv";
const std::string Dutch            = "nl";
const std::string Dzongkha         = "dz";
const std::string Esperanto        = "eo";
const std::string Estonian         = "et";
const std::string Ewe              = "ee";
const std::string Faroese          = "fo";
const std::string Fijian           = "fj";
const std::string Finnish          = "fi";
const std::string French           = "fr";
const std::string WesternFrisian   = "fy";
const std::string Fulah            = "ff";
const std::string Gaelic           = "gd";
const std::string Galician         = "gl";
const std::string Ganda            = "lg";
const std::string Georgian         = "ka";
const std::string German           = "de";
const std::string Greek            = "el";
const std::string Greenlandic      = "kl";
const std::string Guarani          = "gn";
const std::string Gujarati         = "gu";
const std::string Haitian          = "ht";
const std::string Hausa            = "ha";
const std::string Hebrew           = "he";
const std::string Herero           = "hz";
const std::string Hindi            = "hi";
const std::string HiriMotu         = "ho";
const std::string Hungarian        = "hu";
const std::string Icelandic        = "is";
const std::string Ido              = "io";
const std::string Igbo             = "ig";
const std::string Indonesian       = "id";
const std::string Interlingua      = "ia";
const std::string Interlingue      = "ie";
const std::string Inuktitut        = "iu";
const std::string Inupiaq          = "ik";
const std::string Irish            = "ga";
const std::string Italian          = "it";
const std::string Japanese         = "ja";
const std::string Javanese         = "jv";
const std::string Kannada          = "kn";
const std::string Kanuri           = "kr";
const std::string Kashmiri         = "ks";
const std::string Kazakh           = "kk";
const std::string Khmer            = "km";
const std::string Kikuyu           = "ki";
const std::string Kinyarwanda      = "rw";
const std::string Kyrgyz           = "ky";
const std::string Komi             = "kv";
const std::string Kongo            = "kg";
const std::string Korean           = "ko";
const std::string Kuanyama         = "kj";
const std::string Kurdish          = "ku";
const std::string Lao              = "lo";
const std::string Latin            = "la";
const std::string Latvian          = "lv";
const std::string Limburgan        = "li"; // a.k.a. Limburgish
const std::string Lingala          = "ln";
const std::string Lithuanian       = "lt";
const std::string LubaKatanga      = "lu";
const std::string Luxembourgish    = "lb";
const std::string Macedonian       = "mk";
const std::string Malagasy         = "mg";
const std::string Malay            = "ms";
const std::string Malayalam        = "ml";
const std::string Maltese          = "mt";
const std::string Manx             = "gv";
const std::string Maori            = "mi";
const std::string Marathi          = "mr";
const std::string Marshallese      = "mh";
const std::string Mongolian        = "mn";
const std::string Nauruan          = "na";
const std::string Navajo           = "nv";
const std::string NorthNdebele     = "nd";
const std::string SouthNdebele     = "nr";
const std::string Ndonga           = "ng";
const std::string Nepali           = "ne";
const std::string Norwegian        = "no";
const std::string NorwegianBokmal  = "nb";
const std::string NorwegianNynorsk = "nn";
const std::string Occitan          = "oc";
const std::string Ojibwa           = "oj";
const std::string Oriya            = "or";
const std::string Oromo            = "om";
const std::string Ossetian         = "os";
const std::string Pali             = "pi";
const std::string Pashto           = "ps";
const std::string Persian          = "fa";
const std::string Polish           = "pl";
const std::string Portuguese       = "pt";
const std::string Punjabi          = "pa";
const std::string Quechua          = "qu";
const std::string Romanian         = "ro";
const std::string Romansh          = "rm";
const std::string Rundi            = "rn";
const std::string Russian          = "ru";
const std::string NorthSami        = "se";
const std::string Samoan           = "sm";
const std::string Sango            = "sg";
const std::string Sanskrit         = "sa";
const std::string Sardinian        = "sc";
const std::string Serbian          = "sr";
const std::string Shona            = "sn";
const std::string Sindhi           = "sd";
const std::string Sinhala          = "si";
const std::string Slovak           = "sk";
const std::string Slovenian        = "sl";
const std::string Somali           = "so";
const std::string SouthSotho       = "st";
const std::string Spanish          = "es";
const std::string Sundanese        = "su";
const std::string Swahili          = "sw";
const std::string Swati            = "ss";
const std::string Swedish          = "sv";
const std::string Tagalog          = "tl";
const std::string Tahitian         = "ty";
const std::string Tajik            = "tg";
const std::string Tamil            = "ta";
const std::string Tatar            = "tt";
const std::string Telugu           = "te";
const std::string Thai             = "th";
const std::string Tibetan          = "bo";
const std::string Tigrinya         = "ti";
const std::string Tonga            = "to";
const std::string Tsonga           = "ts";
const std::string Tswana           = "tn";
const std::string Turkish          = "tr";
const std::string Turkmen          = "tk";
const std::string Twi              = "tw";
const std::string Uighur           = "ug";
const std::string Ukrainian        = "uk";
const std::string Urdu             = "ur";
const std::string Uzbek            = "uz";
const std::string Venda            = "ve";
const std::string Vietnamese       = "vi";
const std::string Volapuk          = "vo";
const std::string Walloon          = "wa";
const std::string Welsh            = "cy";
const std::string Wolof            = "wo";
const std::string Xhosa            = "xh";
const std::string SichuanYi        = "ii";
const std::string Yiddish          = "yi";
const std::string Yoruba           = "yo";
const std::string Zhuang           = "za";
const std::string Zulu             = "zu";

// Three-letter codes - Europe languages
const std::string Frankish         = "frk";
const std::string Friulian         = "fur";
const std::string Gagauz           = "gag";
const std::string LowGerman        = "nds";
const std::string Kashubian        = "csb";
const std::string Picard           = "pcd";
const std::string Provencal        = "prv";
const std::string BalkanRomani     = "rmn";
const std::string FinnishRomani    = "rmf";
const std::string Scots            = "sco";
const std::string Sorbian          = "wen";

// Three-letter codes - North America languages
const std::string Cherokee         = "chr";
const std::string Chipewyan        = "chp";
const std::string Dogrib           = "dgr";
const std::string Gwichin          = "gwi";
const std::string NorthSlavey      = "scs";
const std::string SouthSlavey      = "xsl";

// Three-letter codes - Latin America languages
const std::string Achi             = "acr";
const std::string Akateko          = "knj";
const std::string Awakateko        = "agu";
const std::string ChichimecaJonaz  = "pei";
const std::string Chochoteco       = "coz";
const std::string Chol             = "ctu";
const std::string Chontal          = "chf";
const std::string Chorti           = "caa";
const std::string Chuj             = "cac";
const std::string Cocopa           = "coc";
const std::string ElNayarCora      = "crn";
const std::string SantaTeresaCora  = "cok";
const std::string Diegueno         = "dih";
const std::string Garifuna         = "cab";
const std::string Huarijio         = "var";
const std::string Huasteco         = "hus";
const std::string Huichol          = "hch";
const std::string Itza             = "itz";
const std::string Ixcateco         = "ixc";
const std::string Ixil             = "ixl";
const std::string Jakalteko        = "jac";
const std::string Kanjobal         = "kjb";
const std::string Kaqchiquel       = "cak";
const std::string Kekchi           = "kek";
const std::string Kiche            = "quc";
const std::string Kickapoo         = "kic";
const std::string Kiliwa           = "klb";
const std::string Lacandon         = "lac";
const std::string Mam              = "mam";
const std::string Matlatzinca      = "mat";
const std::string Mayo             = "mfy";
const std::string CentralMazahua   = "maz";
const std::string TolucaMazahua    = "mmc";
const std::string Mopan            = "mop";
const std::string Motozintleco     = "mhc";
const std::string Ocuilteco        = "ocu";
const std::string Oodham           = "ood";
const std::string Otomi            = "oto";
const std::string Paipai           = "ppi";
const std::string Plautdietsch     = "pdt";
const std::string Pokomam          = "poc";
const std::string Pokomchi         = "poh";
const std::string EastPurepecha    = "tsz";
const std::string WestPurepecha    = "pua";
const std::string Sakapulteko      = "quv";
const std::string Seri             = "sei";
const std::string Sipakapense      = "qum";
const std::string Tektiteko        = "ttc";
const std::string Tepeuxila        = "cux";
const std::string Teutila          = "cut";
const std::string Tojolabal        = "toj";
const std::string Totonaco         = "toc";
const std::string Tzeltal          = "tzh";
const std::string Tzotzil          = "tzo";
const std::string Tzutujil         = "tzj";
const std::string Uspanteko        = "usp";
const std::string Xinka            = "xin";
const std::string Yaqui            = "yaq";
const std::string Yucatec          = "yua";
const std::string Zapoteco         = "zap";

// Three-letter codes - Russian Federation minority languages
const std::string Adyghe           = "ady";
const std::string Altai            = "alt";
const std::string Buryat           = "bua";
const std::string Cheremiss        = "chm";
const std::string Chukchi          = "ckt";
const std::string Dolgan           = "dlg";
const std::string Erzya            = "myv";
const std::string Evenki           = "evn";
const std::string Ingush           = "inh";
const std::string Kabardian        = "kbd";
const std::string Kalmyk           = "xal";
const std::string KarachayBalkar   = "krc";
const std::string Karelian         = "krl";
const std::string Khakas           = "kjh";
const std::string Khanty           = "kca";
const std::string Koryak           = "kpy";
const std::string Mansi            = "mns";
const std::string Moksha           = "mdf";
const std::string Sakha            = "sah";
const std::string Tuvin            = "tyv";
const std::string Udmurt           = "udm";
const std::string Yurak            = "yrk";

// Three-letter codes - Africa languages
const std::string Angolar          = "aoa";
const std::string Dagaare          = "dga";
const std::string Dagbani          = "dag";
const std::string Dangme           = "ada";
const std::string Dinka            = "din";
const std::string Dyula            = "dyu";
const std::string Fanagalo         = "fng";
const std::string Forro            = "cri";
const std::string Ga               = "gaa";
const std::string Gonja            = "gjn";
const std::string Kabuverdianu     = "kea";
const std::string Kasem            = "xsm";
const std::string Khwe             = "xuu";
const std::string Krio             = "kri";
const std::string LubaKasai        = "lua";
const std::string Mandinka         = "mnk";
const std::string Moore            = "mos";
const std::string NorthSotho       = "nso";
const std::string Nama             = "naq";
const std::string Neuer            = "nus";
const std::string Nzema            = "nzi";
const std::string Principense      = "pre";
const std::string Soninke          = "snk";
const std::string Zarma            = "dje";

// Three-letter codes - Africa languages, miscelaneous
const std::string Fon              = "fon";
const std::string Khoisan          = "khi";
const std::string Maore            = "swb";
const std::string Mwali            = "wlc";
const std::string Ndzwani          = "wni";
const std::string Ngazidja         = "zdj";
const std::string Tamajeq          = "thz";

// Three-letter codes - Oceania languages
const std::string Chuukese         = "chk";
const std::string Gilbertese       = "gil";
const std::string Hawaiian         = "haw";
const std::string Kosraean         = "kos";
const std::string Niuean           = "niu";
const std::string Palauan          = "pau";
const std::string Pohnpeian        = "pon";
const std::string Sonsoralese      = "sov";
const std::string Tobian           = "tox";
const std::string Tokelauan        = "tkl";
const std::string Tuvaluan         = "tvl";
const std::string Ulithian         = "uli";
const std::string Yapese           = "yap";

}
// clang-format on
