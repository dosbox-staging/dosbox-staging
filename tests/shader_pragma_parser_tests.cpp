// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/render/private/shader_pragma_parser.h"

#include <gtest/gtest.h>

#include <string>

using namespace ShaderPragma;

// ----------------------------------------------------------------------------
// Defaults (no pragmas)
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, EmptySource)
{
	const auto result = Parse("test", "");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->pass_name, "");
	EXPECT_EQ(result->input_ids, (std::vector<std::string>{"Previous"}));
	EXPECT_EQ(result->output_size, ShaderOutputSize::Previous);
	EXPECT_TRUE(result->preset.params.empty());
}

TEST(ShaderPragmaParser, NoPragmasInSource)
{
	const std::string source =
	        "void main() {\n"
	        "    gl_FragColor = vec4(1.0);\n"
	        "}\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->pass_name, "");
	EXPECT_EQ(result->input_ids, (std::vector<std::string>{"Previous"}));
	EXPECT_EQ(result->output_size, ShaderOutputSize::Previous);
	EXPECT_TRUE(result->preset.params.empty());
}

// ----------------------------------------------------------------------------
// #pragma name
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, NameValid)
{
	const auto result = Parse("test", "#pragma name Main_Pass1\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->pass_name, "Main_Pass1");
}

TEST(ShaderPragmaParser, NameInBlockComment)
{
	// Per the README convention: pragmas are wrapped in a block comment,
	// each on its own line, so the GLSL compiler ignores them while we
	// still pick them up.
	const std::string source =
	        "/*\n"
	        "#pragma name Wrapped_Pass\n"
	        "*/\n"
	        "void main() {}\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->pass_name, "Wrapped_Pass");
}

TEST(ShaderPragmaParser, NameMissingValue)
{
	EXPECT_FALSE(Parse("test", "#pragma name\n").has_value());
}

TEST(ShaderPragmaParser, NameTooManyTokens)
{
	EXPECT_FALSE(Parse("test", "#pragma name Foo Bar\n").has_value());
}

// ----------------------------------------------------------------------------
// #pragma inputN
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, SingleInput)
{
	const auto result = Parse("test", "#pragma input0 Main_Pass2\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->input_ids, (std::vector<std::string>{"Main_Pass2"}));
}

TEST(ShaderPragmaParser, TwoInputsInOrder)
{
	const std::string source =
	        "#pragma input0 Previous\n"
	        "#pragma input1 Helper_Pass\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->input_ids,
	          (std::vector<std::string>{"Previous", "Helper_Pass"}));
}

TEST(ShaderPragmaParser, TwoInputsReversedOrder)
{
	// Indices, not declaration order, determine slot.
	const std::string source =
	        "#pragma input1 Helper_Pass\n"
	        "#pragma input0 Previous\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->input_ids,
	          (std::vector<std::string>{"Previous", "Helper_Pass"}));
}

TEST(ShaderPragmaParser, NonContiguousInputIndices)
{
	// input0 + input2, missing input1.
	const std::string source =
	        "#pragma input0 Previous\n"
	        "#pragma input2 Helper_Pass\n";

	EXPECT_FALSE(Parse("test", source).has_value());
}

TEST(ShaderPragmaParser, InputNonNumericIndex)
{
	EXPECT_FALSE(Parse("test", "#pragma inputX Previous\n").has_value());
}

TEST(ShaderPragmaParser, InputMissingName)
{
	EXPECT_FALSE(Parse("test", "#pragma input0\n").has_value());
}

