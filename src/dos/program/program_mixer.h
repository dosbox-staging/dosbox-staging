// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_MIXER_H
#define DOSBOX_PROGRAM_MIXER_H

#include "programs.h"

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "audio_frame.h"
#include "audio/mixer.h"

constexpr auto GlobalVirtualChannelName = "*";

using ChannelInfosMap = std::map<std::string, std::set<ChannelFeature>>;

class ChannelInfos {
public:
	ChannelInfos(const ChannelInfosMap& channel_infos);

	bool HasChannel(const std::string& channel_name) const;

	bool HasFeature(const std::string& channel_name,
	                const ChannelFeature feature) const;

private:
	ChannelInfosMap features_by_channel_name = {};
};

namespace MixerCommand {

struct SelectChannel {
	std::string channel_name = {};
	bool operator==(const SelectChannel that) const;
};

struct SetVolume {
	AudioFrame volume_as_gain = {};
	bool operator==(const SetVolume that) const;
};

struct SetStereoMode {
	StereoLine lineout_map = {};
	bool operator==(const SetStereoMode that) const;
};

struct SetCrossfeedStrength {
	// 0.0 to 1.0
	float strength = {};
	bool operator==(const SetCrossfeedStrength that) const;
};

struct SetReverbLevel {
	// 0.0 to 1.0
	float level = {};
	bool operator==(const SetReverbLevel that) const;
};

struct SetChorusLevel {
	// 0.0 to 1.0
	float level = {};
	bool operator==(const SetChorusLevel that) const;
};

using Command = std::variant<SelectChannel, SetVolume, SetStereoMode,
                             SetCrossfeedStrength, SetReverbLevel, SetChorusLevel>;

enum class ErrorType {
	InactiveChannel,

	InvalidGlobalCommand,
	InvalidMasterChannelCommand,
	InvalidChannelCommand,
	MissingChannelCommand,

	InvalidGlobalCrossfeedStrength,
	InvalidGlobalReverbLevel,
	InvalidGlobalChorusLevel,

	InvalidCrossfeedStrength,
	InvalidReverbLevel,
	InvalidChorusLevel,

	MissingCrossfeedStrength,
	MissingReverbLevel,
	MissingChorusLevel,

	InvalidVolumeCommand,
};

struct Executor {
	void operator()(const SelectChannel cmd);
	void operator()(const SetVolume cmd);
	void operator()(const SetStereoMode cmd);
	void operator()(const SetCrossfeedStrength cmd);
	void operator()(const SetReverbLevel cmd);
	void operator()(const SetChorusLevel cmd);

private:
	bool global_command = false;

	// If 'master_channel' is true, then the MASTER channel is selected,
	// otherwise 'channel' points to the selected non-master channel.
	bool master_channel = false;

	std::shared_ptr<MixerChannel> channel = {};
};

std::variant<ErrorType, std::queue<Command>> ParseCommands(
        const std::vector<std::string>& args, const ChannelInfos& channel_infos,
        const std::vector<std::string>& all_channel_names);

void ExecuteCommands(Executor& executor, std::queue<Command>& commands);

} // namespace MixerCommand

class MIXER final : public Program {
public:
	MIXER()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::Dosbox,
		               HELP_CmdType::Program,
		               "MIXER"};
	}
	void Run() override;

private:
	void ShowMixerStatus();

	static void AddMessages();
};

#endif
