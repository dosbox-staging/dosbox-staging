// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio/clap/plugin_manager.cpp"
#include "midi/private/clap_synth.h"

#include <algorithm>
#include <utility>

#include <gtest/gtest.h>

namespace {

struct TestPlugin {
	std::vector<uint32_t> input_channels  = {};
	std::vector<uint32_t> output_channels = {2};
	uint32_t note_inputs                  = 1;
	uint32_t note_outputs                 = 0;
	uint32_t first_note_dialects          = CLAP_NOTE_DIALECT_MIDI;
	const char* first_output_type         = CLAP_PORT_STEREO;
	bool has_note_ports                   = true;
	bool has_audio_ports                  = true;
	bool note_query_succeeds              = true;
	int failed_input_port                 = -1;
	int failed_output_port                = -1;
	const clap_process_t* last_process    = nullptr;
	std::vector<uint32_t> block_sizes     = {};
	std::vector<uint16_t> event_types     = {};

	static TestPlugin& Get(const clap_plugin_t* plugin)
	{
		return *static_cast<TestPlugin*>(plugin->plugin_data);
	}

	static void Noop(const clap_plugin_t*) {}

	bool ValidateAudioPorts()
	{
		std::vector<uint32_t> inputs = {}, outputs = {};
		if (!Clap::validate_audio_ports(&plugin, inputs, outputs)) {
			return false;
		}
		EXPECT_EQ(inputs, input_channels);
		EXPECT_EQ(outputs, output_channels);
		return true;
	}

	void Process(const clap_process_t* block)
	{
		last_process = block;
		block_sizes.push_back(block->frames_count);
		const auto* events = block->in_events;
		for (uint32_t i = 0; i < events->size(events); ++i) {
			const auto* header = events->get(events, i);
			ASSERT_NE(header, nullptr);
			event_types.push_back(header->type);
		}
		for (const bool is_input : {true, false}) {
			const auto& counts   = is_input ? input_channels
			                                : output_channels;
			const auto num_ports = is_input
			                             ? block->audio_inputs_count
			                             : block->audio_outputs_count;
			const auto* ports    = is_input ? block->audio_inputs
			                                : block->audio_outputs;
			ASSERT_EQ(num_ports, counts.size());
			if (counts.empty()) {
				EXPECT_EQ(ports, nullptr);
				continue;
			}
			ASSERT_NE(ports, nullptr);
			for (size_t i = 0; i < counts.size(); ++i) {
				const auto& port = ports[i];
				ASSERT_EQ(port.channel_count, counts[i]);
				ASSERT_NE(port.data32, nullptr);
				EXPECT_EQ(port.data64, nullptr);
				for (uint32_t c = 0; c < port.channel_count; ++c) {
					auto* samples = port.data32[c];
					ASSERT_NE(samples, nullptr);
					if (is_input) {
						EXPECT_TRUE(std::all_of(
						        samples,
						        samples + block->frames_count,
						        [](float sample) {
							        return sample == 0.0f;
						        }));
					} else {
						std::fill_n(samples,
						            block->frames_count,
						            static_cast<float>(
						                    i * 10 + c + 1));
					}
				}
			}
		}
	}

	static uint32_t CountNotePorts(const clap_plugin_t* p, bool input)
	{
		return input ? Get(p).note_inputs : Get(p).note_outputs;
	}

	static bool GetNotePort(const clap_plugin_t* p, uint32_t index,
	                        bool input, clap_note_port_info_t* info)
	{
		const auto& state = Get(p);
		if (!state.note_query_succeeds || !input || index != 0) {
			return false;
		}
		info->supported_dialects = state.first_note_dialects;
		return true;
	}

	static uint32_t CountAudioPorts(const clap_plugin_t* p, bool input)
	{
		const auto& state = Get(p);
		return static_cast<uint32_t>(input ? state.input_channels.size()
		                                   : state.output_channels.size());
	}

	static bool GetAudioPort(const clap_plugin_t* p, uint32_t index,
	                         bool input, clap_audio_port_info_t* info)
	{
		const auto& state    = Get(p);
		const auto& channels = input ? state.input_channels
		                             : state.output_channels;
		const auto failed    = input ? state.failed_input_port
		                             : state.failed_output_port;
		if (index >= channels.size() || static_cast<int>(index) == failed) {
			return false;
		}
		info->channel_count = channels[index];
		info->port_type = !input && index == 0 ? state.first_output_type
		                                       : nullptr;
		return true;
	}

	static clap_process_status ProcessBlock(const clap_plugin_t* p,
	                                        const clap_process_t* block)
	{
		Get(p).Process(block);
		return CLAP_PROCESS_CONTINUE;
	}

