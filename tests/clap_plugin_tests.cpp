// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio/clap/library.h"

#include <gtest/gtest.h>

static Clap::PluginInfo make_plugin_info(const std::string& library_name,
                                         const std::string& plugin_name,
                                         const uint32_t index)
{
	Clap::PluginInfo info = {};
	info.library_path     = std_fs::path("plugins") / library_name;
	info.name             = plugin_name;
	info.index            = index;
	return info;
}

TEST(ClapPlugin, LibraryNameWithOrWithoutExtension)
{
	const auto info = make_plugin_info("Synth.clap", "My Synth", 0);
	EXPECT_TRUE(info.Matches("Synth", "My Synth"));
	EXPECT_TRUE(info.Matches("Synth.clap", "My Synth"));
	EXPECT_TRUE(info.Matches("Synth.clap", "mY sYnTh"));
	EXPECT_FALSE(info.Matches("Other", "My Synth"));
	EXPECT_FALSE(info.Matches("Other.clap", "My Synth"));
}

TEST(ClapPlugin, LibraryNameMatchesAreCaseSensitive)
{
	const auto info = make_plugin_info("Synth.clap", "My Synth", 0);
	EXPECT_FALSE(info.Matches("synth", "My Synth"));
	EXPECT_FALSE(info.Matches("synth.clap", "My Synth"));
	EXPECT_FALSE(info.Matches("Synth.CLAP", "My Synth"));
	EXPECT_FALSE(info.Matches("sYnTh.ClAp", "My Synth"));
}

TEST(ClapPlugin, EmptySelectors)
{
	const auto info = make_plugin_info("Synth.clap", "My Synth", 3);
	EXPECT_TRUE(info.Matches("Synth", ""));
	EXPECT_TRUE(info.Matches("", "My Synth"));
	EXPECT_TRUE(info.Matches("", ""));
	EXPECT_FALSE(info.Matches("Other", ""));
	EXPECT_FALSE(info.Matches("", "Other Synth"));
}

TEST(ClapPlugin, PartialNameMatchesAreCaseInsensitive)
{
	const auto info = make_plugin_info("Synth.clap", "Roland SC-55 v1.20", 2);
	EXPECT_TRUE(info.Matches("Synth", "roland"));
	EXPECT_TRUE(info.Matches("Synth", "sc-55"));
	EXPECT_TRUE(info.Matches("Synth", "V1.20"));
	EXPECT_TRUE(info.Matches("", "SC-55 v1.2"));
	EXPECT_FALSE(info.Matches("Synth", "v1.21"));
	EXPECT_FALSE(info.Matches("Synth", "SC-55 v1.20 extra"));
	EXPECT_FALSE(info.Matches("Other", "SC-55"));
	EXPECT_FALSE(info.Matches("Syn", "SC-55"));
}

TEST(ClapPlugin, IndexIsZeroBasedAndScopedToLibrary)
{
	const auto first = make_plugin_info("Synth.clap", "First Synth", 0);
	// Descriptor indices need not be contiguous if a descriptor is missing.
	const auto second = make_plugin_info("Synth.clap", "Second Synth", 3);
	const auto other  = make_plugin_info("Other.clap", "Other Synth", 3);
	EXPECT_TRUE(first.Matches("Synth", "0"));
	EXPECT_FALSE(first.Matches("Synth", "3"));
	EXPECT_TRUE(second.Matches("Synth", "3"));
	EXPECT_TRUE(second.Matches("Synth.clap", "003"));
	EXPECT_FALSE(second.Matches("Synth", "1"));
	EXPECT_FALSE(other.Matches("Synth", "3"));
	EXPECT_TRUE(other.Matches("Other", "3"));
}

TEST(ClapPlugin, InvalidOrOutOfRangeIndices)
{
	const auto info = make_plugin_info("Synth.clap", "My Synth", 0);
	EXPECT_FALSE(info.Matches("Synth", "-1"));
	EXPECT_FALSE(info.Matches("Synth", "1"));
	EXPECT_FALSE(info.Matches("Synth", "0abc"));
	EXPECT_FALSE(info.Matches("Synth", "4294967296"));
	EXPECT_FALSE(info.Matches("Synth", "999999999999999999999999"));
}

TEST(ClapPlugin, NumericSelectorsTakePrecedenceOverNames)
{
	const auto info = make_plugin_info("Synth.clap", "3", 0);
	EXPECT_TRUE(info.Matches("Synth", "0"));
	EXPECT_FALSE(info.Matches("Synth", "3"));
	// Selectors that cannot be parsed as an int are treated as names.
	const auto overflow = make_plugin_info("Synth.clap", "4294967296", 0);
	EXPECT_TRUE(overflow.Matches("Synth", "4294967296"));
}
