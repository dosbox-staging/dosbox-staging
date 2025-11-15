// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/host_locale.h"
#include "misc/support.h"

#include <gtest/gtest.h>

namespace {

TEST(LanguageTerritory, Empty)
{
	const auto empty = LanguageTerritory("");

	EXPECT_TRUE(empty.IsEmpty());

	EXPECT_FALSE(empty.IsGeneric());
	EXPECT_FALSE(empty.IsEnglish());
	EXPECT_FALSE(empty.GetDosCountryCode());

	EXPECT_TRUE(empty.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(empty.GetLanguageFiles().empty());
}

TEST(LanguageTerritory, Unknown)
{
	const auto unknown = LanguageTerritory("foo-BAR");

	EXPECT_FALSE(unknown.IsEmpty());

	EXPECT_FALSE(unknown.IsGeneric());
	EXPECT_FALSE(unknown.IsEnglish());
	EXPECT_FALSE(unknown.GetDosCountryCode());

	EXPECT_TRUE(unknown.GetMatchingKeyboardLayouts().empty());

	EXPECT_EQ(unknown.GetLanguageFiles(),
	          std::vector<std::string>({"foo_BAR", "foo"}));
}

TEST(LanguageTerritory, InvalidFormat)
{
	const auto invalid_1 = LanguageTerritory("-");
	const auto invalid_2 = LanguageTerritory(".FooBar");
	const auto invalid_3 = LanguageTerritory("@FooBar");

	EXPECT_TRUE(invalid_1.IsEmpty());
	EXPECT_TRUE(invalid_2.IsEmpty());
	EXPECT_TRUE(invalid_3.IsEmpty());

	EXPECT_FALSE(invalid_1.IsGeneric());
	EXPECT_FALSE(invalid_2.IsGeneric());
	EXPECT_FALSE(invalid_3.IsGeneric());

	EXPECT_FALSE(invalid_1.IsEnglish());
	EXPECT_FALSE(invalid_2.IsEnglish());
	EXPECT_FALSE(invalid_3.IsEnglish());

	EXPECT_FALSE(invalid_1.GetDosCountryCode());
	EXPECT_FALSE(invalid_2.GetDosCountryCode());
	EXPECT_FALSE(invalid_3.GetDosCountryCode());

	EXPECT_TRUE(invalid_1.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(invalid_2.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(invalid_3.GetMatchingKeyboardLayouts().empty());

	EXPECT_TRUE(invalid_1.GetLanguageFiles().empty());
	EXPECT_TRUE(invalid_2.GetLanguageFiles().empty());
	EXPECT_TRUE(invalid_3.GetLanguageFiles().empty());
}

TEST(LanguageTerritory, Generic)
{
	const auto generic_1 = LanguageTerritory("c");
	const auto generic_2 = LanguageTerritory("C");
	const auto generic_3 = LanguageTerritory("posix");
	const auto generic_4 = LanguageTerritory("POSIX");
	const auto generic_5 = LanguageTerritory("PoSiX");

	EXPECT_FALSE(generic_1.IsEmpty());
	EXPECT_FALSE(generic_2.IsEmpty());
	EXPECT_FALSE(generic_3.IsEmpty());
	EXPECT_FALSE(generic_4.IsEmpty());
	EXPECT_FALSE(generic_5.IsEmpty());

	EXPECT_TRUE(generic_1.IsGeneric());
	EXPECT_TRUE(generic_2.IsGeneric());
	EXPECT_TRUE(generic_3.IsGeneric());
	EXPECT_TRUE(generic_4.IsGeneric());
	EXPECT_TRUE(generic_5.IsGeneric());

	EXPECT_FALSE(generic_1.IsEnglish());
	EXPECT_FALSE(generic_2.IsEnglish());
	EXPECT_FALSE(generic_3.IsEnglish());
	EXPECT_FALSE(generic_4.IsEnglish());
	EXPECT_FALSE(generic_5.IsEnglish());

	EXPECT_FALSE(generic_1.GetDosCountryCode());
	EXPECT_FALSE(generic_2.GetDosCountryCode());
	EXPECT_FALSE(generic_3.GetDosCountryCode());
	EXPECT_FALSE(generic_4.GetDosCountryCode());
	EXPECT_FALSE(generic_5.GetDosCountryCode());

	EXPECT_TRUE(generic_1.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(generic_2.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(generic_3.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(generic_4.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(generic_5.GetMatchingKeyboardLayouts().empty());

	EXPECT_EQ(generic_1.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(generic_2.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(generic_3.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(generic_4.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(generic_5.GetLanguageFiles(), std::vector<std::string>({"en"}));
}

TEST(LanguageTerritory, English)
{
	const auto en_1  = LanguageTerritory("en");
	const auto en_2  = LanguageTerritory("EN");
	const auto en_3  = LanguageTerritory("eN");
	const auto en_US = LanguageTerritory("en_US");
	const auto en_GB = LanguageTerritory("en-GB");

	EXPECT_FALSE(en_1.IsEmpty());
	EXPECT_FALSE(en_2.IsEmpty());
	EXPECT_FALSE(en_3.IsEmpty());
	EXPECT_FALSE(en_US.IsEmpty());
	EXPECT_FALSE(en_GB.IsEmpty());

	EXPECT_FALSE(en_1.IsGeneric());
	EXPECT_FALSE(en_2.IsGeneric());
	EXPECT_FALSE(en_3.IsGeneric());
	EXPECT_FALSE(en_US.IsGeneric());
	EXPECT_FALSE(en_GB.IsGeneric());

	EXPECT_TRUE(en_1.IsEnglish());
	EXPECT_TRUE(en_2.IsEnglish());
	EXPECT_TRUE(en_3.IsEnglish());
	EXPECT_TRUE(en_US.IsEnglish());
	EXPECT_TRUE(en_GB.IsEnglish());

	/* Temporarily disabled due to problems with std::optional on GCC 12

	EXPECT_FALSE(en_1.GetDosCountryCode());
	EXPECT_FALSE(en_2.GetDosCountryCode());
	EXPECT_FALSE(en_3.GetDosCountryCode());

	EXPECT_EQ(en_US.GetDosCountryCode(), DosCountry::UnitedStates);
	EXPECT_EQ(en_GB.GetDosCountryCode(), DosCountry::UnitedKingdom);

	EXPECT_TRUE(en_1.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(en_2.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(en_3.GetMatchingKeyboardLayouts().empty());
	EXPECT_TRUE(en_US.GetMatchingKeyboardLayouts().contains("us"));
	EXPECT_TRUE(en_GB.GetMatchingKeyboardLayouts().contains("uk"));

	*/

	EXPECT_EQ(en_1.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(en_2.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(en_3.GetLanguageFiles(), std::vector<std::string>({"en"}));
	EXPECT_EQ(en_US.GetLanguageFiles(),
	          std::vector<std::string>({"en_US", "en"}));
	EXPECT_EQ(en_GB.GetLanguageFiles(),
	          std::vector<std::string>({"en_GB", "en"}));
}

TEST(LanguageTerritory, Portuguese)
{
	const auto pt_1    = LanguageTerritory("pt");
	const auto pt_2    = LanguageTerritory("Pt");
	const auto pt_PT_1 = LanguageTerritory("pt-PT");
	const auto pt_PT_2 = LanguageTerritory("Pt_pT");

	EXPECT_FALSE(pt_1.IsEmpty());
	EXPECT_FALSE(pt_2.IsEmpty());
	EXPECT_FALSE(pt_PT_1.IsEmpty());
	EXPECT_FALSE(pt_PT_2.IsEmpty());

	EXPECT_FALSE(pt_1.IsGeneric());
	EXPECT_FALSE(pt_2.IsGeneric());
	EXPECT_FALSE(pt_PT_1.IsGeneric());
	EXPECT_FALSE(pt_PT_2.IsGeneric());

	EXPECT_FALSE(pt_1.IsEnglish());
	EXPECT_FALSE(pt_2.IsEnglish());
	EXPECT_FALSE(pt_PT_1.IsEnglish());
	EXPECT_FALSE(pt_PT_2.IsEnglish());

	/* Temporarily disabled due to problems with std::optional on GCC 12

	EXPECT_FALSE(pt_1.GetDosCountryCode());
	EXPECT_FALSE(pt_2.GetDosCountryCode());

	EXPECT_EQ(pt_PT_1.GetDosCountryCode(), DosCountry::Portugal);
	EXPECT_EQ(pt_PT_2.GetDosCountryCode(), DosCountry::Portugal);

	EXPECT_TRUE(pt_1.GetMatchingKeyboardLayouts().contains("po"));
	EXPECT_TRUE(pt_2.GetMatchingKeyboardLayouts().contains("po"));
	EXPECT_TRUE(pt_PT_1.GetMatchingKeyboardLayouts().contains("po"));
	EXPECT_TRUE(pt_PT_2.GetMatchingKeyboardLayouts().contains("po"));

	*/

	EXPECT_EQ(pt_1.GetLanguageFiles(), std::vector<std::string>({"pt"}));
	EXPECT_EQ(pt_2.GetLanguageFiles(), std::vector<std::string>({"pt"}));
	EXPECT_EQ(pt_PT_1.GetLanguageFiles(),
	          std::vector<std::string>({"pt_PT", "pt"}));
	EXPECT_EQ(pt_PT_2.GetLanguageFiles(),
	          std::vector<std::string>({"pt_PT", "pt"}));
}

TEST(LanguageTerritory, Brazilian)
{
	const auto pt_BR_1 = LanguageTerritory("pt_BR");
	const auto pt_BR_2 = LanguageTerritory("Pt-br");

	EXPECT_FALSE(pt_BR_1.IsEmpty());
	EXPECT_FALSE(pt_BR_2.IsEmpty());

	EXPECT_FALSE(pt_BR_1.IsGeneric());
	EXPECT_FALSE(pt_BR_2.IsGeneric());

	EXPECT_FALSE(pt_BR_1.IsEnglish());
	EXPECT_FALSE(pt_BR_2.IsEnglish());

	/* Temporarily disabled due to problems with std::optional on GCC 12

	EXPECT_EQ(pt_BR_1.GetDosCountryCode(), DosCountry::Brazil);
	EXPECT_EQ(pt_BR_2.GetDosCountryCode(), DosCountry::Brazil);

	EXPECT_TRUE(pt_BR_1.GetMatchingKeyboardLayouts().contains("br"));
	EXPECT_TRUE(pt_BR_2.GetMatchingKeyboardLayouts().contains("br"));

	*/

	EXPECT_EQ(pt_BR_1.GetLanguageFiles(), std::vector<std::string>({"pt_BR"}));
	EXPECT_EQ(pt_BR_2.GetLanguageFiles(), std::vector<std::string>({"pt_BR"}));
}

TEST(LanguageTerritory, InputStripping)
{
	const auto de_DE = LanguageTerritory("de_DE@foo");
	const auto fr_FR = LanguageTerritory("fr-FR.bar");
	const auto it_IT = LanguageTerritory("it_IT.foo@bar");

	EXPECT_FALSE(de_DE.IsEmpty());
	EXPECT_FALSE(fr_FR.IsEmpty());
	EXPECT_FALSE(it_IT.IsEmpty());

	EXPECT_FALSE(de_DE.IsGeneric());
	EXPECT_FALSE(fr_FR.IsGeneric());
	EXPECT_FALSE(it_IT.IsGeneric());

	EXPECT_FALSE(de_DE.IsEnglish());
	EXPECT_FALSE(fr_FR.IsEnglish());
	EXPECT_FALSE(it_IT.IsEnglish());

	/* Temporarily disabled due to problems with std::optional on GCC 12

	EXPECT_EQ(de_DE.GetDosCountryCode(), DosCountry::Germany);
	EXPECT_EQ(fr_FR.GetDosCountryCode(), DosCountry::France);
	EXPECT_EQ(it_IT.GetDosCountryCode(), DosCountry::Italy);

	EXPECT_TRUE(de_DE.GetMatchingKeyboardLayouts().contains("de"));
	EXPECT_TRUE(fr_FR.GetMatchingKeyboardLayouts().contains("fr"));
	EXPECT_TRUE(it_IT.GetMatchingKeyboardLayouts().contains("it"));

	*/

	EXPECT_EQ(de_DE.GetLanguageFiles(),
	          std::vector<std::string>({"de_DE", "de"}));
	EXPECT_EQ(fr_FR.GetLanguageFiles(),
	          std::vector<std::string>({"fr_FR", "fr"}));
	EXPECT_EQ(it_IT.GetLanguageFiles(),
	          std::vector<std::string>({"it_IT", "it"}));
}

} // namespace
