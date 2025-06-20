/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2025  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_CHANNEL_NAMES_H
#define DOSBOX_CHANNEL_NAMES_H

#include <vector>

namespace ChannelName {

// The CDAUDIO channel has numbered suffixes (ex. CDAUDIO_0, CDAUDIO_1) appened
// for physical CDROM devices. This is to avoid conflicts with multiple drives
// and CDROM_Interface_Image (which does not have a suffix).
constexpr auto CdAudio           = "CDAUDIO";
constexpr auto MaxCdAudioChannel = 32;

constexpr auto Cms                  = "CMS";
constexpr auto CovoxDac             = "COVOX";
constexpr auto DiskNoise            = "DISKNOISE";
constexpr auto DisneySoundSourceDac = "DISNEY";
constexpr auto FluidSynth           = "FSYNTH";
constexpr auto GravisUltrasound     = "GUS";
constexpr auto IbmMusicFeatureCard  = "IMFC";
constexpr auto InnovationSsi2001    = "INNOVATION";
constexpr auto Master               = "MASTER";
constexpr auto Opl                  = "OPL";
constexpr auto PcSpeaker            = "PCSPEAKER";
constexpr auto Ps1AudioCardDac      = "PS1DAC";
constexpr auto Ps1AudioCardPsg      = "PS1";
constexpr auto ReelMagic            = "REELMAGIC";
constexpr auto RolandMt32           = "MT32";
constexpr auto SoundBlasterDac      = "SB";
constexpr auto SoundCanvas          = "SOUNDCANVAS";
constexpr auto StereoOn1Dac         = "STON1";
constexpr auto TandyDac             = "TANDYDAC";
constexpr auto TandyPsg             = "TANDY";

} // namespace ChannelName

// A list of all possible mixer channel names (for enhancements only;
// core functionality should still work even if this list is completely
// empty).
//
// Currently, this is only used by the MIXER command to report errors more
// accurately. The worst that can happen if a new channel is not added to the
// list is that certain error messages will be a little less helpful. Because of
// how this list is used, inventing a mechanism to build this list automatically
// at startup would be overkill.
//
// clang-format off
static std::vector<std::string> AllChannelNames = {
        ChannelName::CdAudio,
        ChannelName::Cms,
        ChannelName::CovoxDac,
        ChannelName::DiskNoise,
        ChannelName::DisneySoundSourceDac,
        ChannelName::FluidSynth,
        ChannelName::GravisUltrasound,
        ChannelName::IbmMusicFeatureCard,
        ChannelName::InnovationSsi2001,
        ChannelName::Master,
        ChannelName::Opl,
        ChannelName::PcSpeaker,
        ChannelName::Ps1AudioCardDac,
        ChannelName::Ps1AudioCardPsg,
        ChannelName::ReelMagic,
        ChannelName::RolandMt32,
        ChannelName::SoundBlasterDac,
        ChannelName::SoundCanvas,
        ChannelName::StereoOn1Dac,
        ChannelName::TandyDac,
        ChannelName::TandyPsg,
};
// clang-format on

#endif // DOSBOX_CHANNEL_NAMES_H
