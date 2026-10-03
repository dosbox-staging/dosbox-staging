// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/private/window_geometry.h"

#include <gtest/gtest.h>

using namespace WindowGeometry;

namespace {

void expect_setting(const std::optional<Setting>& actual, const float x,
                    const float y, const Unit unit)
{
	ASSERT_TRUE(actual.has_value());
	EXPECT_FLOAT_EQ(actual->x, x);
	EXPECT_FLOAT_EQ(actual->y, y);
	EXPECT_EQ(actual->unit, unit);
}

void expect_point(const SDL_Point actual, const int x, const int y)
{
	EXPECT_EQ(actual.x, x);
	EXPECT_EQ(actual.y, y);
}

// 1080p desktop at 150% DPI scaling
constexpr Desktop TestDesktop = {1280, 720, 1.5f};

} // namespace

// ----------------------------------------------------------------------------
// ParseSize
// ----------------------------------------------------------------------------

TEST(WindowGeometry, ParseSizeLogicalUnits)
{
	expect_setting(ParseSize("1024x768"), 1024, 768, Unit::LogicalUnits);
	expect_setting(ParseSize("1024X768"), 1024, 768, Unit::LogicalUnits);
	expect_setting(ParseSize("1024,768"), 1024, 768, Unit::LogicalUnits);
	expect_setting(ParseSize("1024 x 768"), 1024, 768, Unit::LogicalUnits);
}

TEST(WindowGeometry, ParseSizePixels)
{
	expect_setting(ParseSize("1440x1080px"), 1440, 1080, Unit::Pixels);
	expect_setting(ParseSize("1440x1080PX"), 1440, 1080, Unit::Pixels);
	expect_setting(ParseSize("1440x1080 px"), 1440, 1080, Unit::Pixels);
}

TEST(WindowGeometry, ParseSizePercentage)
{
	expect_setting(ParseSize("133x100%"), 133, 100, Unit::Percentage);
	expect_setting(ParseSize("66.5x50%"), 66.5f, 50, Unit::Percentage);
	expect_setting(ParseSize("1000x1000%"), 1000, 1000, Unit::Percentage);
}

TEST(WindowGeometry, ParseSizeInvalid)
{
	EXPECT_FALSE(ParseSize(""));
	EXPECT_FALSE(ParseSize("default"));
	EXPECT_FALSE(ParseSize("1024"));
	EXPECT_FALSE(ParseSize("1024x"));
	EXPECT_FALSE(ParseSize("1024x768x2"));
	EXPECT_FALSE(ParseSize("1024x768pt"));
	EXPECT_FALSE(ParseSize("1024.5x768"));
	EXPECT_FALSE(ParseSize("1024.5x768px"));
	EXPECT_FALSE(ParseSize("100%x50%"));
	EXPECT_FALSE(ParseSize("abcx768"));
}

TEST(WindowGeometry, ParseSizeNotPositive)
{
	EXPECT_FALSE(ParseSize("0x768"));
	EXPECT_FALSE(ParseSize("1024x0"));
	EXPECT_FALSE(ParseSize("-1024x768"));
	EXPECT_FALSE(ParseSize("0x50%"));
	EXPECT_FALSE(ParseSize("-10x50%"));
}

TEST(WindowGeometry, ParseSizePercentageOutOfRange)
{
	EXPECT_FALSE(ParseSize("1001x100%"));
	EXPECT_FALSE(ParseSize("infx100%"));
	EXPECT_FALSE(ParseSize("nanx100%"));
}

// ----------------------------------------------------------------------------
// ParsePosition
// ----------------------------------------------------------------------------

TEST(WindowGeometry, ParsePositionLogicalUnits)
{
	expect_setting(ParsePosition("250,100"), 250, 100, Unit::LogicalUnits);
	expect_setting(ParsePosition("250, 100"), 250, 100, Unit::LogicalUnits);
	expect_setting(ParsePosition("0,0"), 0, 0, Unit::LogicalUnits);
}

TEST(WindowGeometry, ParsePositionPixels)
{
	expect_setting(ParsePosition("375,150px"), 375, 150, Unit::Pixels);
}

TEST(WindowGeometry, ParsePositionPercentage)
{
	expect_setting(ParsePosition("12.5,10%"), 12.5f, 10, Unit::Percentage);
}

TEST(WindowGeometry, ParsePositionInvalid)
{
	EXPECT_FALSE(ParsePosition(""));
	EXPECT_FALSE(ParsePosition("auto"));
	EXPECT_FALSE(ParsePosition("250"));
	EXPECT_FALSE(ParsePosition("250x100"));
	EXPECT_FALSE(ParsePosition("250,100,50"));
	EXPECT_FALSE(ParsePosition("-1,100"));
	EXPECT_FALSE(ParsePosition("250,-1px"));
	EXPECT_FALSE(ParsePosition("-1,10%"));
}

// ----------------------------------------------------------------------------
// SizeToLogicalUnits
// ----------------------------------------------------------------------------

TEST(WindowGeometry, SizeToLogicalUnits)
{
	expect_point(SizeToLogicalUnits({1024, 768, Unit::LogicalUnits}, TestDesktop),
	             1024,
	             768);
}

TEST(WindowGeometry, SizeFromPixelsToLogicalUnits)
{
	expect_point(SizeToLogicalUnits({1440, 1080, Unit::Pixels}, TestDesktop),
	             960,
	             720);
}

TEST(WindowGeometry, SizeFromPercentageToLogicalUnits)
{
	// Both width and height are relative to the desktop height
	expect_point(SizeToLogicalUnits({133.33f, 100, Unit::Percentage}, TestDesktop),
	             960,
	             720);

	expect_point(SizeToLogicalUnits({50, 50, Unit::Percentage}, TestDesktop),
	             360,
	             360);
}

// ----------------------------------------------------------------------------
// PositionToLogicalUnits
// ----------------------------------------------------------------------------

TEST(WindowGeometry, PositionToLogicalUnits)
{
	expect_point(PositionToLogicalUnits({250, 100, Unit::LogicalUnits}, TestDesktop),
	             250,
	             100);
}

TEST(WindowGeometry, PositionFromPixelsToLogicalUnits)
{
	expect_point(PositionToLogicalUnits({375, 150, Unit::Pixels}, TestDesktop),
	             250,
	             100);
}

TEST(WindowGeometry, PositionFromPercentageToLogicalUnits)
{
	// X is relative to the desktop width, Y to the desktop height
	expect_point(PositionToLogicalUnits({50, 50, Unit::Percentage}, TestDesktop),
	             640,
	             360);
}
