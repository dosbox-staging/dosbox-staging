// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/mapper.h"

#include <gtest/gtest.h>

namespace {

constexpr int MaxAxes = 10;

// An Xbox Series X controller over Bluetooth as SDL3 reports it on Windows
constexpr auto XboxMapping =
        "0300fa675e040000130b000024057801,*,a:b0,b:b1,back:b6,dpdown:h0.4,"
        "dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,"
        "leftstick:b8,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,"
        "rightstick:b9,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,";

TEST(MAPPER_FindMappedAxis, sticks_and_triggers)
{
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "leftx", MaxAxes), 0);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "lefty", MaxAxes), 1);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "lefttrigger", MaxAxes), 2);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "rightx", MaxAxes), 3);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "righty", MaxAxes), 4);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "righttrigger", MaxAxes), 5);
}

TEST(MAPPER_FindMappedAxis, buttons_and_hats_are_no_axes)
{
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "a", MaxAxes), std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "x", MaxAxes), std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "dpup", MaxAxes), std::nullopt);
}

TEST(MAPPER_FindMappedAxis, half_axes_are_no_full_axes)
{
	constexpr auto Mapping = "guid,name,lefttrigger:+a2,righttrigger:-a5,";
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "lefttrigger", MaxAxes),
	          std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "righttrigger", MaxAxes),
	          std::nullopt);
}

TEST(MAPPER_FindMappedAxis, inverted_axis)
{
	constexpr auto Mapping = "guid,name,righty:a4~,";
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "righty", MaxAxes), 4);
}

TEST(MAPPER_FindMappedAxis, missing_element)
{
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "paddle1", MaxAxes),
	          std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis("", "leftx", MaxAxes), std::nullopt);
}

TEST(MAPPER_FindMappedAxis, whole_element_names_only)
{
	// "ftx" is the end of "leftx", "left" the start of "leftx"
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "ftx", MaxAxes), std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "left", MaxAxes), std::nullopt);
}

TEST(MAPPER_FindMappedAxis, axis_numbers_up_to_num_axes)
{
	constexpr auto Mapping = "guid,name,leftx:a12,";
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "leftx", MaxAxes), std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "leftx", 16), 12);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "rightx", 3), std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(XboxMapping, "rightx", 4), 3);
}

TEST(MAPPER_FindMappedAxis, invalid_format)
{
	constexpr auto Mapping = "guid,name,leftx:a,lefty:ab,";
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "leftx", MaxAxes), std::nullopt);
	EXPECT_EQ(MAPPER_FindMappedAxis(Mapping, "lefty", MaxAxes), std::nullopt);
}

} // namespace