	static const void* GetExtension(const clap_plugin_t* p, const char* id)
	{
		const auto& state = Get(p);
		if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0 && state.has_note_ports) {
			return &state.note_ports;
		}
		if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0 && state.has_audio_ports) {
			return &state.audio_ports;
		}
		return nullptr;
	}

	static bool Success(const clap_plugin_t*)
	{
		return true;
	}

	static bool Activate(const clap_plugin_t*, double, uint32_t, uint32_t)
	{
		return true;
	}

	clap_plugin_note_ports_t note_ports   = {CountNotePorts, GetNotePort};
	clap_plugin_audio_ports_t audio_ports = {CountAudioPorts, GetAudioPort};
	clap_plugin_t plugin                  = {.desc             = nullptr,
	                                         .plugin_data      = this,
	                                         .init             = Success,
	                                         .destroy          = Noop,
	                                         .activate         = Activate,
	                                         .deactivate       = Noop,
	                                         .start_processing = Success,
	                                         .stop_processing  = Noop,
	                                         .reset            = Noop,
	                                         .process          = ProcessBlock,
	                                         .get_extension    = GetExtension,
	                                         .on_main_thread   = Noop};
};

TEST(ClapPorts, AcceptsAdditionalPorts)
{
	TestPlugin state      = {};
	state.note_inputs     = 4;
	state.note_outputs    = 3;
	state.input_channels  = {1, 2, 6};
	state.output_channels = {2, 1, 8};
	EXPECT_TRUE(Clap::validate_note_ports(&state.plugin));
	EXPECT_TRUE(state.ValidateAudioPorts());
}

TEST(ClapPorts, RequiresMidiOnFirstNoteInput)
{
	TestPlugin state  = {};
	state.note_inputs = 0;
	EXPECT_FALSE(Clap::validate_note_ports(&state.plugin));
	state.note_inputs         = 2;
	state.first_note_dialects = CLAP_NOTE_DIALECT_CLAP;
	EXPECT_FALSE(Clap::validate_note_ports(&state.plugin));
	state.first_note_dialects |= CLAP_NOTE_DIALECT_MIDI;
	EXPECT_TRUE(Clap::validate_note_ports(&state.plugin));
	state.note_query_succeeds = false;
	EXPECT_FALSE(Clap::validate_note_ports(&state.plugin));
	state.has_note_ports = false;
	EXPECT_FALSE(Clap::validate_note_ports(&state.plugin));
}

TEST(ClapPorts, RequiresFirstAudioOutputToBeStereo)
{
	const std::pair<std::vector<uint32_t>, const char*> invalid_outputs[] = {
	        {    {}, CLAP_PORT_STEREO},
	        {{1, 2}, CLAP_PORT_STEREO},
	        {   {2},   CLAP_PORT_MONO},
	        {   {2},          nullptr},
	        {   {2},               ""}
        };
	for (const auto& output : invalid_outputs) {
		SCOPED_TRACE(::testing::PrintToString(output));
		const auto& [channels, type] = output;
		TestPlugin state             = {};
		state.output_channels        = channels;
		state.first_output_type      = type;
		EXPECT_FALSE(state.ValidateAudioPorts());
	}
}

TEST(ClapPorts, RequiresAudioPortsExtension)
{
	TestPlugin state      = {};
	state.has_audio_ports = false;
	EXPECT_FALSE(state.ValidateAudioPorts());
}

TEST(ClapPorts, RejectsInvalidAdditionalAudioPorts)
{
	TestPlugin state        = {};
	state.input_channels    = {1, 2};
	state.output_channels   = {2, 6};
	state.failed_input_port = 1;
	EXPECT_FALSE(state.ValidateAudioPorts());
	state.failed_input_port  = -1;
	state.failed_output_port = 1;
	EXPECT_FALSE(state.ValidateAudioPorts());
	state.failed_output_port = 0;
	EXPECT_FALSE(state.ValidateAudioPorts());
	state.failed_output_port = -1;
	state.input_channels[1]  = 0;
	EXPECT_FALSE(state.ValidateAudioPorts());
	state.input_channels[1]  = 2;
	state.output_channels[1] = 0;
	EXPECT_FALSE(state.ValidateAudioPorts());
}

TEST(ClapProcessing, SuppliesBuffersForAllPortLayouts)
{
	const std::pair<std::vector<uint32_t>, std::vector<uint32_t>> layouts[] = {
	        {           {},       {2}},
                {{1, 2, 6, 65}, {2, 1, 8}}
        };
	for (const auto& layout : layouts) {
		SCOPED_TRACE(::testing::PrintToString(layout));
		const auto& [inputs, outputs] = layout;
		TestPlugin state              = {};
		state.input_channels          = inputs;
		state.output_channels         = outputs;
		Clap::Plugin plugin(nullptr, &state.plugin, inputs, outputs);
		plugin.Activate(32000);
		Clap::EventList events   = {};
		constexpr auto MaxFrames = 8192;
		std::vector<float> left(MaxFrames), right(MaxFrames);
		float* stereo[] = {left.data(), right.data()};
		plugin.Process(stereo, MaxFrames, events);
		ASSERT_FALSE(::testing::Test::HasFatalFailure());
		ASSERT_NE(state.last_process, nullptr);
		EXPECT_EQ(state.last_process->audio_outputs[0].data32, stereo);
		for (uint32_t i = 0; i < state.last_process->audio_outputs_count;
		     ++i) {
			const auto& port = state.last_process->audio_outputs[i];
			for (uint32_t c = 0; c < port.channel_count; ++c) {
				const auto expected = static_cast<float>(i * 10 +
				                                         c + 1);
				EXPECT_EQ(port.data32[c][0], expected);
				EXPECT_EQ(port.data32[c][MaxFrames - 1], expected);
			}
		}

		float next_left = 0.0f, next_right = 0.0f;
		float* next_stereo[] = {&next_left, &next_right};
		plugin.Process(next_stereo, 1, events);
		ASSERT_FALSE(::testing::Test::HasFatalFailure());
		EXPECT_EQ(state.last_process->audio_outputs[0].data32, next_stereo);
		EXPECT_EQ(next_left, 1.0f);
		EXPECT_EQ(next_right, 2.0f);
	}
}

