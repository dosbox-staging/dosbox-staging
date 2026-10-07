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

void expect_size(const SDL_FRect actual, const float w, const float h)
{
	constexpr auto Tolerance = 0.001f;

	EXPECT_NEAR(actual.w, w, Tolerance);
	EXPECT_NEAR(actual.h, h, Tolerance);
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
// LogicalToNative & NativeToLogical
// ----------------------------------------------------------------------------

TEST(WindowGeometry, LogicalToNative)
{
	EXPECT_EQ(LogicalToNative(1000, 1.0f), 1000);
	EXPECT_EQ(LogicalToNative(1000, 1.5f), 1500);
	EXPECT_EQ(LogicalToNative(853, 1.5f), 1280);
}

TEST(WindowGeometry, NativeToLogical)
{
	EXPECT_EQ(NativeToLogical(1000, 1.0f), 1000);
	EXPECT_EQ(NativeToLogical(1500, 1.5f), 1000);
	EXPECT_EQ(NativeToLogical(1280, 1.5f), 853);
}

TEST(WindowGeometry, LogicalToNativeKeepsSpecialPositions)
{
	for (const auto position : {static_cast<int>(SDL_WINDOWPOS_UNDEFINED),
	                            static_cast<int>(SDL_WINDOWPOS_CENTERED),
	                            static_cast<int>(SDL_WINDOWPOS_UNDEFINED_DISPLAY(2)),
	                            static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(2))}) {
		EXPECT_EQ(LogicalToNative(position, 1.5f), position);
	}
}

// Converting from logical units to native units and back must result in the
// original value, otherwise window sizes and positions would change slightly
// every time we convert them back and forth.
TEST(WindowGeometry, LogicalToNativeRoundTrip)
{
	for (const auto content_scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 3.0f}) {
		for (auto logical = -4000; logical <= 8000; ++logical) {
			const auto native = LogicalToNative(logical, content_scale);

			ASSERT_EQ(NativeToLogical(native, content_scale), logical)
			        << "content scale: " << content_scale;
		}
	}
}

// ----------------------------------------------------------------------------
// DisplayPositionToNative & NativeToDisplayPosition
// ----------------------------------------------------------------------------

// Windows example: 4K primary display at 150% scaling, 1440p display at 100%
// scaling to its left, and 1080p display at 125% scaling to its right (display
// bounds are in pixels on Windows)
constexpr SDL_Rect PrimaryDisplay = {0, 0, 3840, 2160};
constexpr SDL_Rect LeftDisplay    = {-2560, 0, 2560, 1440};
constexpr SDL_Rect RightDisplay   = {3840, 0, 1920, 1080};

TEST(WindowGeometry, DisplayPositionToNative)
{
	expect_point(DisplayPositionToNative({100, 50}, PrimaryDisplay, 1.5f), 150, 75);
	expect_point(DisplayPositionToNative({100, 50}, LeftDisplay, 1.0f), -2460, 50);
	expect_point(DisplayPositionToNative({100, 50}, RightDisplay, 1.25f), 3965, 63);
}

TEST(WindowGeometry, NativeToDisplayPosition)
{
	expect_point(NativeToDisplayPosition({150, 75}, PrimaryDisplay, 1.5f), 100, 50);
	expect_point(NativeToDisplayPosition({-2460, 50}, LeftDisplay, 1.0f), 100, 50);
	expect_point(NativeToDisplayPosition({3965, 63}, RightDisplay, 1.25f), 100, 50);

	// The window can be partially off-screen to the left or the top of the
	// display
	expect_point(NativeToDisplayPosition({-30, -15}, PrimaryDisplay, 1.5f), -20, -10);
}

TEST(WindowGeometry, DisplayPositionToNativeKeepsSpecialPositions)
{
	for (const auto position : {static_cast<int>(SDL_WINDOWPOS_UNDEFINED),
	                            static_cast<int>(SDL_WINDOWPOS_CENTERED),
	                            static_cast<int>(SDL_WINDOWPOS_UNDEFINED_DISPLAY(2)),
	                            static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(2))}) {
		expect_point(DisplayPositionToNative({position, position}, RightDisplay, 1.25f),
		             position,
		             position);
	}
}

TEST(WindowGeometry, DisplayPositionRoundTrip)
{
	for (const auto& display : {PrimaryDisplay, LeftDisplay, RightDisplay}) {
		for (const auto content_scale : {1.0f, 1.25f, 1.5f, 2.0f, 2.25f, 3.0f}) {
			for (auto logical = -1000; logical <= 4000; ++logical) {
				const auto native = DisplayPositionToNative({logical, logical},
				                                            display,
				                                            content_scale);

				const auto position = NativeToDisplayPosition(native,
				                                              display,
				                                              content_scale);

				ASSERT_EQ(position.x, logical) << "content scale: " << content_scale;
				ASSERT_EQ(position.y, logical) << "content scale: " << content_scale;
			}
		}
	}
}

TEST(WindowGeometry, NativeSizeRoundTrip)
{
	for (const auto content_scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 3.0f}) {
		for (auto native = 1; native <= 8000; ++native) {
			const auto logical = NativeSizeToLogical(native, content_scale);

			ASSERT_EQ(LogicalSizeToNative(logical, content_scale), native)
			        << "content scale: " << content_scale;
		}
	}
}

