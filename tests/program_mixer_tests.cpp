// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/program_mixer.h"
#include "channel_names.h"

#include <gtest/gtest.h>

using namespace MixerCommand;

const auto Minus6Db = powf(10.0f, -6.0f / 20.0f); // ≈ 0.5012 ≈ 0.5
const auto Plus12Db = powf(10.0f, 12.0f / 20.0f); // ≈ 3.9811 ≈ 2.0

static ChannelInfosMap create_default_channel_infos_map()
{
	ChannelInfosMap infos = {};

	infos["SB"] = {ChannelFeature::Stereo,
	               ChannelFeature::ReverbSend,
	               ChannelFeature::ChorusSend};

	// Mono channel
	infos["OPL"] = {ChannelFeature::ReverbSend, ChannelFeature::ChorusSend};

	// Mono channel
	infos["PCSPEAKER"] = {ChannelFeature::ReverbSend, ChannelFeature::ChorusSend};

	// Stereo channel with no reverb & chorus support
	infos["MT32"] = {ChannelFeature::Stereo};

	return infos;
}

static ChannelInfosMap default_channel_infos_map = create_default_channel_infos_map();

static void assert_success(const std::vector<std::string>& args,
                           const std::queue<Command>& expected,
                           const ChannelInfosMap channel_infos_map = default_channel_infos_map)
{
	const auto result = ParseCommands(args,
	                                  ChannelInfos(channel_infos_map),
	                                  AllChannelNames);

	if (auto error_type = std::get_if<ErrorType>(&result); error_type) {
		FAIL();
	} else {
		auto actual = std::get<std::queue<MixerCommand::Command>>(result);
		EXPECT_EQ(actual, expected);
	}
}

static void assert_failure(const std::vector<std::string>& args,
                           const ErrorType expected_error_type,
                           const ChannelInfosMap channel_infos_map = default_channel_infos_map)
{
	const auto result = ParseCommands(args,
	                                  ChannelInfos(channel_infos_map),
	                                  AllChannelNames);

	if (auto error_type = std::get_if<ErrorType>(&result); error_type) {
		EXPECT_EQ(*error_type, expected_error_type);
	} else {
		FAIL();
	}
}

static std::queue<Command> select_channel(const std::string& channel_name)
{
	std::queue<Command> cmd = {};
	cmd.emplace(SelectChannel{GlobalVirtualChannelName});
	cmd.emplace(SelectChannel{channel_name});
	return cmd;
}

static std::queue<Command> select_sb_channel()
{
	return select_channel("SB");
}

static std::queue<Command> select_pcspeaker_channel()
{
	return select_channel("PCSPEAKER");
}

// ************************************************************************
// SUCCESS CASES
// ************************************************************************
//
// Global
TEST(ProgramMixer, Global_SetReverbLevel)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SetReverbLevel{0.2f});

	assert_success({"r20"}, expected);
}

TEST(ProgramMixer, Global_SetChorusLevel)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SetChorusLevel{0.2f});

	assert_success({"c20"}, expected);
}

TEST(ProgramMixer, Global_SetCrossfeedStrength_StereoChannel)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SetCrossfeedStrength{0.2f});

	assert_success({"x20"}, expected);
}

TEST(ProgramMixer, Global_SetAllValid)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SetReverbLevel{0.2f});
	expected.emplace(SetChorusLevel{0.1f});
	expected.emplace(SetCrossfeedStrength{0.3f});

	assert_success({"r20", "c10", "x30"}, expected);
}

TEST(ProgramMixer, Global_SetAllValidMultiple)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SetCrossfeedStrength{0.07f});
	expected.emplace(SetReverbLevel{0.08f});
	expected.emplace(SetCrossfeedStrength{0.30f});
	expected.emplace(SetChorusLevel{0.09f});
	expected.emplace(SetReverbLevel{0.20f});
	expected.emplace(SetCrossfeedStrength{0.10f});

	assert_success({"x7", "r8", "x30", "c9", "r20", "x10"}, expected);
}

