// SPDX-FileCopyrightText:  2024-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "host_locale.h"

#include "iso_locale_codes.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

CHECK_NARROWING();

// ***************************************************************************
// ISO country/territory to DOS mapping data
// ***************************************************************************

// Mapping from the ISO format data to DOS country codes

// clang-format off
static const std::unordered_map<std::string, DosCountry> IsoToDosCountryMap = {

	// International codes
	{ Iso3166::UnknownState,        DosCountry::International },
	{ Iso3166::InternationalWaters, DosCountry::International },
	{ Iso3166::OutlyingOceania,     DosCountry::International },
	{ Iso3166::UnitedNations,       DosCountry::International },
	{ Iso3166::Antarctica,          DosCountry::International },
	{ Iso3166::EuropeanUnion,       DosCountry::International },
	{ Iso3166::EuroZone,            DosCountry::International },

	// United States of America and associated
	{ Iso3166::UnitedStates,           DosCountry::UnitedStates },
	{ Iso3166::UnitedStates_ISRC,      DosCountry::UnitedStates },
	{ Iso3166::AmericanSamoa,          DosCountry::UnitedStates },
	{ Iso3166::Guam,                   DosCountry::UnitedStates },
	{ Iso3166::JohnstonIsland,         DosCountry::UnitedStates },
	{ Iso3166::MidwayIslands,          DosCountry::UnitedStates },
	{ Iso3166::MinorOutlyingIslands,   DosCountry::UnitedStates },
	{ Iso3166::MiscPacificIslands,     DosCountry::UnitedStates },
	{ Iso3166::NorthernMarianaIslands, DosCountry::UnitedStates },
	{ Iso3166::PuertoRico,             DosCountry::PuertoRico   },
	{ Iso3166::VirginIslandsUS,        DosCountry::UnitedStates },
	{ Iso3166::WakeIsland,             DosCountry::UnitedStates },

	// Canada (English/French)
	{ Iso3166::Canada,                        DosCountry::CanadaEnglish },
	{ Iso639::French + "_" + Iso3166::Canada, DosCountry::CanadaFrench  },

	// United Kingdom of Great Britain and Northern Ireland and associated
	{ Iso3166::UnitedKingdom,               DosCountry::UnitedKingdom },
	{ Iso3166::UnitedKingdom_Alternative,   DosCountry::UnitedKingdom },
	{ Iso3166::AscensionIsland,             DosCountry::UnitedKingdom },
	{ Iso3166::Anguilla,                    DosCountry::LatinAmerica  },
	{ Iso3166::Bermuda,                     DosCountry::LatinAmerica  },
	{ Iso3166::BritishIndianOceanTerritory, DosCountry::UnitedKingdom },
	{ Iso3166::CaymanIslands,               DosCountry::LatinAmerica  },
	{ Iso3166::DiegoGarcia,                 DosCountry::UnitedKingdom },
	{ Iso3166::FalklandIslands,             DosCountry::LatinAmerica  },
	{ Iso3166::Gibraltar,                   DosCountry::UnitedKingdom },
	{ Iso3166::Guernsey,                    DosCountry::UnitedKingdom },
	{ Iso3166::IslandOfSark,                DosCountry::UnitedKingdom },
	{ Iso3166::IsleOfMan,                   DosCountry::UnitedKingdom },
	{ Iso3166::Jersey,                      DosCountry::UnitedKingdom },
	{ Iso3166::Montserrat,                  DosCountry::LatinAmerica  },
	{ Iso3166::NorthernIreland,             DosCountry::UnitedKingdom },
	{ Iso3166::Pitcairn,                    DosCountry::UnitedKingdom },
	{ Iso3166::SaintHelena,                 DosCountry::UnitedKingdom },
	{ Iso3166::SouthGeorgia,                DosCountry::UnitedKingdom },
	{ Iso3166::TristanDaCunha,              DosCountry::UnitedKingdom },
	{ Iso3166::TurksAndCaicosIslands,       DosCountry::LatinAmerica  },
	{ Iso3166::VirginIslandsUK,             DosCountry::UnitedKingdom },

	// France and associated
	{ Iso3166::France,                     DosCountry::France },
	{ Iso3166::France_Metropolitan,        DosCountry::France },
	{ Iso3166::ClippertonIsland,           DosCountry::France },
	{ Iso3166::FrenchAntarcticTerritories, DosCountry::France },
	{ Iso3166::FrenchGuiana,               DosCountry::France },
	{ Iso3166::FrenchPolynesia,            DosCountry::France },
	{ Iso3166::FrenchSouthernTerritories,  DosCountry::France },
	{ Iso3166::Guadeloupe,                 DosCountry::France },
	{ Iso3166::Martinique,                 DosCountry::France },
	{ Iso3166::Mayotte,                    DosCountry::France },
	{ Iso3166::NewCaledonia,               DosCountry::France },
	{ Iso3166::Reunion,                    DosCountry::France },
	{ Iso3166::SaintBarthelemy,            DosCountry::France },
	{ Iso3166::SaintMartin,                DosCountry::France },
	{ Iso3166::SaintPierreAndMiquelon,     DosCountry::France },
	{ Iso3166::WallisAndFutuna,            DosCountry::France },

	// Spain and associated
	{ Iso3166::Spain,                      DosCountry::Spain },
	{ Iso3166::CanaryIslands,              DosCountry::Spain },
	{ Iso3166::CanaryIslands_Alternative,  DosCountry::Spain },
	{ Iso3166::CeutaAndMelilla,            DosCountry::Spain },

	// Netherlands and associated
	{ Iso3166::Netherlands,   DosCountry::Netherlands  },
	{ Iso3166::Aruba,         DosCountry::Aruba        },
	{ Iso3166::Bonaire,       DosCountry::Netherlands  },
	{ Iso3166::Curacao,       DosCountry::LatinAmerica },
	{ Iso3166::DutchAntilles, DosCountry::LatinAmerica },
	{ Iso3166::SintMaarten,   DosCountry::Netherlands  },

	// Denmark and associated
	{ Iso3166::Denmark,       DosCountry::Denmark      },
	{ Iso3166::FaroeIslands,  DosCountry::FaroeIslands },
	{ Iso3166::Greenland,     DosCountry::Greenland    },

	// Finland and associated
	{ Iso3166::Finland,       DosCountry::Finland      },
	{ Iso3166::AlandIslands,  DosCountry::Finland      },

	// Norway and associated
	{ Iso3166::Norway,              DosCountry::Norway },
	{ Iso3166::BouvetIsland,        DosCountry::Norway },
	{ Iso3166::DronningMaudLand,    DosCountry::Norway },
	{ Iso3166::SvalbardAndJanMayen, DosCountry::Norway },

	// Australia and associated
	{ Iso3166::Australia,               DosCountry::Australia },
	{ Iso3166::ChristmasIsland,         DosCountry::Australia },
	{ Iso3166::CocosIslands,            DosCountry::Australia },
	{ Iso3166::HeardAndMcDonaldIslands, DosCountry::Australia },
	{ Iso3166::NorfolkIsland,           DosCountry::Australia },

	// New Zealand and associated
	{ Iso3166::NewZealand,  DosCountry::NewZealand },
	{ Iso3166::CookIslands, DosCountry::NewZealand },
	{ Iso3166::Niue,        DosCountry::NewZealand },
	{ Iso3166::Tokelau,     DosCountry::NewZealand },

	// China and associated
	{ Iso3166::China,       DosCountry::China    },
	{ Iso3166::HongKong,    DosCountry::HongKong },
	{ Iso3166::Macao,       DosCountry::China    },

	// Europe
	{ Iso3166::Albania,              DosCountry::Albania        },
	{ Iso3166::Andorra,              DosCountry::Andorra        },
	{ Iso3166::Armenia,              DosCountry::Armenia        },
	{ Iso3166::Austria,              DosCountry::Austria        },
	{ Iso3166::Azerbaijan,           DosCountry::Azerbaijan     },
	{ Iso3166::Belarus,              DosCountry::Belarus        },
	{ Iso3166::Belgium,              DosCountry::Belgium        },
	{ Iso3166::BosniaAndHerzegovina, DosCountry::BosniaLatin    },
	// TODO: Find a way to detect DosCountry::BosniaCyrillic
	{ Iso3166::Bulgaria,             DosCountry::Bulgaria       },
	{ Iso3166::Croatia,              DosCountry::Croatia        },
	{ Iso3166::Czechia,              DosCountry::Czechia        },
	{ Iso3166::Czechoslovakia,       DosCountry::Czechia        },
	{ Iso3166::EastGermany,          DosCountry::Germany        },
	{ Iso3166::Estonia,              DosCountry::Estonia        },
	{ Iso3166::Georgia,              DosCountry::Georgia        },
	{ Iso3166::Germany,              DosCountry::Germany        },
	{ Iso3166::Greece,               DosCountry::Greece         },
	{ Iso3166::Hungary,              DosCountry::Hungary        },
	{ Iso3166::Iceland,              DosCountry::Iceland        },
	{ Iso3166::Ireland,              DosCountry::Ireland        },
	{ Iso3166::Italy,                DosCountry::Italy          },
	{ Iso3166::Kazakhstan,           DosCountry::Kazakhstan     },
	{ Iso3166::Latvia,               DosCountry::Latvia         },
	{ Iso3166::Liechtenstein,        DosCountry::Liechtenstein  },
	{ Iso3166::Lithuania,            DosCountry::Lithuania      },
	{ Iso3166::Luxembourg,           DosCountry::Luxembourg     },
	{ Iso3166::Malta,                DosCountry::Malta          },
	{ Iso3166::Monaco,               DosCountry::Monaco         },
	{ Iso3166::Montenegro,           DosCountry::Montenegro     },
	{ Iso3166::NorthMacedonia,       DosCountry::NorthMacedonia },
	{ Iso3166::Poland,               DosCountry::Poland         },
	{ Iso3166::Portugal,             DosCountry::Portugal       },
	{ Iso3166::Romania,              DosCountry::Romania        },
	{ Iso3166::Russia,               DosCountry::Russia         },
	{ Iso3166::SanMarino,            DosCountry::SanMarino      },
	{ Iso3166::Serbia,               DosCountry::Serbia         },
	{ Iso3166::Slovakia,             DosCountry::Slovakia       },
	{ Iso3166::Slovenia,             DosCountry::Slovenia       },
	{ Iso3166::SovietUnion,          DosCountry::Russia         },
	{ Iso3166::Sweden,               DosCountry::Sweden         },
	{ Iso3166::Switzerland,          DosCountry::Switzerland    },
	{ Iso3166::Turkey,               DosCountry::Turkey         },
	{ Iso3166::Ukraine,              DosCountry::Ukraine        },
	{ Iso3166::VaticanCity,          DosCountry::VaticanCity    },
	{ Iso3166::Yugoslavia,           DosCountry::Yugoslavia     },

	// Asia
	{ Iso3166::Bahrain,            DosCountry::Bahrain      },
	{ Iso3166::Emirates,           DosCountry::Emirates     },
	{ Iso3166::India,              DosCountry::India        },
	{ Iso3166::Indonesia,          DosCountry::Indonesia    },
	{ Iso3166::Israel,             DosCountry::Israel       },
	{ Iso3166::Japan,              DosCountry::Japan        },
	{ Iso3166::Jordan,             DosCountry::Jordan       },
	{ Iso3166::Kuwait,             DosCountry::Kuwait       },
	{ Iso3166::Kyrgyzstan,         DosCountry::Kyrgyzstan   },
	{ Iso3166::Lebanon,            DosCountry::Lebanon      },
	{ Iso3166::Malaysia,           DosCountry::Malaysia     },
	{ Iso3166::Mongolia,           DosCountry::Mongolia     },
	{ Iso3166::NorthVietnam,       DosCountry::Vietnam      },
	{ Iso3166::Oman,               DosCountry::Oman         },
	{ Iso3166::Pakistan,           DosCountry::Pakistan     },
	{ Iso3166::Philippines,        DosCountry::Philippines  },
	{ Iso3166::Qatar,              DosCountry::Qatar        },
	{ Iso3166::SaudiArabia,        DosCountry::SaudiArabia  },
	{ Iso3166::Singapore,          DosCountry::Singapore    },
	{ Iso3166::SouthKorea,         DosCountry::SouthKorea   },
	{ Iso3166::SouthYemen,         DosCountry::Yemen        },
	{ Iso3166::Syria,              DosCountry::Syria        },
	{ Iso3166::Taiwan,             DosCountry::Taiwan       },
	{ Iso3166::Tajikistan,         DosCountry::Tajikistan   },
	{ Iso3166::Thailand,           DosCountry::Thailand     },
	{ Iso3166::Turkmenistan,       DosCountry::Turkmenistan },
	{ Iso3166::Uzbekistan,         DosCountry::Uzbekistan   },
	{ Iso3166::Yemen,              DosCountry::Yemen        },
	{ Iso3166::Vietnam,            DosCountry::Vietnam      },

	// Africa
	{ Iso3166::Algeria,          DosCountry::Algeria     },
	{ Iso3166::Benin,            DosCountry::Benin       },
	{ Iso3166::Botswana,         DosCountry::Botswana    },
	{ Iso3166::Congo,            DosCountry::Congo       },
	{ Iso3166::Dahomey,          DosCountry::Benin       },
	{ Iso3166::Egypt,            DosCountry::Egypt       },
	{ Iso3166::Eritrea,          DosCountry::Eritrea     },
	{ Iso3166::Ghana,            DosCountry::Ghana       },
	{ Iso3166::Kenya,            DosCountry::Kenya       },
	{ Iso3166::Madagascar,       DosCountry::Madagascar  },
	{ Iso3166::Morocco,          DosCountry::Morocco     },
	{ Iso3166::Niger,            DosCountry::Niger       },
	{ Iso3166::Nigeria,          DosCountry::Nigeria     },
	{ Iso3166::Rwanda,           DosCountry::Rwanda      },
	{ Iso3166::Senegal,          DosCountry::Senegal     },
	{ Iso3166::Seychelles,       DosCountry::Seychelles  },
	{ Iso3166::SouthAfrica,      DosCountry::SouthAfrica },
	{ Iso3166::SouthernRhodesia, DosCountry::Zimbabwe    },
	{ Iso3166::Tanzania,         DosCountry::Tanzania    },
	{ Iso3166::Tunisia,          DosCountry::Tunisia     },
	{ Iso3166::Uganda,           DosCountry::Uganda      },
	{ Iso3166::Zambia,           DosCountry::Zambia      },
	{ Iso3166::Zimbabwe,         DosCountry::Zimbabwe    },

	// Americas
	{ Iso3166::Argentina,       DosCountry::Argentina  },
	{ Iso3166::Bolivia,         DosCountry::Bolivia    },
	{ Iso3166::Brazil,          DosCountry::Brazil     },
	{ Iso3166::Chile,           DosCountry::Chile      },
	{ Iso3166::Colombia,        DosCountry::Colombia   },
	{ Iso3166::CostaRica,       DosCountry::CostaRica  },
	{ Iso3166::Cuba,            DosCountry::Cuba       },
	{ Iso3166::Ecuador,         DosCountry::Ecuador    },
	{ Iso3166::ElSalvador,      DosCountry::ElSalvador },
	{ Iso3166::Guatemala,       DosCountry::Guatemala  },
	{ Iso3166::Haiti,           DosCountry::Haiti      },
	{ Iso3166::Honduras,        DosCountry::Honduras   },
	{ Iso3166::Mexico,          DosCountry::Mexico     },
	{ Iso3166::Nicaragua,       DosCountry::Nicaragua  },
	{ Iso3166::Panama,          DosCountry::Panama     },
	{ Iso3166::PanamaCanalZone, DosCountry::Panama     },
	{ Iso3166::Paraguay,        DosCountry::Paraguay   },
	{ Iso3166::Peru,            DosCountry::Peru       },
	{ Iso3166::Uruguay,         DosCountry::Uruguay    },
	{ Iso3166::Venezuela,       DosCountry::Venezuela  },

	// Oceania
	{ Iso3166::Tonga, DosCountry::Tonga },

	// We do not have DOS country codes for these territories - but they
	// are feasible to be assigned to the generic Asia English country code,
	// as English is one of the main language spoken there
	{ Iso3166::Bangladesh, DosCountry::AsiaEnglish },
	{ Iso3166::Bhutan,     DosCountry::AsiaEnglish },
	{ Iso3166::Maldives,   DosCountry::AsiaEnglish },
	{ Iso3166::SriLanka,   DosCountry::AsiaEnglish },

	// We do not have DOS country codes for these territories - but they
	// are feasible to be assigned to the generic Arabic country code
	{ Iso3166::Afghanistan,   DosCountry::Arabic },
	{ Iso3166::Brunei,        DosCountry::Arabic },
	{ Iso3166::Chad,          DosCountry::Arabic },
	{ Iso3166::Djibouti,      DosCountry::Arabic },
	{ Iso3166::Iran,          DosCountry::Arabic },
	{ Iso3166::Iraq,          DosCountry::Arabic },
	{ Iso3166::Libya,         DosCountry::Arabic },
	{ Iso3166::Mauritania,    DosCountry::Arabic },
	{ Iso3166::NeutralZone,   DosCountry::Arabic },
	{ Iso3166::Palestine,     DosCountry::Arabic },
	{ Iso3166::Sudan,         DosCountry::Arabic },
	{ Iso3166::Somalia,       DosCountry::Arabic },
	{ Iso3166::WesternSahara, DosCountry::Arabic },

	// We do not have DOS country codes for these territories - but they
	// are feasible to be assigned to the generic Latin America country code
	{ Iso3166::AntiguaAndBarbuda,         DosCountry::LatinAmerica },
	{ Iso3166::Bahamas,                   DosCountry::LatinAmerica },
	{ Iso3166::Barbados,                  DosCountry::LatinAmerica },
	{ Iso3166::Belize,                    DosCountry::LatinAmerica },
	{ Iso3166::Dominica,                  DosCountry::LatinAmerica },
	{ Iso3166::Dominicana,                DosCountry::LatinAmerica },
	{ Iso3166::Grenada,                   DosCountry::LatinAmerica },
	{ Iso3166::Guyana,                    DosCountry::LatinAmerica },
	{ Iso3166::Jamaica,                   DosCountry::LatinAmerica },
	{ Iso3166::SaintKittsAndNevis,        DosCountry::LatinAmerica },
	{ Iso3166::SaintLucia,                DosCountry::LatinAmerica },
	{ Iso3166::PapuaNewGuinea,            DosCountry::LatinAmerica },
	{ Iso3166::SaintVincentAndGrenadines, DosCountry::LatinAmerica },
	{ Iso3166::Suriname,                  DosCountry::LatinAmerica },
	{ Iso3166::TrinidadAndTobago,         DosCountry::LatinAmerica },

	// Asian countries with English language is selected
	{ Iso639::English + "_" + Iso3166::Afghanistan,  DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Armenia,      DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Azerbaijan,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Bahrain,      DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Brunei,       DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Burma,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Cambodia,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::China,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::EastTimor,    DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Emirates,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Georgia,      DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::HongKong,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::India,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Indonesia,    DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Iran,         DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Iraq,         DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Israel,       DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Japan,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Jordan,       DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Kazakhstan,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Kuwait,       DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Kyrgyzstan,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Laos,         DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Lebanon,      DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Macao,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Malaysia,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Mongolia,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Myanmar,      DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Nepal,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::NorthKorea,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::NorthVietnam, DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Oman,         DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Pakistan,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Palestine,    DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Philippines,  DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Qatar,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::SaudiArabia,  DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Singapore,    DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::SouthKorea,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::SouthYemen,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Syria,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Taiwan,       DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Tajikistan,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Thailand,     DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::TimorLeste,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Turkey,       DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Turkmenistan, DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Uzbekistan,   DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Yemen,        DosCountry::AsiaEnglish },
	{ Iso639::English + "_" + Iso3166::Vietnam,      DosCountry::AsiaEnglish },
};
// clang-format on