// Window sizes in pixels must be restored exactly on Windows and X11, where
// the native units are pixels.
TEST(WindowGeometry, PixelSizeRoundTrip)
{
	for (const auto content_scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 3.0f}) {
		const Desktop desktop = {2560.0f / content_scale, 1440.0f / content_scale, content_scale};

		for (auto px = 1; px <= 8000; ++px) {
			const auto px_float = static_cast<float>(px);
			const auto size = SizeToLogicalUnits({px_float, px_float, Unit::Pixels}, desktop);

			ASSERT_EQ(LogicalSizeToNative(size.w, content_scale), px)
			        << "content scale: " << content_scale;
		}
	}
}

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

TEST(WindowGeometry, ParseWindowSizeSettingNamedSizes)
{
	// Named sizes are 4:3 window sizes relative to the desktop height
	constexpr auto Small  = 50.0f;
	constexpr auto Medium = 74.0f;
	constexpr auto Large  = 90.0f;

	expect_size_setting(ParseWindowSizeSetting("small"), Small * 4 / 3, Small, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("s"), Small * 4 / 3, Small, Unit::Percentage);

	expect_size_setting(ParseWindowSizeSetting("medium"), Medium * 4 / 3, Medium, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("m"), Medium * 4 / 3, Medium, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("default"), Medium * 4 / 3, Medium, Unit::Percentage);

	expect_size_setting(ParseWindowSizeSetting("large"), Large * 4 / 3, Large, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("l"), Large * 4 / 3, Large, Unit::Percentage);

	expect_size_setting(ParseWindowSizeSetting("Medium"), Medium * 4 / 3, Medium, Unit::Percentage);
	expect_size_setting(ParseWindowSizeSetting("LARGE"), Large * 4 / 3, Large, Unit::Percentage);
}

TEST(WindowGeometry, ParseWindowSizeSettingInvalid)
{
	EXPECT_FALSE(ParseWindowSizeSetting(""));
	EXPECT_FALSE(ParseWindowSizeSetting("desktop"));
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
// ParseViewportPositionSetting
// ----------------------------------------------------------------------------

TEST(WindowGeometry, ParseViewportPositionSetting)
{
	expect_position_setting(ParseViewportPositionSetting("0,0"), 0, 0, Unit::LogicalUnits);
	expect_position_setting(ParseViewportPositionSetting("-100, 50"), -100, 50, Unit::LogicalUnits);
	expect_position_setting(ParseViewportPositionSetting("10,-5px"), 10, -5, Unit::Pixels);
	expect_position_setting(ParseViewportPositionSetting("-12.5,10%"), -12.5f, 10, Unit::Percentage);
	expect_position_setting(ParseViewportPositionSetting("-65535,65535"), -65535, 65535, Unit::LogicalUnits);
}

TEST(WindowGeometry, ParseViewportPositionSettingInvalid)
{
	EXPECT_FALSE(ParseViewportPositionSetting(""));
	EXPECT_FALSE(ParseViewportPositionSetting("100"));
	EXPECT_FALSE(ParseViewportPositionSetting("100x50"));
	EXPECT_FALSE(ParseViewportPositionSetting("1,2,3"));
	EXPECT_FALSE(ParseViewportPositionSetting("-10.5,0px"));
	EXPECT_FALSE(ParseViewportPositionSetting("-65536,0"));
	EXPECT_FALSE(ParseViewportPositionSetting("-1001,0%"));
	EXPECT_FALSE(ParseViewportPositionSetting("-inf,0%"));
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
	            959.976f,
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

// ----------------------------------------------------------------------------
// FormatSize
// ----------------------------------------------------------------------------

TEST(WindowGeometry, FormatSize)
{
	EXPECT_EQ(FormatSize({0, 0, 960, 720}, Unit::LogicalUnits, TestDesktop), "960x720");
	EXPECT_EQ(FormatSize({0, 0, 960, 720}, Unit::Pixels, TestDesktop), "1440x1080px");

	EXPECT_EQ(FormatSize({0, 0, 960, 720}, Unit::Percentage, TestDesktop),
	          "133.33x100%");

	EXPECT_EQ(FormatSize({0, 0, 360, 360}, Unit::Percentage, TestDesktop), "50x50%");
}

TEST(WindowGeometry, FormatSizeRoundTrip)
{
	for (const auto& value :
	     {"1024x768", "1440x1080px", "50x50%", "133.33x100%"}) {
		const auto size = ParseWindowSizeSetting(value);
		ASSERT_TRUE(size.has_value());

		const auto logical_size = SizeToLogicalUnits(*size, TestDesktop);
		EXPECT_EQ(FormatSize(logical_size, size->unit, TestDesktop), value);
	}
}

// ----------------------------------------------------------------------------
// FormatPosition
// ----------------------------------------------------------------------------

TEST(WindowGeometry, FormatPosition)
{
	EXPECT_EQ(FormatPosition({250, 100}, Unit::LogicalUnits, TestDesktop),
	          "250,100");

	EXPECT_EQ(FormatPosition({250, 100}, Unit::Pixels, TestDesktop), "375,150px");
	EXPECT_EQ(FormatPosition({640, 360}, Unit::Percentage, TestDesktop), "50,50%");

	EXPECT_EQ(FormatPosition({100, 100}, Unit::Percentage, TestDesktop),
	          "7.81,13.89%");
}

TEST(WindowGeometry, FormatPositionRoundTrip)
{
	for (const auto& value : {"250,100", "375,150px", "50,50%", "12.5,10%"}) {
		const auto position = ParseWindowPositionSetting(value);
		ASSERT_TRUE(position.has_value());

		const auto logical_position = PositionToLogicalUnits(*position,
		                                                     TestDesktop);
		EXPECT_EQ(FormatPosition(logical_position, position->unit, TestDesktop),
		          value);
	}
}
