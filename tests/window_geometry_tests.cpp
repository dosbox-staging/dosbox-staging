// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/private/window_geometry.h"

#include <gtest/gtest.h>

using namespace WindowGeometry;

namespace {

void expect_size_setting(const std::optional<SizeSetting>& actual,
                         const float w, const float h, const Unit unit)
{
	ASSERT_TRUE(actual.has_value());
	EXPECT_FLOAT_EQ(actual->w, w);
	EXPECT_FLOAT_EQ(actual->h, h);
	EXPECT_EQ(actual->unit, unit);
}

void expect_position_setting(const std::optional<PositionSetting>& actual,
                             const float x, const float y, const Unit unit)
{
	ASSERT_TRUE(actual.has_value());
	EXPECT_FLOAT_EQ(actual->x, x);
	EXPECT_FLOAT_EQ(actual->y, y);
	EXPECT_EQ(actual->unit, unit);
}

void expect_size(const SDL_Rect actual, const int w, const int h)
{
	EXPECT_EQ(actual.w, w);
	EXPECT_EQ(actual.h, h);
}

void expect_point(const SDL_Point actual, const int x, const int y)
{
	EXPECT_EQ(actual.x, x);
	EXPECT_EQ(actual.y, y);
}

// 1080p desktop at 150% display scaling
constexpr Desktop TestDesktop = {1280.0f, 720.0f, 1.5f};

SDL_DisplayMode make_display_mode(const int w, const int h, const float pixel_density)
{
	SDL_DisplayMode mode = {};
	mode.w               = w;
	mode.h               = h;
	mode.pixel_density   = pixel_density;
	return mode;
}

} // namespace

// ----------------------------------------------------------------------------
// CalcDesktop
// ----------------------------------------------------------------------------

// The Windows and macOS examples are from SDL's README-highdpi.md; both are
// 3840x2160 pixel displays at 200% scaling

TEST(WindowGeometry, CalcDesktopWindows)
{
	// The desktop mode size is in pixels, the content scale is the OS-level
	// display scaling factor
	const auto desktop = CalcDesktop(make_display_mode(3840, 2160, 1.0f), 2.0f);

	EXPECT_FLOAT_EQ(desktop.width, 1920.0f);
	EXPECT_FLOAT_EQ(desktop.height, 1080.0f);
	EXPECT_FLOAT_EQ(desktop.display_scale, 2.0f);
}

TEST(WindowGeometry, CalcDesktopMacOs)
{
	// The desktop mode size is in logical units, the content scale is
	// always 1.0
	const auto desktop = CalcDesktop(make_display_mode(1920, 1080, 2.0f), 1.0f);

	EXPECT_FLOAT_EQ(desktop.width, 1920.0f);
	EXPECT_FLOAT_EQ(desktop.height, 1080.0f);
	EXPECT_FLOAT_EQ(desktop.display_scale, 2.0f);
}

TEST(WindowGeometry, CalcDesktopFractionalScale)
{
	// 1440p display at 175% scaling on Windows
	const auto desktop = CalcDesktop(make_display_mode(2560, 1440, 1.0f), 1.75f);

	// The logical size is not rounded, so it converts back to the exact size
	// in pixels
	EXPECT_FLOAT_EQ(desktop.width * desktop.display_scale, 2560.0f);
	EXPECT_FLOAT_EQ(desktop.height * desktop.display_scale, 1440.0f);
}

// ----------------------------------------------------------------------------
// ParseWindowSizeSetting
// ----------------------------------------------------------------------------

TEST(WindowGeometry, ParseWindowSizeSettingLogicalUnits)
{
	expect_size_setting(ParseWindowSizeSetting("1024x768"), 1024, 768, Unit::LogicalUnits);
	expect_size_setting(ParseWindowSizeSetting("1024X768"), 1024, 768, Unit::LogicalUnits);
	expect_size_setting(ParseWindowSizeSetting("1024,768"), 1024, 768, Unit::LogicalUnits);
	expect_size_setting(ParseWindowSizeSetting("1024, 768"), 1024, 768, Unit::LogicalUnits);
}

TEST(WindowGeometry, ParseWindowSizeSettingPixels)
{
	expect_size_setting(ParseWindowSizeSetting("1440x1080px"), 1440, 1080, Unit::Pixels);
	expect_size_setting(ParseWindowSizeSetting("1440x1080PX"), 1440, 1080, Unit::Pixels);
}

TEST(WindowGeometry, ParseWindowSizeSettingPercentage)
{
	expect_size_setting(ParseWindowSizeSetting("133x100%"), 133, 100, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("66.5x50%"), 66.5f, 50, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("1000x1000%"), 1000, 1000, Unit::Percentage);
}