TEST(ClapSynth, DeliversMidiOnceAndRendersStereoInBoundedBlocks)
{
	TestPlugin state = {};
	ClapSynth synth  = {};
	synth.Initialize(std::make_unique<Clap::Plugin>(nullptr,
	                                                &state.plugin,
	                                                state.input_channels,
	                                                state.output_channels),
	                 32000);
	const MidiWork midi({0x90, 60, 100}, 0, MessageType::Channel, 0.0);
	const MidiWork sysex({0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7},
	                     0,
	                     MessageType::SysEx,
	                     0.0);
	synth.ProcessWorkItem(midi);
	synth.ProcessWorkItem(sysex);

	constexpr auto NumFrames = Clap::Plugin::MaxFrameCount + 1;
	RWQueue<AudioFrame> fifo(NumFrames + 1);
	synth.RenderAudioFramesToFifo(NumFrames, fifo);
	synth.RenderAudioFramesToFifo(1, fifo);
	ASSERT_FALSE(::testing::Test::HasFatalFailure());
	EXPECT_EQ(state.block_sizes,
	          (std::vector<uint32_t>{Clap::Plugin::MaxFrameCount, 1, 1}));
	EXPECT_EQ(state.event_types,
	          (std::vector<uint16_t>{CLAP_EVENT_MIDI, CLAP_EVENT_MIDI_SYSEX}));

	ASSERT_EQ(fifo.Size(), NumFrames + 1);
	std::vector<AudioFrame> frames = {};
	fifo.BulkDequeue(frames, NumFrames + 1);
	EXPECT_TRUE(std::all_of(frames.begin(), frames.end(), [](const auto& frame) {
		return frame[0] == 1.0f && frame[1] == 2.0f;
	}));
}

TEST(ClapSelection, FindsFirstMatchingPlugin)
{
	// Factory indices can have gaps when descriptors cannot be retrieved.
	const std::vector<Clap::PluginInfo> infos = {
	        {"plugins/First.clap",           0,      "first-0",                  "Piano Lead", "", ""},
	        {"plugins/First.clap",           2,      "first-2",       "Layered Strings v2.00", "", ""},
	        {"plugins/First.clap",           5,      "first-5", "1 / 2147483648 / 4294967296", "", ""},
	        { "other/Second.CLAP",           0,     "second-0",                  "Piano Bass", "", ""},
	        { "other/Second.CLAP",           1,     "second-1",       "Layered Strings v1.00", "", ""},
	        { "other/Second.CLAP", 2147483647u, "second-large",                  "High Index", "", ""}
        };
	struct Search {
		const char* library;
		const char* plugin;
		const char* expected_id;
	};
	const Search searches[] = {
	        {           "",	                       "",      "first-0"},
	        {      "first",	                       "",      "first-0"},
	        {"SECOND.clap",                               "",     "second-0"},
	        {      "eCoNd",	                       "",     "second-0"},
	        {           "",	                  "pIaNo",      "first-0"},
	        {      "first",                        "STRINGS",      "first-2"},
	        { "first.CLAP",                          "v2.00",      "first-2"},
	        {     "second",                        "strings",     "second-1"},
	        {           "",	                   "bass",     "second-0"},
	        {      "first",	                      "0",      "first-0"},
	        {      "first",	                      "2",      "first-2"},
	        {      "first",	                   "0002",      "first-2"},
	        {     "second",	                      "1",     "second-1"},
	        {           "",	                      "1",     "second-1"},
	        {     "second",                     "2147483647", "second-large"},
	        {    "missing",	                       "",             ""},
	        {    "plugins",	                       "",             ""},
	        {      "first",	                   "bass",             ""},
	        {      "first",	                      "1",             ""},
	        {      "first",	                      "3",             ""},
	        {      "first",                     "4294967296",             ""},
	        {      "first",                     "2147483648",             ""},
	        {           "", "999999999999999999999999999999",             ""}
        };
	for (const auto& search : searches) {
		SCOPED_TRACE(std::string(search.library) + " / " + search.plugin);
		const auto found = Clap::find_plugin(infos,
		                                     search.library,
		                                     search.plugin);
		EXPECT_EQ(found ? found->id : "", search.expected_id);
	}
}

TEST(ClapSelection, HandlesEmptyDiscoveryList)
{
	EXPECT_FALSE(Clap::find_plugin({}, "", ""));
}

} // namespace