// Master
TEST(ProgramMixer, Master_SetVolume)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SelectChannel{ChannelName::Master});
	expected.emplace(SetVolume{AudioFrame(0.2f, 0.2f)});

	assert_success({"master", "20"}, expected);
}

TEST(ProgramMixer, Master_SetVolumeMultiple)
{
	std::queue<Command> expected = {};
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SelectChannel{ChannelName::Master});
	expected.emplace(SetVolume{AudioFrame(0.1f, 0.1f)});
	expected.emplace(SetVolume{AudioFrame(0.2f, 0.2f)});

	assert_success({"master", "10", "20"}, expected);
}

// Channel
TEST(ProgramMixer, Channel_SetVolumePercentMinLimit)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(0.0f, 0.0f)});

	assert_success({"sb", "0"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumePercentMaxLimit)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(99.99f, 99.99f)});

	assert_success({"sb", "9999"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumePercentSingle)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(0.2f, 0.2f)});

	assert_success({"sb", "20"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumePercentSinglePlus)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(0.2f, 0.2f)});

	assert_success({"sb", "+20"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumePercentStereo)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(0.2f, 1.5f)});

	assert_success({"sb", "20:150"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeDecibelMinLimit)
{
	const auto Minus96Db = powf(10.0f, -96.0f / 20.0f); // ≈ 0.0

	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(Minus96Db, Minus96Db)});

	assert_success({"sb", "d-96"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeDecibelMaxLimit)
{
	const auto Plus40Db = 99.99f;

	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(Plus40Db, Plus40Db)});

	assert_success({"sb", "d40"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeDecibelSingle)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(Minus6Db, Minus6Db)});

	assert_success({"sb", "d-6"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeDecibelSinglePlus)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(Plus12Db, Plus12Db)});

	assert_success({"sb", "d+12"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeDecibelStereo)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(Minus6Db, Plus12Db)});

	assert_success({"sb", "d-6:d12"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeDecibelPercentStereo)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(Minus6Db, 1.23f)});

	assert_success({"sb", "d-6:123"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumePercentDecibelStereo)
{
	auto expected = select_sb_channel();
	expected.emplace(SetVolume{AudioFrame(0.4f, Plus12Db)});

	assert_success({"sb", "40:d12"}, expected);
}

TEST(ProgramMixer, Channel_SetVolumeChannelNameStartsWithLetterD)
{
	auto infos = create_default_channel_infos_map();
	infos["DISNEY"] = {ChannelFeature::ReverbSend, ChannelFeature::ChorusSend};

	auto expected = select_channel("DISNEY");
	expected.emplace(SetVolume{AudioFrame(0.0f, 0.0f)});

	assert_success({"disney", "0"}, expected, infos);
}

TEST(ProgramMixer, Channel_SetStereoModeStereo)
{
	auto expected = select_sb_channel();
	expected.emplace(SetStereoMode{StereoMap});

	assert_success({"sb", "stereo"}, expected);
}

TEST(ProgramMixer, Channel_SetStereoModeReverse)
{
	auto expected = select_sb_channel();
	expected.emplace(SetStereoMode{ReverseMap});

	assert_success({"sb", "reverse"}, expected);
}

TEST(ProgramMixer, Channel_SetCrossfeedStrength)
{
	auto expected = select_sb_channel();
	expected.emplace(SetCrossfeedStrength{0.1f});

	assert_success({"sb", "x10"}, expected);
}

TEST(ProgramMixer, Channel_SetCrossfeedStrengthLimits)
{
	auto expected = select_sb_channel();
	expected.emplace(SetCrossfeedStrength{0.0f});
	expected.emplace(SetCrossfeedStrength{1.0f});

	assert_success({"sb", "x0", "x100"}, expected);
}

TEST(ProgramMixer, Channel_SetReverbLevel_StereoChannel)
{
	auto expected = select_sb_channel();
	expected.emplace(SetReverbLevel{0.2f});

	assert_success({"sb", "r20"}, expected);
}

TEST(ProgramMixer, Channel_SetReverbLevel_MonoChannel)
{
	auto expected = select_pcspeaker_channel();
	expected.emplace(SetReverbLevel{0.2f});

	assert_success({"pcspeaker", "r20"}, expected);
}

TEST(ProgramMixer, Channel_SetReverbLevelLimits)
{
	auto expected = select_sb_channel();
	expected.emplace(SetReverbLevel{0.0f});
	expected.emplace(SetReverbLevel{1.0f});

	assert_success({"sb", "r0", "r100"}, expected);
}

TEST(ProgramMixer, Channel_SetChorusLevel_StereoChannel)
{
	auto expected = select_sb_channel();
	expected.emplace(SetChorusLevel{0.2f});

	assert_success({"sb", "c20"}, expected);
}

TEST(ProgramMixer, Channel_SetChorusLevel_MonoChannel)
{
	auto expected = select_pcspeaker_channel();
	expected.emplace(SetChorusLevel{0.2f});

	assert_success({"pcspeaker", "c20"}, expected);
}

TEST(ProgramMixer, Channel_SetChorusLevelLimits)
{
	auto expected = select_sb_channel();
	expected.emplace(SetChorusLevel{0.0f});
	expected.emplace(SetChorusLevel{1.0f});

	assert_success({"sb", "c0", "c100"}, expected);
}

TEST(ProgramMixer, AllCommands)
{
	std::queue<Command> expected = {};
	// Global
	expected.emplace(SelectChannel{GlobalVirtualChannelName});
	expected.emplace(SetCrossfeedStrength{0.07f});
	expected.emplace(SetReverbLevel{0.08f});
	expected.emplace(SetChorusLevel{0.30f});
	// MASTER
	expected.emplace(SelectChannel{ChannelName::Master});
	expected.emplace(SetVolume{AudioFrame(0.1f, Minus6Db)});
	// SB
	expected.emplace(SelectChannel{"SB"});
	expected.emplace(SetChorusLevel{0.09f});
	expected.emplace(SetReverbLevel{0.20f});
	expected.emplace(SetStereoMode{ReverseMap});
	expected.emplace(SetCrossfeedStrength{0.10f});
	expected.emplace(SetVolume{AudioFrame(0.20f, 0.20f)});

	assert_success({"x7", "r8", "c30", "master", "10:d-6", "sb", "c9", "r20", "reverse", "x10", "20"},
	               expected);
}

// ************************************************************************
// FAILURE CASES
// ************************************************************************
//
// Global commands
TEST(ProgramMixer, Global_InvalidSetVolumeCommand)
{
	assert_failure({"10"}, ErrorType::InvalidGlobalCommand);
}

TEST(ProgramMixer, Global_InvalidSetStereoModeCommand)
{
	assert_failure({"stereo"}, ErrorType::InvalidGlobalCommand);
}

TEST(ProgramMixer, Global_InvalidCommand)
{
	assert_failure({"asdf"}, ErrorType::InvalidGlobalCommand);
}

TEST(ProgramMixer, Global_InactiveChannel)
{
	assert_failure({"gus"}, ErrorType::InactiveChannel);
}

TEST(ProgramMixer, Global_InactiveChannelChannelNameStartsWithLetterD)
{
	assert_failure({"disney"}, ErrorType::InactiveChannel);
}

// Master commands
TEST(ProgramMixer, Master_MissingCommand)
{
	assert_failure({"master"}, ErrorType::MissingChannelCommand);
}

TEST(ProgramMixer, Master_InvalidSetStereoModeCommand)
{
	assert_failure({"master", "stereo"}, ErrorType::InvalidChannelCommand);
}

TEST(ProgramMixer, Master_InvalidSetReverbCommand)
{
	assert_failure({"master", "r20"}, ErrorType::InvalidMasterChannelCommand);
}

TEST(ProgramMixer, Master_InvalidSetChorusLevelCommand)
{
	assert_failure({"master", "c20"}, ErrorType::InvalidMasterChannelCommand);
}

TEST(ProgramMixer, Master_InvalidSetCrossfeedStrengthCommand)
{
	assert_failure({"master", "x20"}, ErrorType::InvalidMasterChannelCommand);
}

TEST(ProgramMixer, Master_MissingCommandBeforeChannelCommand)
{
	// "gus" is a valid channel name
	assert_failure({"master", "opl"}, ErrorType::MissingChannelCommand);
}

TEST(ProgramMixer, Master_InvalidCommand)
{
	// "asdf" is not valid channel name
	assert_failure({"master", "asdf"}, ErrorType::InvalidMasterChannelCommand);
}

TEST(ProgramMixer, Master_InvalidSingleLetterCommand)
{
	// valid command prefixes
	assert_failure({"master", "x"}, ErrorType::InvalidMasterChannelCommand);
	assert_failure({"master", "r"}, ErrorType::InvalidMasterChannelCommand);
	assert_failure({"master", "c"}, ErrorType::InvalidMasterChannelCommand);

	assert_failure({"master", "."}, ErrorType::InvalidMasterChannelCommand);
	assert_failure({"master", "$"}, ErrorType::InvalidMasterChannelCommand);
	assert_failure({"master", "w"}, ErrorType::InvalidMasterChannelCommand);
}

TEST(ProgramMixer, Master_InactiveChannel)
{
	assert_failure({"master", "10", "gus"}, ErrorType::InactiveChannel);
}

// Channel commands
TEST(ProgramMixer, Channel_InactiveChannel)
{
	assert_failure({"sb", "10", "gus"}, ErrorType::InactiveChannel);
}

// Set stereo mode
TEST(ProgramMixer, SetStereoModeReverse_InvalidForMonoChannel)
{
	assert_failure({"pcspeaker", "reverse"}, ErrorType::InvalidChannelCommand);
}

// Set volume
TEST(ProgramMixer, SetVolume_InvalidPercentVolume_Over)
{
	assert_failure({"sb", "10000"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidPercentVolume_Negative)
{
	assert_failure({"sb", "-1"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidPercentVolume_ExtraLetters)
{
	assert_failure({"sb", "50ab"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidDecibelVolume_Over)
{
	assert_failure({"sb", "d40.1"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidDecibelVolume_Under)
{
	assert_failure({"sb", "d-96.1"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidDecibelVolume_ExtraLetters)
{
	assert_failure({"sb", "d6ab"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidStereoVolume_RightMissing)
{
	assert_failure({"sb", "10:"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidStereoVolume_LeftMissing)
{
	assert_failure({"sb", ":10"}, ErrorType::InvalidChannelCommand);
}

TEST(ProgramMixer, SetVolume_InvalidStereoVolume_LeftInvalid)
{
	assert_failure({"sb", "10a:20"}, ErrorType::InvalidVolumeCommand);
}

TEST(ProgramMixer, SetVolume_InvalidStereoVolume_RightInvalid)
{
	assert_failure({"sb", "10:20a"}, ErrorType::InvalidVolumeCommand);
}

// Set crossfeed strength
//
TEST(ProgramMixer, SetCrossfeedStrength_MissingStrength_Channel)
{
	assert_failure({"sb", "x"}, ErrorType::MissingCrossfeedStrength);
}

TEST(ProgramMixer, SetCrossfeedStrength_MissingStrength_Global)
{
	assert_failure({"x"}, ErrorType::MissingCrossfeedStrength);
}

TEST(ProgramMixer, SetCrossfeedStrength_InvalidStrength_Over)
{
	assert_failure({"sb", "x101"}, ErrorType::InvalidCrossfeedStrength);
}

TEST(ProgramMixer, SetCrossfeedStrength_InvalidStrength_Under)
{
	assert_failure({"sb", "x-1"}, ErrorType::InvalidCrossfeedStrength);
}

TEST(ProgramMixer, SetCrossfeedStrength_InvalidStrength_Global)
{
	assert_failure({"x-1"}, ErrorType::InvalidGlobalCrossfeedStrength);
}

TEST(ProgramMixer, SetCrossfeedStrength_InvalidStrength_ExtraLetters)
{
	assert_failure({"sb", "x50f"}, ErrorType::InvalidCrossfeedStrength);
}

TEST(ProgramMixer, SetCrossfeedStrength_InvalidForMonoChannel)
{
	assert_failure({"pcspeaker", "x30"}, ErrorType::InvalidChannelCommand);
}

// Set chorus level
//
TEST(ProgramMixer, SetChorusLevel_ChorusNotSupported_MissingLevel)
{
	assert_failure({"mt32", "c"}, ErrorType::InvalidChannelCommand);
}

TEST(ProgramMixer, SetChorusLevel_ChorusNotSupported)
{
	assert_failure({"mt32", "c20"}, ErrorType::InvalidChannelCommand);
}

TEST(ProgramMixer, SetChorusLevel_MissingLevel_Channel)
{
	assert_failure({"sb", "c"}, ErrorType::MissingChorusLevel);
}

TEST(ProgramMixer, SetChorusLevel_MissingLevel_Global)
{
	assert_failure({"c"}, ErrorType::MissingChorusLevel);
}

TEST(ProgramMixer, SetChorusLevel_InvalidLevel_Over)
{
	assert_failure({"sb", "c101"}, ErrorType::InvalidChorusLevel);
}

TEST(ProgramMixer, SetChorusLevel_InvalidLevel_Under)
{
	assert_failure({"sb", "c-1"}, ErrorType::InvalidChorusLevel);
}

TEST(ProgramMixer, SetChorusLevel_InvalidLevel_Global)
{
	assert_failure({"c-1"}, ErrorType::InvalidGlobalChorusLevel);
}

TEST(ProgramMixer, SetChorusLevel_InvalidLevel_ExtraLetters)
{
	assert_failure({"sb", "c50f"}, ErrorType::InvalidChorusLevel);
}

// Set reverb level
//
TEST(ProgramMixer, SetReverbLevel_ReverbNotSupported_MissingLevel)
{
	assert_failure({"mt32", "r"}, ErrorType::InvalidChannelCommand);
}

TEST(ProgramMixer, SetReverbLevel_ReverbNotSupported)
{
	assert_failure({"mt32", "r20"}, ErrorType::InvalidChannelCommand);
}

TEST(ProgramMixer, SetReverbLevel_MissingLevel_Channel)
{
	assert_failure({"sb", "r"}, ErrorType::MissingReverbLevel);
}

TEST(ProgramMixer, SetReverbLevel_MissingLevel_Global)
{
	assert_failure({"r"}, ErrorType::MissingReverbLevel);
}

TEST(ProgramMixer, SetReverbLevel_InvalidLevel_Over)
{
	assert_failure({"sb", "r101"}, ErrorType::InvalidReverbLevel);
}

TEST(ProgramMixer, SetReverbLevel_InvalidLevel_Under)
{
	assert_failure({"sb", "r-1"}, ErrorType::InvalidReverbLevel);
}

TEST(ProgramMixer, SetReverbLevel_InvalidLevel_Global)
{
	assert_failure({"r-1"}, ErrorType::InvalidGlobalReverbLevel);
}

TEST(ProgramMixer, SetReverbLevel_InvalidLevel_ExtraLetters)
{
	assert_failure({"sb", "r50f"}, ErrorType::InvalidReverbLevel);
}