TEST(WindowGeometry, ParseWindowSizeSettingInvalid)
{
	EXPECT_FALSE(ParseWindowSizeSetting(""));
	EXPECT_FALSE(ParseWindowSizeSetting("default"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024x"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024x768x2"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024x768x"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024x768pt"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024.5x768"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024.5x768px"));
	EXPECT_FALSE(ParseWindowSizeSetting("100%x50%"));
	EXPECT_FALSE(ParseWindowSizeSetting("abcx768"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024 x 768"));
	EXPECT_FALSE(ParseWindowSizeSetting("1440x1080 px"));
}

TEST(WindowGeometry, ParseWindowSizeSettingNotPositive)
{
	EXPECT_FALSE(ParseWindowSizeSetting("0x768"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024x0"));
	EXPECT_FALSE(ParseWindowSizeSetting("-1024x768"));
	EXPECT_FALSE(ParseWindowSizeSetting("0x50%"));
	EXPECT_FALSE(ParseWindowSizeSetting("-10x50%"));
}

TEST(WindowGeometry, ParseWindowSizeSettingOutOfRange)
{
	expect_size_setting(ParseWindowSizeSetting("65535x65535"), 65535, 65535, Unit::LogicalUnits);

	EXPECT_FALSE(ParseWindowSizeSetting("65536x768"));
	EXPECT_FALSE(ParseWindowSizeSetting("1024x65536px"));
	EXPECT_FALSE(ParseWindowSizeSetting("4000000000x768"));
}

TEST(WindowGeometry, ParseWindowSizeSettingPercentageOutOfRange)
{
	EXPECT_FALSE(ParseWindowSizeSetting("1001x100%"));
	EXPECT_FALSE(ParseWindowSizeSetting("infx100%"));
	EXPECT_FALSE(ParseWindowSizeSetting("nanx100%"));
}

// ----------------------------------------------------------------------------
// ParseWindowPositionSetting
// ----------------------------------------------------------------------------

TEST(WindowGeometry, ParseWindowPositionSettingLogicalUnits)
{
	expect_position_setting(ParseWindowPositionSetting("250,100"), 250, 100, Unit::LogicalUnits);
	expect_position_setting(ParseWindowPositionSetting("250, 100"), 250, 100, Unit::LogicalUnits);
	expect_position_setting(ParseWindowPositionSetting("0,0"), 0, 0, Unit::LogicalUnits);
}

TEST(WindowGeometry, ParseWindowPositionSettingPixels)
{
	expect_position_setting(ParseWindowPositionSetting("375,150px"), 375, 150, Unit::Pixels);
}

TEST(WindowGeometry, ParseWindowPositionSettingPercentage)
{
	expect_position_setting(ParseWindowPositionSetting("12.5,10%"), 12.5f, 10, Unit::Percentage);
}

TEST(WindowGeometry, ParseWindowPositionSettingInvalid)
{
	EXPECT_FALSE(ParseWindowPositionSetting(""));
	EXPECT_FALSE(ParseWindowPositionSetting("auto"));
	EXPECT_FALSE(ParseWindowPositionSetting("250"));
	EXPECT_FALSE(ParseWindowPositionSetting("250x100"));
	EXPECT_FALSE(ParseWindowPositionSetting("250,100,50"));
	EXPECT_FALSE(ParseWindowPositionSetting("-1,100"));
	EXPECT_FALSE(ParseWindowPositionSetting("250,-1px"));
	EXPECT_FALSE(ParseWindowPositionSetting("-1,10%"));
	EXPECT_FALSE(ParseWindowPositionSetting("65536,0"));
}

// ----------------------------------------------------------------------------
// SizeToLogicalUnits
// ----------------------------------------------------------------------------

TEST(WindowGeometry, SizeToLogicalUnits)
{
	expect_size(SizeToLogicalUnits({1024, 768, Unit::LogicalUnits}, TestDesktop),
	             1024,
	             768);
}

TEST(WindowGeometry, SizeFromPixelsToLogicalUnits)
{
	expect_size(SizeToLogicalUnits({1440, 1080, Unit::Pixels}, TestDesktop),
	             960,
	             720);
}

TEST(WindowGeometry, SizeFromPercentageToLogicalUnits)
{
	// Both width and height are relative to the desktop height
	expect_size(SizeToLogicalUnits({133.33f, 100, Unit::Percentage}, TestDesktop),
	             960,
	             720);

	expect_size(SizeToLogicalUnits({50, 50, Unit::Percentage}, TestDesktop),
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