// ----------------------------------------------------------------------------
// #pragma output_size
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, OutputSizePrevious)
{
	const auto result = Parse("test", "#pragma output_size Previous\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->output_size, ShaderOutputSize::Previous);
}

TEST(ShaderPragmaParser, OutputSizeRendered)
{
	const auto result = Parse("test", "#pragma output_size Rendered\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->output_size, ShaderOutputSize::Rendered);
}

TEST(ShaderPragmaParser, OutputSizeVideoMode)
{
	const auto result = Parse("test", "#pragma output_size VideoMode\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->output_size, ShaderOutputSize::VideoMode);
}

TEST(ShaderPragmaParser, OutputSizeViewport)
{
	const auto result = Parse("test", "#pragma output_size Viewport\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->output_size, ShaderOutputSize::Viewport);
}

TEST(ShaderPragmaParser, OutputSizeInvalidValue)
{
	EXPECT_FALSE(Parse("test", "#pragma output_size Banana\n").has_value());
}

TEST(ShaderPragmaParser, OutputSizeMissingValue)
{
	EXPECT_FALSE(Parse("test", "#pragma output_size\n").has_value());
}

// ----------------------------------------------------------------------------
// #pragma parameter
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, ParameterValid)
{
	const auto result = Parse(
	        "test",
	        "#pragma parameter OUTPUT_GAMMA \"OUTPUT GAMMA\" 2.2 0.0 5.0 0.1\n");

	ASSERT_TRUE(result.has_value());
	ASSERT_TRUE(result->preset.params.contains("OUTPUT_GAMMA"));
	EXPECT_FLOAT_EQ(result->preset.params.at("OUTPUT_GAMMA"), 2.2f);
}

TEST(ShaderPragmaParser, ParameterMultiple)
{
	const std::string source =
	        "#pragma parameter A \"AAA\" 1.0 0.0 2.0 0.5\n"
	        "#pragma parameter B \"BBB\" -3.0 -5.0 0.0 0.25\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());
	EXPECT_FLOAT_EQ(result->preset.params.at("A"), 1.0f);
	EXPECT_FLOAT_EQ(result->preset.params.at("B"), -3.0f);
}

TEST(ShaderPragmaParser, ParameterWrongArgCount)
{
	// Only 3 numbers instead of 4 after the display name.
	EXPECT_FALSE(
	        Parse("test", "#pragma parameter A \"AAA\" 1.0 0.0 2.0\n").has_value());
}

TEST(ShaderPragmaParser, ParameterNonFloatDefault)
{
	EXPECT_FALSE(Parse("test", "#pragma parameter A \"AAA\" notafloat 0.0 2.0 0.5\n")
	                     .has_value());
}

// ----------------------------------------------------------------------------
// Setting pragmas (linear_filtering, float_output, force_*)
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, LinearFilteringOn)
{
	const auto result = Parse("test", "#pragma linear_filtering on\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->preset.settings.texture_filter_mode,
	          TextureFilterMode::Bilinear);
}

TEST(ShaderPragmaParser, LinearFilteringOff)
{
	const auto result = Parse("test", "#pragma linear_filtering off\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->preset.settings.texture_filter_mode,
	          TextureFilterMode::NearestNeighbour);
}

TEST(ShaderPragmaParser, FloatOutputOn)
{
	const auto result = Parse("test", "#pragma float_output on\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->preset.settings.float_output_texture);
}

TEST(ShaderPragmaParser, FloatOutputOff)
{
	const auto result = Parse("test", "#pragma float_output off\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_FALSE(result->preset.settings.float_output_texture);
}

TEST(ShaderPragmaParser, ForceSingleScanOn)
{
	const auto result = Parse("test", "#pragma force_single_scan on\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->preset.settings.force_single_scan);
}

TEST(ShaderPragmaParser, ForceNoPixelDoublingOn)
{
	const auto result = Parse("test", "#pragma force_no_pixel_doubling on\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_TRUE(result->preset.settings.force_no_pixel_doubling);
}

TEST(ShaderPragmaParser, SettingInvalidBool)
{
	EXPECT_FALSE(Parse("test", "#pragma linear_filtering maybe\n").has_value());
}

TEST(ShaderPragmaParser, UnknownSetting)
{
	EXPECT_FALSE(Parse("test", "#pragma what_is_this on\n").has_value());
}

// ----------------------------------------------------------------------------
// SetSetting (also used by [settings] INI section)
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, SetSettingForceSingleScan)
{
	ShaderSettings settings = {};

	EXPECT_TRUE(SetSetting("force_single_scan", "yes", settings));
	EXPECT_TRUE(settings.force_single_scan);

	EXPECT_TRUE(SetSetting("force_single_scan", "no", settings));
	EXPECT_FALSE(settings.force_single_scan);
}

TEST(ShaderPragmaParser, SetSettingLinearFiltering)
{
	ShaderSettings settings = {};

	EXPECT_TRUE(SetSetting("linear_filtering", "on", settings));
	EXPECT_EQ(settings.texture_filter_mode, TextureFilterMode::Bilinear);

	EXPECT_TRUE(SetSetting("linear_filtering", "off", settings));
	EXPECT_EQ(settings.texture_filter_mode, TextureFilterMode::NearestNeighbour);
}

TEST(ShaderPragmaParser, SetSettingUnknownName)
{
	ShaderSettings settings = {};
	EXPECT_FALSE(SetSetting("nope", "on", settings));
}

TEST(ShaderPragmaParser, SetSettingInvalidValue)
{
	ShaderSettings settings = {};
	EXPECT_FALSE(SetSetting("linear_filtering", "maybe", settings));
}

// ----------------------------------------------------------------------------
// #pragma wrap_modeN
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, WrapModeDefaultsToClampToEdge)
{
	const auto result = Parse("test", "");

	ASSERT_TRUE(result.has_value());

	EXPECT_EQ(result->input_wrap_modes,
	          (std::vector<TextureWrapMode>{TextureWrapMode::ClampToEdge}));
}

TEST(ShaderPragmaParser, WrapModeAllValidValues)
{
	const std::string source =
	        "#pragma input0 In0\n"
	        "#pragma input1 In1\n"
	        "#pragma input2 In2\n"
	        "#pragma input3 In3\n"
	        "#pragma wrap_mode0 Repeat\n"
	        "#pragma wrap_mode1 MirroredRepeat\n"
	        "#pragma wrap_mode2 ClampToEdge\n"
	        "#pragma wrap_mode3 ClampToBorder\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());

	using enum TextureWrapMode;
	EXPECT_EQ(result->input_wrap_modes,
	          (std::vector<TextureWrapMode>{
	                  Repeat, MirroredRepeat, ClampToEdge, ClampToBorder}));
}

TEST(ShaderPragmaParser, WrapModeUnspecifiedInputsDefault)
{
	// Two inputs, only the first has a wrap mode set explicitly.
	const std::string source =
	        "#pragma input0 In0\n"
	        "#pragma input1 In1\n"
	        "#pragma wrap_mode0 Repeat\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());

	using enum TextureWrapMode;
	EXPECT_EQ(result->input_wrap_modes,
	          (std::vector<TextureWrapMode>{Repeat, ClampToEdge}));
}

TEST(ShaderPragmaParser, WrapModeWithImplicitDefaultInput)
{
	// No `inputN` pragmas: the implicit `input0 Previous` is used,
	// and `wrap_mode0` applies to it.
	const auto result = Parse("test", "#pragma wrap_mode0 MirroredRepeat\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->input_ids, (std::vector<std::string>{"Previous"}));

	EXPECT_EQ(result->input_wrap_modes,
	          (std::vector<TextureWrapMode>{TextureWrapMode::MirroredRepeat}));
}

TEST(ShaderPragmaParser, WrapModeInvalidValue)
{
	EXPECT_FALSE(Parse("test", "#pragma wrap_mode0 Bogus\n").has_value());
}

TEST(ShaderPragmaParser, WrapModeNonNumericIndex)
{
	EXPECT_FALSE(Parse("test", "#pragma wrap_modeX Repeat\n").has_value());
}

TEST(ShaderPragmaParser, WrapModeMissingValue)
{
	EXPECT_FALSE(Parse("test", "#pragma wrap_mode0\n").has_value());
}

TEST(ShaderPragmaParser, WrapModeIndexOutOfRange)
{
	// Only `input0` exists (implicitly), but wrap_mode5 is specified.
	EXPECT_FALSE(Parse("test", "#pragma wrap_mode5 Repeat\n").has_value());
}

// ----------------------------------------------------------------------------
// Multi-pragma combinations
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, FullShaderHeader)
{
	const std::string source =
	        "/*\n"
	        "#pragma name CrtHyllian_Main\n"
	        "#pragma input0 Previous\n"
	        "#pragma input1 ImageAdjustments\n"
	        "#pragma wrap_mode0 ClampToBorder\n"
	        "#pragma wrap_mode1 Repeat\n"
	        "#pragma output_size Viewport\n"
	        "#pragma linear_filtering off\n"
	        "#pragma force_single_scan on\n"
	        "#pragma float_output on\n"
	        "#pragma parameter PHOSPHOR_LAYOUT \"PHOSPHOR LAYOUT\" 2.0 0.0 19.0 1.0\n"
	        "*/\n"
	        "void main() {}\n";

	const auto result = Parse("test", source);

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->pass_name, "CrtHyllian_Main");

	EXPECT_EQ(result->input_ids,
	          (std::vector<std::string>{"Previous", "ImageAdjustments"}));

	EXPECT_EQ(result->input_wrap_modes,
	          (std::vector<TextureWrapMode>{TextureWrapMode::ClampToBorder,
	                                        TextureWrapMode::Repeat}));

	EXPECT_EQ(result->output_size, ShaderOutputSize::Viewport);

	EXPECT_EQ(result->preset.settings.texture_filter_mode,
	          TextureFilterMode::NearestNeighbour);

	EXPECT_TRUE(result->preset.settings.force_single_scan);
	EXPECT_TRUE(result->preset.settings.float_output_texture);
	EXPECT_FLOAT_EQ(result->preset.params.at("PHOSPHOR_LAYOUT"), 2.0f);
}

TEST(ShaderPragmaParser, AllErrorsReported)
{
	// Two invalid pragmas in the same source. Parsing keeps going,
	// returns empty optional, and (when run) emits two LOG_ERR lines.
	const std::string source =
	        "#pragma name\n"
	        "#pragma output_size Bogus\n";

	EXPECT_FALSE(Parse("test", source).has_value());
}

// ----------------------------------------------------------------------------
// Whitespace and formatting tolerance
// ----------------------------------------------------------------------------

TEST(ShaderPragmaParser, ExtraWhitespaceBetweenTokens)
{
	const auto result = Parse("test", "#pragma    input0    Previous\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->input_ids, (std::vector<std::string>{"Previous"}));
}

TEST(ShaderPragmaParser, LeadingWhitespaceBeforePragma)
{
	const auto result = Parse("test", "    #pragma name Indented_Pass\n");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->pass_name, "Indented_Pass");
}