// Mapping from the ISO format data to preferred DOS keyboard layouts

// Order of keyboard layouts is currently not significant, but this might change
// in the future - so, if possible, order the keyboard layouts from the most to
// the least popular.

// clang-format off
static const std::unordered_map<std::string, std::set<std::string>> IsoToKeyboardLayouts = {

	{ Iso639::Abkhazian,        { "ka", "rx", "rx443"          } },
	{ Iso639::Albanian,         { "sq", "sq448"                } },
	{ Iso639::Arabic,           { "ar470", "ar462"             } },
	{ Iso639::Armenian,         { "hy"                         } },
	{ Iso639::Azerbaijani,      { "az"                         } },
	{ Iso639::Belarusian,       { "by",                        } },
	{ Iso639::Bosnian,          { "ba"                         } },
	{ Iso639::Breton,           { "uk,", "uk168"               } },
	// Bulgarian priority order confirmed by a native speaker
	{ Iso639::Bulgarian,        { "bg103", "bg", "bg241"       } },
	{ Iso639::Catalan,          { "es", "it", "it142"          } },
	{ Iso639::Chechen,          { "ce", "ce443"                } },
	{ Iso639::Cherokee,         { "us"                         } },
	{ Iso639::Chipewyan,        { "cf", "cf445"                } },
	{ Iso639::ChurchSlavonic,   { "ru", "ru443", "rx", "rx443" } },
	{ Iso639::Cree,             { "cf", "cf445"                } },
	{ Iso639::Croatian,         { "hr"                         } },
	{ Iso639::Czech,            { "cz489", "cz243", "cz"       } },
	{ Iso639::Danish,           { "dk"                         } },
	{ Iso639::Dogrib,           { "cf", "cf445"                } },	
	{ Iso639::Dutch,            { "nl"                         } },
	{ Iso639::Esperanto,        { "gr453", "tr", "tr440"       } },
	{ Iso639::Estonian,         { "ee",                        } },
	{ Iso639::Faroese,          { "fo", "dk"                   } },
	{ Iso639::Finnish,          { "fi",                        } },
	{ Iso639::Fon,              { "bn"                         } },
	{ Iso639::Frankish,         { "be"                         } },
	{ Iso639::French,           { "fr", "fx"                   } },
	{ Iso639::WesternFrisian,   { "de", "gr453", "nl"          } },
	{ Iso639::Friulian,         { "it", "it142"                } },
	{ Iso639::Gaelic,           { "uk,", "uk168"               } },
	{ Iso639::Gagauz,           { "ro", "ro446"                } },
	{ Iso639::Georgian,         { "ka"                         } },
	{ Iso639::German,           { "de", "gr453"                } },
	{ Iso639::LowGerman,        { "de", "gr453", "nl"          } },
	{ Iso639::Greek,            { "gk", "gk220", "gk459"       } },
	{ Iso639::Greenlandic,      { "dk"                         } },
	{ Iso639::Gwichin,          { "cf", "cf445"                } },
	{ Iso639::Haitian,          { "fr", "fx"                   } },
	{ Iso639::Hausa,            { "ng"                         } },
	{ Iso639::Hebrew,           { "il"                         } },
	// Hungarian priority order confirmed by a native speaker
	{ Iso639::Hungarian,        { "hu208", "hu"                } },
	{ Iso639::Icelandic,        { "is", "is161"                } },
	{ Iso639::Igbo,             { "ng"                         } },
	{ Iso639::Inuktitut,        { "cf", "cf445"                } },
	{ Iso639::Irish,            { "uk,", "uk168"               } },
	{ Iso639::Italian,          { "it", "it142", "ix"          } },
	{ Iso639::Kashubian,        { "pl", "pl214"                } },
	{ Iso639::Kazakh,           { "kk", "kk476"                } },
	{ Iso639::Kyrgyz,           { "ky"                         } },
	{ Iso639::Latin,            { "it", "it142"                } },
	{ Iso639::Latvian,          { "lv", "lv455"                } },
	{ Iso639::Limburgan,        { "nl"                         } },
	{ Iso639::Lithuanian,	    { "lt210", "lt456", "lt", "lt211", "lt221" } },
	{ Iso639::Luxembourgish,    { "us", "de", "gr453"          } },
	{ Iso639::Macedonian,       { "mk"                         } },
	{ Iso639::Maltese,          { "mt", "mt103"                } },
	{ Iso639::Manx,             { "uk,", "uk168"               } },
	{ Iso639::Mongolian,        { "mn",                        } },
	{ Iso639::Norwegian,        { "no"                         } },
	{ Iso639::NorwegianBokmal,  { "no"                         } },
	{ Iso639::NorwegianNynorsk, { "no"                         } },
	{ Iso639::Occitan,          { "it", "it142"                } },
	{ Iso639::Ossetian,         { "ka", "rx", "rx443"          } },
	{ Iso639::Picard,           { "be"                         } },
	// Polish priority order confirmed by a native speaker
	{ Iso639::Polish,           { "pl", "pl214"                } },
	{ Iso639::Portuguese,       { "po", "px"                   } },
	{ Iso639::Provencal,        { "it", "it142"                } },
	{ Iso639::BalkanRomani,     { "mk"                         } },
	{ Iso639::FinnishRomani,    { "fi", "no", "sv"             } },
	{ Iso639::Romanian,         { "ro", "ro446"                } },
	{ Iso639::Russian,          { "ru", "ru443", "rx", "rx443" } },
	{ Iso639::NorthSami,        { "fi", "no", "sv"             } },
	{ Iso639::Sardinian,        { "it", "it142"                } },
	{ Iso639::Scots,            { "uk,", "uk168"               } },
	{ Iso639::Serbian,          { "yc", "yc450"                } },
	{ Iso639::NorthSlavey,      { "cf", "cf445"                } },
	{ Iso639::SouthSlavey,      { "cf", "cf445"                } },
	{ Iso639::Slovak,           { "sk"                         } },
	{ Iso639::Slovenian,        { "si"                         } },
	{ Iso639::Sorbian,          { "de", "gr453"                } },
	{ Iso639::Spanish,          { "es", "sx", "la"             } },
	{ Iso639::Swedish,          { "sv"                         } },
	{ Iso639::Tagalog,          { "ph"                         } },
	{ Iso639::Tajik,            { "tj"                         } },
	{ Iso639::Tamajeq,          { "ng"                         } },
	{ Iso639::Tatar,            { "tt", "tt443"                } },
	{ Iso639::Tahitian,         { "fr", "fx"                   } },
	{ Iso639::Turkish,          { "tr", "tr440"                } },
	{ Iso639::Turkmen,          { "tm"                         } },
	{ Iso639::Ukrainian,        { "ur", "ur2007", "ur2001", "ur1996", "ur465" } },
	{ Iso639::Uzbek,            { "uz"                         } },
	{ Iso639::Vietnamese,       { "vi"                         } },
	{ Iso639::Walloon,          { "be"                         } },
	{ Iso639::Welsh,            { "uk,", "uk168"               } },
	{ Iso639::Yoruba,           { "ng", "bn"                   } },

	// English
	{ Iso639::English + "_" + Iso3166::UnitedStates,  { "us", "ux", "dv", "rh", "lh", "co" } },
	{ Iso639::English + "_" + Iso3166::UnitedKingdom, { "uk", "uk168", "kx" } },

	// Definitions to prevent recommending international layouts for users
	// using european country locales
	{ Iso639::French     + "_" + Iso3166::Belgium,  { "be",         } },
	{ Iso639::French     + "_" + Iso3166::France,   { "fr",         } },
	{ Iso639::Italian    + "_" + Iso3166::Italy,    { "it", "it142" } },
	{ Iso639::Portuguese + "_" + Iso3166::Portugal, { "po",         } },
	{ Iso639::Spanish    + "_" + Iso3166::Spain,    { "es",         } },

	// Beninese / Canadian / Nigerien French
	{ Iso639::French + "_" + Iso3166::Benin,   { "bn"          } },
	{ Iso639::French + "_" + Iso3166::Canada,  { "cf", "cf445" } },
	{ Iso639::French + "_" + Iso3166::Niger,   { "ne"          } },

	// Brazilian Portuguese
	{ Iso639::Portuguese + "_" + Iso3166::Brazil, { "br", "br274" } },

	// Montenegrin
	{ Iso639::Croatian + "_" + Iso3166::Montenegro, { "cg" } },
	{ Iso639::Serbian  + "_" + Iso3166::Montenegro, { "cg" } },

	// Swiss German / French
	{ Iso639::German + "_" + Iso3166::Switzerland, { "sd", } },
	{ Iso639::French + "_" + Iso3166::Switzerland, { "sf"  } },

	// Latin American languages
	{ Iso639::Achi,            { "la" } },
	{ Iso639::Akateko,         { "la" } },
	{ Iso639::Awakateko,       { "la" } },
	{ Iso639::Aymara,          { "la" } },
	{ Iso639::ChichimecaJonaz, { "la" } },
	{ Iso639::Chochoteco,      { "la" } },
	{ Iso639::Chol,            { "la" } },
	{ Iso639::Chontal,         { "la" } },
	{ Iso639::Chorti,          { "la" } },
	{ Iso639::Chuj,            { "la" } },
	{ Iso639::Cocopa,          { "la" } },
	{ Iso639::ElNayarCora,     { "la" } },
	{ Iso639::SantaTeresaCora, { "la" } },
	{ Iso639::Diegueno,        { "la" } },
	{ Iso639::Garifuna,        { "la" } },
	{ Iso639::Guarani,         { "la" } },
	{ Iso639::Huarijio,        { "la" } },
	{ Iso639::Huasteco,        { "la" } },
	{ Iso639::Huichol,         { "la" } },
	{ Iso639::Itza,            { "la" } },
	{ Iso639::Ixcateco,        { "la" } },
	{ Iso639::Ixil,            { "la" } },
	{ Iso639::Jakalteko,       { "la" } },
	{ Iso639::Kanjobal,        { "la" } },
	{ Iso639::Kaqchiquel,      { "la" } },
	{ Iso639::Kekchi,          { "la" } },
	{ Iso639::Kiche,           { "la" } },
	{ Iso639::Kickapoo,        { "la" } },
	{ Iso639::Kiliwa,          { "la" } },
	{ Iso639::Lacandon,        { "la" } },
	{ Iso639::Mam,             { "la" } },
	{ Iso639::Matlatzinca,     { "la" } },
	{ Iso639::Mayo,            { "la" } },
	{ Iso639::CentralMazahua,  { "la" } },
	{ Iso639::TolucaMazahua,   { "la" } },
	{ Iso639::Mopan,           { "la" } },
	{ Iso639::Motozintleco,    { "la" } },
	{ Iso639::Ocuilteco,       { "la" } },
	{ Iso639::Oodham,          { "la" } },
	{ Iso639::Otomi,           { "la" } },
	{ Iso639::Paipai,          { "la" } },
	{ Iso639::Plautdietsch,    { "la" } },
	{ Iso639::Pokomam,         { "la" } },
	{ Iso639::Pokomchi,        { "la" } },
	{ Iso639::EastPurepecha,   { "la" } },
	{ Iso639::WestPurepecha,   { "la" } },
	{ Iso639::Quechua,         { "la" } },
	{ Iso639::Sakapulteko,     { "la" } },
	{ Iso639::Seri,            { "la" } },
	{ Iso639::Sipakapense,     { "la" } },
	{ Iso639::Tektiteko,       { "la" } },
	{ Iso639::Tepeuxila,       { "la" } },
	{ Iso639::Teutila,         { "la" } },
	{ Iso639::Tojolabal,       { "la" } },
	{ Iso639::Totonaco,        { "la" } },
	{ Iso639::Tzeltal,         { "la" } },
	{ Iso639::Tzotzil,         { "la" } },
	{ Iso639::Tzutujil,        { "la" } },
	{ Iso639::Uspanteko,       { "la" } },
	{ Iso639::Xinka,           { "la" } },
	{ Iso639::Yaqui,           { "la" } },
	{ Iso639::Yucatec,         { "la" } },
	{ Iso639::Zapoteco,        { "la" } },

	// Russian Federation minority languages, using international Cyrillic
	{ Iso639::Adyghe,         { "rx", "rx443" } },
	{ Iso639::Altai,          { "rx", "rx443" } },
	{ Iso639::Avaric,         { "rx", "rx443" } },
	{ Iso639::Bashkir,        { "rx", "rx443" } },
	{ Iso639::Buryat,         { "rx", "rx443" } },
	{ Iso639::Cheremiss,      { "rx", "rx443" } },
	{ Iso639::Chukchi,        { "rx", "rx443" } },
	{ Iso639::Chuvash,        { "rx", "rx443" } },
	{ Iso639::Dolgan,         { "rx", "rx443" } },
	{ Iso639::Erzya,          { "rx", "rx443" } },
	{ Iso639::Evenki,         { "rx", "rx443" } },
	{ Iso639::Ingush,         { "rx", "rx443" } },
	{ Iso639::Kabardian,      { "rx", "rx443" } },
	{ Iso639::Kalmyk,         { "rx", "rx443" } },
	{ Iso639::KarachayBalkar, { "rx", "rx443" } },
	{ Iso639::Karelian,       { "rx", "rx443" } },
	{ Iso639::Khakas,         { "rx", "rx443" } },
	{ Iso639::Khanty,         { "rx", "rx443" } },
	{ Iso639::Komi,           { "rx", "rx443" } },
	{ Iso639::Koryak,         { "rx", "rx443" } },
	{ Iso639::Mansi,          { "rx", "rx443" } },
	{ Iso639::Moksha,         { "rx", "rx443" } },
	{ Iso639::Sakha,          { "rx", "rx443" } },
	{ Iso639::Tuvin,          { "rx", "rx443" } },
	{ Iso639::Udmurt,         { "rx", "rx443" } },
	{ Iso639::Yiddish,        { "rx", "rx443" } },
	{ Iso639::Yurak,          { "rx", "rx443" } },

	// Other languages, using international keyboard layouts
	{ Iso639::Afrikaans,    { "ux", "kx" } },
	{ Iso639::Chamorro,     { "ux", "kx" } },
	{ Iso639::Chichewa,     { "ux", "kx" } },
	{ Iso639::Fanagalo,     { "ux", "kx" } },
	{ Iso639::Khoisan,      { "ux", "kx" } },
	{ Iso639::Khwe,         { "ux", "kx" } },
	{ Iso639::Malagasy,     { "ux", "kx" } },
	{ Iso639::Maore,        { "ux", "kx" } },
	{ Iso639::Maori,        { "ux", "kx" } },
	{ Iso639::Mwali,        { "ux", "kx" } },
	{ Iso639::Nama,         { "ux", "kx" } },
	{ Iso639::Ndzwani,      { "ux", "kx" } },
	{ Iso639::Ngazidja,     { "ux", "kx" } },
	{ Iso639::NorthNdebele, { "ux", "kx" } },
	{ Iso639::SouthNdebele, { "ux", "kx" } },
	{ Iso639::NorthSotho,   { "ux", "kx" } },
	{ Iso639::SouthSotho,   { "ux", "kx" } },
	{ Iso639::Swati,        { "ux", "kx" } },
	{ Iso639::Tsonga,       { "ux", "kx" } },
	{ Iso639::Tswana,       { "ux", "kx" } },
	{ Iso639::Venda,        { "ux", "kx" } },
	{ Iso639::Xhosa,        { "ux", "kx" } },
	{ Iso639::Zulu,         { "ux", "kx" } },
	{ Iso639::Bislama,      { "ux", "kx", "fx" } },
	{ Iso639::Chuukese,     { "ux", "kx", "fx" } },
	{ Iso639::Fijian,       { "ux", "kx", "fx" } },
	{ Iso639::Gilbertese,   { "ux", "kx", "fx" } },
	{ Iso639::Hawaiian,     { "ux", "kx", "fx" } },
	{ Iso639::Kosraean,     { "ux", "kx", "fx" } },
	{ Iso639::Marshallese,  { "ux", "kx", "fx" } },
	{ Iso639::Nauruan,      { "ux", "kx", "fx" } },
	{ Iso639::Niuean,       { "ux", "kx", "fx" } },
	{ Iso639::Palauan,      { "ux", "kx", "fx" } },
	{ Iso639::Pohnpeian,    { "ux", "kx", "fx" } },
	{ Iso639::Samoan,       { "ux", "kx", "fx" } },
	{ Iso639::Sonsoralese,  { "ux", "kx", "fx" } },
	{ Iso639::Tobian,       { "ux", "kx", "fx" } },
	{ Iso639::Tokelauan,    { "ux", "kx", "fx" } },
	{ Iso639::Tonga,        { "ux", "kx", "fx" } },
	{ Iso639::Tuvaluan,     { "ux", "kx", "fx" } },
	{ Iso639::Ulithian,     { "ux", "kx", "fx" } },
	{ Iso639::Yapese,       { "ux", "kx", "fx" } },
	{ Iso639::Bambara,      { "ux", "kx", "fx", "px" } },
	{ Iso639::Dagaare,      { "ux", "kx", "fx", "px" } },
	{ Iso639::Dagbani,      { "ux", "kx", "fx", "px" } },
	{ Iso639::Dangme,       { "ux", "kx", "fx", "px" } },
	{ Iso639::Dyula,        { "ux", "kx", "fx", "px" } },
	{ Iso639::Ewe,          { "ux", "kx", "fx", "px" } },
	{ Iso639::Ga,           { "ux", "kx", "fx", "px" } },
	{ Iso639::Gonja,        { "ux", "kx", "fx", "px" } },
	{ Iso639::Kabuverdianu, { "ux", "kx", "fx", "px" } },
	{ Iso639::Kanuri,       { "ux", "kx", "fx", "px" } },
	{ Iso639::Kasem,        { "ux", "kx", "fx", "px" } },
	{ Iso639::Krio,         { "ux", "kx", "fx", "px" } },
	{ Iso639::Mandinka,     { "ux", "kx", "fx", "px" } },
	{ Iso639::Moore,        { "ux", "kx", "fx", "px" } },
	{ Iso639::Nzema,        { "ux", "kx", "fx", "px" } },
	{ Iso639::Twi,          { "ux", "kx", "fx", "px" } },
	{ Iso639::Zarma,        { "ux", "kx", "fx", "px", "ng" } },
	{ Iso639::Angolar,      { "ux", "kx", "fx", "px", "bx", "sx" } },
	{ Iso639::Forro,        { "ux", "kx", "fx", "px", "bx", "sx" } },	
	{ Iso639::Kongo,        { "ux", "kx", "fx", "px", "bx", "sx" } },
	{ Iso639::Lingala,      { "ux", "kx", "fx", "px", "bx", "sx" } },
	{ Iso639::LubaKasai,    { "ux", "kx", "fx", "px", "bx", "sx" } },	
	{ Iso639::Principense,  { "ux", "kx", "fx", "px", "bx", "sx" } },	
	{ Iso639::Sango,        { "ux", "kx", "fx", "px", "bx", "sx" } },
	{ Iso639::Afar,         { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Dinka,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },	
	{ Iso639::Fulah,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Ganda,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Kinyarwanda,  { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Neuer,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Oromo,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Rundi,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },	
	{ Iso639::Somali,       { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Soninke,      { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },	
	{ Iso639::Swahili,      { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
	{ Iso639::Wolof,        { "ux", "kx", "fx", "px", "bx", "sx", "ix" } },
};
// clang-format on

// ***************************************************************************
// LanguageTerritory structure
// ***************************************************************************

LanguageTerritory::LanguageTerritory(const std::string& language,
                                     const std::string& territory)
        : language(language),
          territory(territory)
{
	Normalize();
}

LanguageTerritory::LanguageTerritory(const std::string& input)
{
	std::string tmp = input;

	// Strip the modifier and the codeset
	tmp = tmp.substr(0, tmp.rfind('@'));
	tmp = tmp.substr(0, tmp.rfind('.'));

	const auto tokens = split(tmp, "_-");
	if (tokens.empty() || tokens.size() >= 3) {
		// Invalid format
		return;
	}

	language = tokens[0];
	if (!IsEmpty() && !IsGeneric() && tokens.size() == 2) {
		territory = tokens[1];
	}

	Normalize();
}

void LanguageTerritory::Normalize()
{
	auto check_characters = [&](const std::string& input) {
		for (const auto c : input) {
			if (!is_printable_ascii(c) || std::isdigit(c)) {
				// Found invalid character
				language.clear();
				territory.clear();
				return;
			}
		}
	};

	check_characters(language);
	check_characters(territory);

	lowcase(language);
	upcase(territory);

	if (IsEmpty() || IsGeneric()) {
		territory = {};
	}
}

bool LanguageTerritory::IsEmpty() const
{
	return language.empty();
}

bool LanguageTerritory::IsGeneric() const
{
	return language == "c" || language == "posix";
}

bool LanguageTerritory::IsEnglish() const
{
	return language == Iso639::English;
}

const std::string& LanguageTerritory::GetIsoLanguageCode() const
{
	return language;
}

const std::string& LanguageTerritory::GetIsoTerritoryCode() const
{
	return territory;
}

std::optional<DosCountry> LanguageTerritory::GetDosCountryCode() const
{
	if (IsEmpty() || IsGeneric()) {
		return {};
	}

	const auto key = language + "_" + territory;
	if (IsoToDosCountryMap.contains(key)) {
		return IsoToDosCountryMap.at(key);
	}

	if (IsoToDosCountryMap.contains(territory)) {
		return IsoToDosCountryMap.at(territory);
	}

	return {};
}

std::vector<std::string> LanguageTerritory::GetLanguageFiles() const
{
	if (IsEmpty()) {
		return {};
	} else if (IsGeneric()) {
		return {Iso639::English};
	}

	std::vector<std::string> result = {};

	if (!territory.empty()) {
		result.emplace_back(language + "_" + territory);
	}

	if (language == Iso639::Portuguese && territory == Iso3166::Brazil) {
		// Brazilian Portuguese differs a lot from the regular one,
		// they can't be substituted
		return result;
	}

	result.push_back(language);
	return result;
}

const std::set<std::string>& LanguageTerritory::GetMatchingKeyboardLayouts() const
{
	static const std::set<std::string> Empty = {};

	if (IsEmpty() || IsGeneric()) {
		return Empty;
	}

	const auto key = language + "_" + territory;
	if (IsoToKeyboardLayouts.contains(key)) {
		return IsoToKeyboardLayouts.at(key);
	}

	if (IsoToKeyboardLayouts.contains(language)) {
		return IsoToKeyboardLayouts.at(language);
	}

	return Empty;
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
		// Unable to create locale object, detection failed
		return;
	}

	GetNumericFormat(locale);
	GetDateFormat(locale);

	// Both 'GetDateFormat' and 'DetectTimeDateFormat' are able to retrieve
	// the date format from the C++ standard library, but they try to use
	// different methods; even if one of them fails, there is still a chance
	// the other one suceeds.

	DetectCurrencyFormat(locale);
	DetectTimeDateFormat(locale);
}

// On macOS the C++ standard library returns generic values. To get the real
// values, we need to refer to the CoreFoundation API.
#if !C_COREFOUNDATION

void StdLibLocale::GetNumericFormat(const std::locale& locale)
{
	const auto& format = std::use_facet<std::numpunct<char>>(locale);

	thousands_separator = format.thousands_sep();
	decimal_separator   = format.decimal_point();
}

void StdLibLocale::GetDateFormat(const std::locale& locale)
{
	const auto& format = std::use_facet<std::time_get<char>>(locale);

	switch (format.date_order()) {
	case std::time_base::dmy:
		date_format = DosDateFormat::DayMonthYear;
		break;
	case std::time_base::mdy:
		date_format = DosDateFormat::MonthDayYear;
		break;
	case std::time_base::ymd:
		date_format = DosDateFormat::YearMonthDay;
		break;
	case std::time_base::ydm:
		// DOS does not support this
	default:
		// C++ library is allowed to return 'std::time_base::no_order'
		break;
	}
}

#if !defined(WIN32)
void StdLibLocale::DetectCurrencyFormat(const std::locale& locale)
{
	// We can only get a suitable data if monetary locale format is UTF-8
	if (!IsMonetaryUtf8(locale)) {
		return;
	}

	// Retrieve currency code
	constexpr bool International = true;
	const auto& format_code =
		std::use_facet<std::moneypunct<char, International>>(locale);
	currency_code = format_code.curr_symbol();
	trim(currency_code);

	// Retrieve currency symbol
	const auto& format_symbol =
		std::use_facet<std::moneypunct<char>>(locale);
	currency_utf8 = format_symbol.curr_symbol();
	trim(currency_utf8);

	// Retrieve currency precision
	const auto precision = format_symbol.frac_digits();
	if (precision > 0 && precision <= UINT8_MAX) {
		currency_precision = static_cast<uint8_t>(precision);
	}

	// Detect amount/symbol order
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

	constexpr double TestMoney = 123.45;

	std::stringstream test_money_code_stream = {};
	std::stringstream test_money_utf8_stream = {};

	test_money_code_stream.imbue(locale);
	test_money_utf8_stream.imbue(locale);

	test_money_code_stream << std::showbase << std::put_money(TestMoney, true);
	test_money_utf8_stream << std::showbase << std::put_money(TestMoney, false);

	auto money_code_example = test_money_code_stream.str();
	auto money_utf8_example = test_money_utf8_stream.str();

	trim(money_code_example);
	trim(money_utf8_example);

	currency_code_format = detect_format(money_code_example, currency_code);
	currency_utf8_format = detect_format(money_code_example, currency_utf8);
}
#endif

void StdLibLocale::DetectTimeDateFormat(const std::locale& locale)
{
	std::tm test_time_date = {};

	test_time_date.tm_isdst = 0;   // no DST in effect
	test_time_date.tm_year  = 111; // 2011 (years since 1900)
	test_time_date.tm_mon   = 11;  // 12th month (months since January)
	test_time_date.tm_mday  = 13;  // 13th day
	test_time_date.tm_hour  = 22;
	test_time_date.tm_min   = 14;
	test_time_date.tm_sec   = 15;

	std::stringstream test_time_stream = {};
	std::stringstream test_date_stream = {};

	test_time_stream.imbue(locale);
	test_date_stream.imbue(locale);

	test_time_stream << std::put_time(&test_time_date, "%X");
	test_date_stream << std::put_time(&test_time_date, "%x");

	auto time_example = test_time_stream.str();
	auto date_example = test_date_stream.str();

	trim(time_example);
	trim(date_example);

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
}

#endif
