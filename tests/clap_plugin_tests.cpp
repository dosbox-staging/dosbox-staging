// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Keep the implementation's narrowing checks local to this include.
#ifdef __GNUC__
#pragma GCC diagnostic push
#endif
#include "audio/clap/plugin_manager.cpp"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "clap_test_plugin.h"
#include "midi/clap.cpp"
#include "midi/private/clap_synth.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#ifndef CLAP_TEST_PLUGIN_PATH
#define CLAP_TEST_PLUGIN_PATH ""
#endif

#ifndef CLAP_TEST_INCOMPATIBLE_PLUGIN_PATH
#define CLAP_TEST_INCOMPATIBLE_PLUGIN_PATH ""
#endif

static_assert(!std::string_view{CLAP_TEST_PLUGIN_PATH}.empty(),
              "CLAP_TEST_PLUGIN_PATH must specify a non-empty path");
static_assert(!std::string_view{CLAP_TEST_INCOMPATIBLE_PLUGIN_PATH}.empty(),
              "CLAP_TEST_INCOMPATIBLE_PLUGIN_PATH must specify a non-empty path");

namespace {

class ClapLibrary : public ::testing::Test {
protected:
	void SetUp() override
	{
		OpenFixture(CLAP_TEST_PLUGIN_PATH);
		ASSERT_NE(state, nullptr);
	}

	void OpenFixture(const char* path)
	{
		handle.reset();
		state        = nullptr;
		library_path = path;

		// Keep the module loaded so counters remain available after the
		// production Library wrapper closes its own handle.
		handle.reset(dynlib_open(library_path));
		ASSERT_NE(handle, nullptr);

		const auto reset = reinterpret_cast<ClapTest::ResetFunction>(
		        dynlib_get_symbol(handle.get(), "ResetTestLibrary"));
		ASSERT_NE(reset, nullptr);

		state = reset();
	}

	std_fs::path library_path     = {};
	ClapTest::LibraryState* state = nullptr;
	std::unique_ptr<std::remove_pointer_t<dynlib_handle>, decltype(&dynlib_close)>
	        handle = {nullptr, dynlib_close};
};

TEST_F(ClapLibrary, InitialisesEnumeratesAndDeinitialises)
{
	{
		Clap::Library library(library_path);
		const auto infos = library.GetPluginInfos();

		ASSERT_EQ(infos.size(), 2);
		EXPECT_EQ(infos[0].library_path, library_path);
		EXPECT_EQ(infos[0].index, 0);
		EXPECT_EQ(infos[0].id, "org.dosbox-staging.test");
		EXPECT_EQ(infos[0].name, "Test synth");
		EXPECT_EQ(infos[0].description, "Test plugin library");
		EXPECT_EQ(infos[0].version, "1.0");
		EXPECT_EQ(infos[1].index, 1);
		EXPECT_EQ(infos[1].id, "org.dosbox-staging.test.second");
		EXPECT_EQ(state->init_calls, 1);
		EXPECT_EQ(state->deinit_calls, 0);
	}

	EXPECT_EQ(state->deinit_calls, 1);
}

TEST_F(ClapLibrary, DoesNotCallClapAfterFailedEntryInitialisation)
{
	state->init_succeeds = false;

	EXPECT_THROW(Clap::Library{library_path}, std::runtime_error);

	EXPECT_EQ(state->init_calls, 1);
	EXPECT_EQ(state->deinit_calls, 0);
	EXPECT_EQ(state->factory_calls, 0);
	EXPECT_EQ(state->descriptor_calls, 0);
	EXPECT_EQ(state->create_calls, 0);

	state->init_succeeds = true;

	{
		Clap::Library library(library_path);

		EXPECT_EQ(library.GetPluginInfos().size(), 2);
	}

	EXPECT_EQ(state->init_calls, 2);
	EXPECT_EQ(state->deinit_calls, 1);
}

TEST_F(ClapLibrary, RejectsIncompatibleEntryBeforeInitialisation)
{
	OpenFixture(CLAP_TEST_INCOMPATIBLE_PLUGIN_PATH);
	ASSERT_NE(state, nullptr);

	EXPECT_THROW(Clap::Library{library_path}, std::runtime_error);

	EXPECT_EQ(state->init_calls, 0);
	EXPECT_EQ(state->deinit_calls, 0);
	EXPECT_EQ(state->factory_calls, 0);
}

TEST_F(ClapLibrary, HandlesMissingFactoryDuringDiscovery)
{
	state->has_factory = false;

	{
		Clap::Library library(library_path);

		EXPECT_TRUE(library.GetPluginInfos().empty());
	}

	EXPECT_EQ(state->init_calls, 1);
	EXPECT_EQ(state->deinit_calls, 1);
	EXPECT_EQ(state->factory_calls, 1);
	EXPECT_EQ(state->descriptor_calls, 0);
}

TEST_F(ClapLibrary, HandlesMissingFactoryDuringLoading)
{
	state->has_factory    = false;
	Clap::PluginInfo info = {};
	info.library_path     = library_path;
	info.id               = state->descriptors[0].id;

	EXPECT_EQ(Clap::PluginManager::GetInstance().LoadPlugin(info), nullptr);

	EXPECT_EQ(state->init_calls, 1);
	EXPECT_EQ(state->deinit_calls, 1);
	EXPECT_EQ(state->factory_calls, 1);
	EXPECT_EQ(state->create_calls, 0);
}

TEST_F(ClapLibrary, AcceptsNullOptionalDescriptorFields)
{
	state->descriptors[0].description = nullptr;
	state->descriptors[0].version     = nullptr;

	Clap::Library library(library_path);
	const auto infos = library.GetPluginInfos();

	ASSERT_EQ(infos.size(), 2);
	EXPECT_EQ(infos[0].id, "org.dosbox-staging.test");
	EXPECT_EQ(infos[0].name, "Test synth");
	EXPECT_TRUE(infos[0].description.empty());
	EXPECT_TRUE(infos[0].version.empty());
}

TEST_F(ClapLibrary, SkipsMissingDescriptor)
{
	state->has_first_descriptor = false;

	Clap::Library library(library_path);
	const auto infos = library.GetPluginInfos();

	ASSERT_EQ(infos.size(), 1);
	EXPECT_EQ(infos.front().index, 1);
	EXPECT_EQ(infos.front().id, "org.dosbox-staging.test.second");
	EXPECT_EQ(state->descriptor_calls, 2);
}

TEST_F(ClapLibrary, SkipsIncompatibleDescriptor)
{
	state->descriptors[0].clap_version = {0, 9, 0};

	Clap::Library library(library_path);
	const auto infos = library.GetPluginInfos();

	ASSERT_EQ(infos.size(), 1);
	EXPECT_EQ(infos.front().index, 1);
	EXPECT_EQ(infos.front().id, "org.dosbox-staging.test.second");
	EXPECT_EQ(state->descriptor_calls, 2);
}

TEST_F(ClapLibrary, SkipsMissingRequiredDescriptorFields)
{
	const char* const invalid_values[] = {nullptr, ""};

	for (const auto value : invalid_values) {
		SCOPED_TRACE(value ? "blank" : "null");

		Clap::Library library(library_path);

		state->descriptors[0].id = value;

		const auto infos = library.GetPluginInfos();

		ASSERT_EQ(infos.size(), 1);
		EXPECT_EQ(infos.front().index, 1);

		state->descriptors[0].id   = "org.dosbox-staging.test";
		state->descriptors[0].name = value;

		const auto next_infos = library.GetPluginInfos();

		ASSERT_EQ(next_infos.size(), 1);
		EXPECT_EQ(next_infos.front().index, 1);

		state->descriptors[0].name = "Test synth";
	}
}

TEST_F(ClapLibrary, RequiresAllSynthFeatures)
{
	struct FeatureCase {
		const char* name                  = nullptr;
		std::vector<const char*> features = {};
	};

	// clang-format off
	const FeatureCase cases[] = {
	        {"null", {}},
	        {"empty", {nullptr}},
	        {"missing instrument",
	         {CLAP_PLUGIN_FEATURE_SYNTHESIZER,
	          CLAP_PLUGIN_FEATURE_STEREO,
	          nullptr}},
	        {"missing synthesizer",
	         {CLAP_PLUGIN_FEATURE_INSTRUMENT,
	          CLAP_PLUGIN_FEATURE_STEREO,
	          nullptr}},
	        {"missing stereo",
	         {CLAP_PLUGIN_FEATURE_INSTRUMENT,
	          CLAP_PLUGIN_FEATURE_SYNTHESIZER,
	          nullptr}},
	        {"effect",
	         {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
	          CLAP_PLUGIN_FEATURE_STEREO,
	          nullptr}},
	        {"duplicate instrument",
	         {CLAP_PLUGIN_FEATURE_INSTRUMENT,
	          CLAP_PLUGIN_FEATURE_INSTRUMENT,
	          CLAP_PLUGIN_FEATURE_STEREO,
	          nullptr}},
	        {"partial names",
	         {"instrument-extra", "synthesizer-extra", "stereo-extra", nullptr}},
	        {"wrong case",
	         {"Instrument",
	          CLAP_PLUGIN_FEATURE_SYNTHESIZER,
	          CLAP_PLUGIN_FEATURE_STEREO,
	          nullptr}}
	};
	// clang-format on

	Clap::Library library(library_path);

	for (const auto& feature_case : cases) {
		SCOPED_TRACE(feature_case.name);

		state->descriptors[0].features = feature_case.features.empty()
		                                       ? nullptr
		                                       : feature_case.features.data();

		const auto infos = library.GetPluginInfos();

		ASSERT_EQ(infos.size(), 1);
		EXPECT_EQ(infos.front().index, 1);
		EXPECT_EQ(infos.front().id, "org.dosbox-staging.test.second");
	}
}

TEST_F(ClapLibrary, AcceptsReorderedAndAdditionalSynthFeatures)
{
	const char* const features[] = {"org.dosbox-staging:unknown",
	                                CLAP_PLUGIN_FEATURE_STEREO,
	                                CLAP_PLUGIN_FEATURE_INSTRUMENT,
	                                CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
	                                CLAP_PLUGIN_FEATURE_SYNTHESIZER,
	                                CLAP_PLUGIN_FEATURE_INSTRUMENT,
	                                nullptr};

	state->descriptors[0].features = features;

	Clap::Library library(library_path);
	const auto infos = library.GetPluginInfos();

	ASSERT_EQ(infos.size(), 2);
	EXPECT_EQ(infos[0].index, 0);
	EXPECT_EQ(infos[0].id, "org.dosbox-staging.test");
	EXPECT_EQ(infos[1].index, 1);
}

TEST_F(ClapLibrary, FiltersEffectsFromListingAndSelection)
{
	const char* const features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
	                                CLAP_PLUGIN_FEATURE_STEREO,
	                                nullptr};

	state->descriptors[0].name     = "Test effect";
	state->descriptors[0].features = features;

	Clap::Library library(library_path);
	const auto infos = library.GetPluginInfos();

	ASSERT_EQ(infos.size(), 1);

	const auto library_name = library_path.filename().string();
	const auto listing      = format_clap_plugin_list(infos, nullptr);

	EXPECT_EQ(listing, "  " + library_name + "\n   1 - Second test synth\n");
	EXPECT_FALSE(Clap::find_plugin(infos, library_name, "0"));
	EXPECT_FALSE(Clap::find_plugin(infos, "", "effect"));

	const auto selected = Clap::find_plugin(infos, library_name, "1");

	ASSERT_TRUE(selected);
	EXPECT_EQ(selected->id, "org.dosbox-staging.test.second");
}

struct TestPlugin {
	enum class State {
		Uninitialised,
		ActivationFailed,
		Activated,
		StartFailed,
		Started,
		Processing,
		Stopped,
		Deactivated,
		Destroyed
	};

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
	State activation_result               = State::Activated;
	State start_result                    = State::Started;
	clap_process_status process_status    = CLAP_PROCESS_CONTINUE;
	std::vector<State> lifecycle          = {State::Uninitialised};

	State GetState() const
	{
		return lifecycle.back();
	}

	static TestPlugin& Get(const clap_plugin_t* plugin)
	{
		return *static_cast<TestPlugin*>(plugin->plugin_data);
	}

	static void Noop(const clap_plugin_t*) {}

	bool ValidateAudioPorts()
	{
		std::vector<uint32_t> inputs  = {};
		std::vector<uint32_t> outputs = {};

		if (!Clap::validate_audio_ports(&plugin, inputs, outputs)) {
			return false;
		}

		EXPECT_EQ(inputs, input_channels);
		EXPECT_EQ(outputs, output_channels);

		return true;
	}

	void Process(const clap_process_t* block)
	{
		EXPECT_TRUE(GetState() == State::Started ||
		            GetState() == State::Processing);

		lifecycle.push_back(State::Processing);
		last_process = block;
		block_sizes.push_back(block->frames_count);

		const auto* events = block->in_events;

		for (uint32_t event_index = 0; event_index < events->size(events);
		     ++event_index) {
			const auto* header = events->get(events, event_index);
			ASSERT_NE(header, nullptr);

			event_types.push_back(header->type);
		}

		ASSERT_EQ(block->audio_inputs_count, input_channels.size());
		if (input_channels.empty()) {
			EXPECT_EQ(block->audio_inputs, nullptr);
		} else {
			ASSERT_NE(block->audio_inputs, nullptr);
		}

		for (size_t in_port_index = 0;
		     in_port_index < input_channels.size();
		     ++in_port_index) {
			const auto& port = block->audio_inputs[in_port_index];
			ASSERT_EQ(port.channel_count, input_channels[in_port_index]);
			ASSERT_NE(port.data32, nullptr);
			EXPECT_EQ(port.data64, nullptr);

			for (uint32_t channel_index = 0;
			     channel_index < port.channel_count;
			     ++channel_index) {
				const auto* samples = port.data32[channel_index];
				ASSERT_NE(samples, nullptr);

				EXPECT_TRUE(std::all_of(samples,
				                        samples + block->frames_count,
				                        [](float sample) {
					                        return sample == 0.0f;
				                        }));
			}
		}

		ASSERT_EQ(block->audio_outputs_count, output_channels.size());
		ASSERT_NE(block->audio_outputs, nullptr);

		for (size_t out_port_index = 0;
		     out_port_index < output_channels.size();
		     ++out_port_index) {
			const auto& port = block->audio_outputs[out_port_index];
			ASSERT_EQ(port.channel_count,
			          output_channels[out_port_index]);
			ASSERT_NE(port.data32, nullptr);
			EXPECT_EQ(port.data64, nullptr);

			for (uint32_t channel_index = 0;
			     channel_index < port.channel_count;
			     ++channel_index) {
				auto* samples = port.data32[channel_index];
				ASSERT_NE(samples, nullptr);

				std::fill_n(samples,
				            block->frames_count,
				            static_cast<float>(out_port_index * 10 +
				                               channel_index + 1));
			}
		}
	}

	static uint32_t CountNotePorts(const clap_plugin_t* p, bool input)
	{
		return input ? Get(p).note_inputs : Get(p).note_outputs;
	}

	static bool GetNotePort(const clap_plugin_t* p, uint32_t port_index,
	                        bool input, clap_note_port_info_t* info)
	{
		const auto& state = Get(p);
		if (!state.note_query_succeeds || !input || port_index != 0) {
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

	static bool GetAudioPort(const clap_plugin_t* p, uint32_t port_index,
	                         bool input, clap_audio_port_info_t* info)
	{
		const auto& state    = Get(p);
		const auto& channels = input ? state.input_channels
		                             : state.output_channels;
		const auto failed    = input ? state.failed_input_port
		                             : state.failed_output_port;
		if (port_index >= channels.size() ||
		    static_cast<int>(port_index) == failed) {
			return false;
		}

		info->channel_count = channels[port_index];
		info->port_type = !input && port_index == 0 ? state.first_output_type
		                                            : nullptr;
		return true;
	}

	static clap_process_status ProcessBlock(const clap_plugin_t* p,
	                                        const clap_process_t* block)
	{
		Get(p).Process(block);
		return Get(p).process_status;
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

	static bool Activate(const clap_plugin_t* p, double, uint32_t, uint32_t)
	{
		auto& state = Get(p);
		EXPECT_TRUE(state.GetState() == State::Uninitialised ||
		            state.GetState() == State::ActivationFailed ||
		            state.GetState() == State::Deactivated);

		state.lifecycle.push_back(state.activation_result);
		return state.GetState() == State::Activated;
	}

	static bool Start(const clap_plugin_t* p)
	{
		auto& state = Get(p);
		EXPECT_TRUE(state.GetState() == State::Activated ||
		            state.GetState() == State::StartFailed ||
		            state.GetState() == State::Stopped);

		state.lifecycle.push_back(state.start_result);
		return state.GetState() == State::Started;
	}

	static void Stop(const clap_plugin_t* p)
	{
		auto& state = Get(p);
		EXPECT_TRUE(state.GetState() == State::Started ||
		            state.GetState() == State::Processing);

		state.lifecycle.push_back(State::Stopped);
	}

	static void Deactivate(const clap_plugin_t* p)
	{
		auto& state = Get(p);
		EXPECT_TRUE(state.GetState() == State::Activated ||
		            state.GetState() == State::StartFailed ||
		            state.GetState() == State::Stopped);

		state.lifecycle.push_back(State::Deactivated);
	}

	static void Destroy(const clap_plugin_t* p)
	{
		auto& state = Get(p);
		EXPECT_TRUE(state.GetState() == State::Uninitialised ||
		            state.GetState() == State::ActivationFailed ||
		            state.GetState() == State::Deactivated);

		state.lifecycle.push_back(State::Destroyed);
	}

	clap_plugin_note_ports_t note_ports   = {CountNotePorts, GetNotePort};
	clap_plugin_audio_ports_t audio_ports = {CountAudioPorts, GetAudioPort};
	clap_plugin_t plugin                  = {.desc             = nullptr,
	                                         .plugin_data      = this,
	                                         .init             = Success,
	                                         .destroy          = Destroy,
	                                         .activate         = Activate,
	                                         .deactivate       = Deactivate,
	                                         .start_processing = Start,
	                                         .stop_processing  = Stop,
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
		std::vector<float> left(MaxFrames);
		std::vector<float> right(MaxFrames);
		float* stereo[] = {left.data(), right.data()};

		plugin.Process(stereo, MaxFrames, events);

		ASSERT_FALSE(::testing::Test::HasFatalFailure());
		ASSERT_NE(state.last_process, nullptr);
		EXPECT_EQ(state.last_process->audio_outputs[0].data32, stereo);

		for (uint32_t out_port_index = 0;
		     out_port_index < state.last_process->audio_outputs_count;
		     ++out_port_index) {
			const auto& port = state.last_process->audio_outputs[out_port_index];

			for (uint32_t channel_index = 0;
			     channel_index < port.channel_count;
			     ++channel_index) {
				const auto expected = static_cast<float>(
				        out_port_index * 10 + channel_index + 1);

				EXPECT_EQ(port.data32[channel_index][0], expected);
				EXPECT_EQ(port.data32[channel_index][MaxFrames - 1],
				          expected);
			}
		}

		float next_left      = 0.0f;
		float next_right     = 0.0f;
		float* next_stereo[] = {&next_left, &next_right};

		plugin.Process(next_stereo, 1, events);

		ASSERT_FALSE(::testing::Test::HasFatalFailure());
		EXPECT_EQ(state.last_process->audio_outputs[0].data32, next_stereo);
		EXPECT_EQ(next_left, 1.0f);
		EXPECT_EQ(next_right, 2.0f);

		plugin.StopProcessing();
	}
}

TEST(ClapProcessing, SilencesFailedBlocksAndRecovers)
{
	TestPlugin state = {};
	Clap::Plugin plugin(nullptr, &state.plugin, {}, {2});
	plugin.Activate(32000);

	std::array<float, 4> left  = {};
	std::array<float, 4> right = {};
	float* stereo[]            = {left.data(), right.data()};
	Clap::EventList events     = {};

	plugin.Process(stereo, 4, events);

	EXPECT_EQ(left, (std::array<float, 4>{1, 1, 1, 1}));
	EXPECT_EQ(right, (std::array<float, 4>{2, 2, 2, 2}));

	state.process_status = CLAP_PROCESS_ERROR;
	plugin.Process(stereo, 3, events);

	EXPECT_EQ(left, (std::array<float, 4>{0, 0, 0, 1}));
	EXPECT_EQ(right, (std::array<float, 4>{0, 0, 0, 2}));

	plugin.Process(stereo, 4, events);

	EXPECT_EQ(left, (std::array<float, 4>{0, 0, 0, 0}));
	EXPECT_EQ(right, (std::array<float, 4>{0, 0, 0, 0}));

	state.process_status = CLAP_PROCESS_CONTINUE;
	plugin.Process(stereo, 4, events);

	EXPECT_EQ(left, (std::array<float, 4>{1, 1, 1, 1}));
	EXPECT_EQ(right, (std::array<float, 4>{2, 2, 2, 2}));

	state.process_status = CLAP_PROCESS_ERROR;
	plugin.Process(stereo, 4, events);

	EXPECT_EQ(left, (std::array<float, 4>{0, 0, 0, 0}));
	EXPECT_EQ(right, (std::array<float, 4>{0, 0, 0, 0}));

	plugin.StopProcessing();
}

TEST(ClapSynth, DoesNotReplayMidiAfterFailedBlock)
{
	TestPlugin state = {};
	ClapSynth synth  = {};

	synth.Initialize(std::make_unique<Clap::Plugin>(nullptr,
	                                                &state.plugin,
	                                                state.input_channels,
	                                                state.output_channels),
	                 32000);

	synth.ProcessWorkItem(MidiWork({0x90, 60, 100}, 0, MessageType::Channel, 0.0));

	RWQueue<AudioFrame> fifo(4);

	state.process_status = CLAP_PROCESS_ERROR;
	synth.RenderAudioFramesToFifo(2, fifo);

	state.process_status = CLAP_PROCESS_CONTINUE;
	synth.RenderAudioFramesToFifo(1, fifo);

	synth.StopProcessing();

	ASSERT_FALSE(::testing::Test::HasFatalFailure());

	EXPECT_EQ(state.event_types, (std::vector<uint16_t>{CLAP_EVENT_MIDI}));
	ASSERT_EQ(fifo.Size(), 3);

	std::vector<AudioFrame> frames = {};
	fifo.BulkDequeue(frames, 3);

	EXPECT_EQ(frames[0], (AudioFrame{0.0f, 0.0f}));
	EXPECT_EQ(frames[1], (AudioFrame{0.0f, 0.0f}));
	EXPECT_EQ(frames[2], (AudioFrame{1.0f, 2.0f}));
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
	synth.StopProcessing();

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

TEST(ClapLifecycle, FollowsActivationAndProcessingOrder)
{
	using enum TestPlugin::State;

	TestPlugin state = {};

	{
		Clap::Plugin plugin(nullptr, &state.plugin, {}, {2});
		plugin.Activate(32000);

		float left             = 0.0f;
		float right            = 0.0f;
		float* stereo[]        = {&left, &right};
		Clap::EventList events = {};

		plugin.Process(stereo, 1, events);
		plugin.Process(stereo, 1, events);
		plugin.StopProcessing();
	}

	EXPECT_EQ(state.lifecycle,
	          (std::vector<TestPlugin::State>{Uninitialised,
	                                          Activated,
	                                          Started,
	                                          Processing,
	                                          Processing,
	                                          Stopped,
	                                          Deactivated,
	                                          Destroyed}));
}

TEST(ClapLifecycle, RejectsFailedActivation)
{
	using enum TestPlugin::State;

	TestPlugin state        = {};
	state.activation_result = ActivationFailed;

	{
		Clap::Plugin plugin(nullptr, &state.plugin, {}, {2});
		EXPECT_THROW(plugin.Activate(32000), std::runtime_error);
	}

	EXPECT_EQ(state.lifecycle,
	          (std::vector<TestPlugin::State>{Uninitialised,
	                                          ActivationFailed,
	                                          Destroyed}));
}

TEST(ClapLifecycle, RendersSilenceWhenProcessingCannotStart)
{
	using enum TestPlugin::State;

	TestPlugin state   = {};
	state.start_result = StartFailed;

	{
		Clap::Plugin plugin(nullptr, &state.plugin, {}, {2});
		plugin.Activate(32000);

		float left             = 1.0f;
		float right            = 1.0f;
		float* stereo[]        = {&left, &right};
		Clap::EventList events = {};

		plugin.Process(stereo, 1, events);
		plugin.Process(stereo, 1, events);

		EXPECT_EQ(left, 0.0f);
		EXPECT_EQ(right, 0.0f);
	}

	EXPECT_EQ(state.lifecycle,
	          (std::vector<TestPlugin::State>{
	                  Uninitialised, Activated, StartFailed, Deactivated, Destroyed}));
}

TEST(ClapConfiguration, ParsesQuotedEnvironmentAssignments)
{
	const auto actual = parse_clap_environment(
	        R"(MODE=gm "SOUNDCANVAS_ROM_PATH=C:\Software\My ROMs" EMPTY= TOKEN=a=b MODE=gs)");
	const std::unordered_map<std::string, std::string> expected = {
	        {                "MODE",                     "gs"},
	        {"SOUNDCANVAS_ROM_PATH", R"(C:\Software\My ROMs)"},
	        {               "EMPTY",                       ""},
	        {               "TOKEN",                    "a=b"}
        };

	EXPECT_EQ(actual, expected);
	EXPECT_TRUE(parse_clap_environment(" \t").empty());

	for (const auto* invalid :
	     {"MISSING", "=value", "\"BAD NAME=value\"", "\"UNCLOSED=value"}) {
		SCOPED_TRACE(invalid);

		EXPECT_THROW(parse_clap_environment(invalid), std::runtime_error);
	}
}

TEST(ClapConfiguration, ProvidesSelectorsAndSampleRate)
{
	SectionProp section("clap");
	init_clap_config_settings(section);

	EXPECT_EQ(section.GetString("clap_library"), "");
	EXPECT_EQ(section.GetString("clap_plugin"), "");
	EXPECT_EQ(section.GetString("clap_env"), "");
	EXPECT_EQ(section.GetString("clap_sample_rate"), "auto");

	EXPECT_TRUE(section.GetProperty("clap_plugin")->SetValue("2"));
	EXPECT_EQ(section.GetString("clap_plugin"), "2");

	EXPECT_TRUE(section.GetProperty("clap_sample_rate")->SetValue("33103"));
	EXPECT_EQ(section.GetString("clap_sample_rate"), "33103");
}

TEST(ClapConfiguration, RequiresPluginSelectorBeforeReadingEnvironment)
{
	CommandLine command_line(0, nullptr);

	struct RestoreControl {
		ConfigPtr previous_control = std::move(control);

		~RestoreControl()
		{
			control = std::move(previous_control);
		}
	} restore_control = {};

	control = std::make_unique<Config>(&command_line);
	CLAP_AddConfigSection(control);

	const auto section = get_section("clap");

	for (const auto* environment : {"", "MISSING"}) {
		SCOPED_TRACE(environment);

		ASSERT_TRUE(section->GetProperty("clap_env")->SetValue(environment));

		try {
			MidiDeviceClap device;
			FAIL() << "Expected missing plugin selectors to be rejected";
		} catch (const std::runtime_error& error) {
			const auto message = std::string_view(error.what());

			EXPECT_NE(message.find("At least one"),
			          std::string_view::npos);
			EXPECT_NE(message.find("clap_library"),
			          std::string_view::npos);
			EXPECT_NE(message.find("clap_plugin"),
			          std::string_view::npos);
		}
	}
}

TEST(ClapConfiguration, PreservesEnvironmentQuotesInConfigFiles)
{
	CommandLine command_line(0, nullptr);

	struct RestoreControl {
		ConfigPtr previous_control = std::move(control);

		~RestoreControl()
		{
			control = std::move(previous_control);
		}
	} restore_control = {};

	control = std::make_unique<Config>(&command_line);

	CLAP_AddConfigSection(control);

	const auto section = get_section("clap");
	const std::string settings = R"("FIRST=C:\My ROMs" "SECOND=another value")";

	ASSERT_TRUE(section->HandleInputLine("clap_env = " + settings));
	EXPECT_EQ(section->GetString("clap_env"), settings);

	const std::unordered_map<std::string, std::string> expected = {
	        { "FIRST", R"(C:\My ROMs)"},
                {"SECOND", "another value"}
        };

	EXPECT_EQ(parse_clap_environment(section->GetString("clap_env")), expected);

	ASSERT_TRUE(section->HandleInputLine("clap_library = \"Example\""));
	EXPECT_EQ(section->GetString("clap_library"), "Example");
}

TEST(ClapConfiguration, SetsEnvironmentForNativeAndCrtReaders)
{
	constexpr auto Name  = "DOSBOX_CLAP_ENV_TEST";
	constexpr auto Value = "value with spaces=one";

	struct RestoreEnvironment {
		const char* name                 = nullptr;
		std::optional<std::string> value = {};

		~RestoreEnvironment()
		{
#if !defined(_WIN32)
			if (!value) {
				unsetenv(name);
				return;
			}
#endif
			set_env_var(name,
			            value ? value->c_str() : nullptr,
			            Env::Overwrite);
		}
	};

	const auto previous_value                    = std::getenv(Name);
	const RestoreEnvironment restore_environment = {
	        Name,
	        previous_value ? std::make_optional<std::string>(previous_value)
	                       : std::nullopt};

	set_env_var(Name, Value, Env::Overwrite);

	EXPECT_EQ(get_env_var(Name), Value);
	EXPECT_STREQ(std::getenv(Name), Value);
}

TEST(ClapListing, GroupsLibrariesAndHighlightsTheActiveInstance)
{
	const std::vector<Clap::PluginInfo> infos = {
	        {"plugins/First.clap", 0,   "piano",         "Piano"},
	        {"plugins/First.clap", 2, "strings",       "Strings"},
	        {  "other/First.clap", 0,   "piano", "Another Piano"}
        };
	const std::string first_library = "  First.clap\n   0 - Piano\n   2 - Strings\n";

	EXPECT_EQ(format_clap_plugin_list(infos, nullptr),
	          first_library + "  First.clap\n   0 - Another Piano\n");

	EXPECT_EQ(format_clap_plugin_list(infos, &infos.back()),
	          first_library + "  First.clap\n" +
	                  convert_ansi_markup("[color=light-green]") +
	                  " * 0 - Another Piano\n" + convert_ansi_markup("[reset]"));

	EXPECT_TRUE(format_clap_plugin_list({}, nullptr).empty());
}

TEST(ClapListing, ExcludesDedicatedSynthLibraries)
{
	const std::vector<Clap::PluginInfo> infos = {
	        {     "plugins/Nuked-SC55.clap", 0,    "sc55",    "Roland SC-55 v1.21"},
	        {          "plugins/First.clap", 2,   "piano",                 "Piano"},
	        {"other/Custom-Nuked-SC55.clap", 1, "sc55mk2", "Roland SC-55mk2 v1.01"},
	        {         "plugins/Second.clap", 4, "strings",               "Strings"},
	        {       "other/Nuked-SC55.clap", 1, "sc55mk2", "Roland SC-55mk2 v1.01"}
        };

	const auto generic_infos = Clap::filter_plugin_infos(
	        infos, Clap::PluginFilter::GenericSynthOnly);

	EXPECT_EQ(format_clap_plugin_list(generic_infos, nullptr),
	          "  First.clap\n   2 - Piano\n"
	          "  Custom-Nuked-SC55.clap\n   1 - Roland SC-55mk2 v1.01\n"
	          "  Second.clap\n   4 - Strings\n");

	const auto dedicated_only = Clap::filter_plugin_infos(
	        {infos.front(), infos.back()}, Clap::PluginFilter::GenericSynthOnly);

	EXPECT_TRUE(dedicated_only.empty());
	EXPECT_TRUE(format_clap_plugin_list(dedicated_only, nullptr).empty());
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
	        {           "",	                       "",             ""},
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
	        {           "",	                      "0",             ""},
	        {           "",	                      "1",             ""},
	        {           "",	                   "0002",             ""},
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

TEST(ClapSelection, MatchesExactLibraryWithPartialPluginNames)
{
	constexpr auto ModelName = "Roland SC-55 v1.21";

	const std::vector<Clap::PluginInfo> infos = {
	        {"plugins/xxxNuked-SC55.clap", 0,     "renamed",            ModelName},
	        {   "plugins/Nuked-SC55.clap", 0, "other-model", "Roland SC-55 v1.20"},
	        {   "plugins/Nuked-SC55.clap", 2,       "model",            ModelName},
	        {     "other/Nuked-SC55.clap", 2,       "later",            ModelName}
        };

	struct Search {
		const char* library;
		const char* plugin;
		const char* expected_id;
	};

	const Search searches[] = {
	        {        "Nuked-SC55.clap",            ModelName,       "model"},
	        {        "Nuked-SC55.clap",                   "", "other-model"},
	        {	               "",            ModelName,     "renamed"},
	        {        "Nuked-SC55.clap",                  "0", "other-model"},
	        {        "Nuked-SC55.clap",                  "2",       "model"},
	        {        "Nuked-SC55.clap",               "0002",       "model"},
	        {        "Nuked-SC55.clap",                  "3",            ""},
	        {             "Nuked-SC55",            ModelName,            ""},
	        {        "nuked-sc55.clap",            ModelName,            ""},
	        {"plugins/Nuked-SC55.clap",            ModelName,            ""},
	        {        "Nuked-SC55.clap",       "Roland SC-55", "other-model"},
	        {        "Nuked-SC55.clap", "roland sc-55 v1.21",       "model"},
	        {        "Nuked-SC55.clap", "Roland SC-55 v1.22",            ""},
	        {        "Nuked-SC55.clap",         "2147483648",            ""},
	        {	               "",                  "0",            ""},
	        {	               "",                   "",            ""}
        };

	for (const auto& search : searches) {
		SCOPED_TRACE(std::string(search.library) + " / " + search.plugin);

		const auto found = Clap::find_plugin(infos,
		                                     search.library,
		                                     search.plugin,
		                                     true);

		EXPECT_EQ(found ? found->id : "", search.expected_id);
	}

	// Generic selection still accepts partial matches in discovery order.
	const auto partial_match = Clap::find_plugin(infos, "nuked-sc55", "roland sc-55");

	ASSERT_TRUE(partial_match);
	EXPECT_EQ(partial_match->id, "renamed");
}

TEST(ClapSelection, IgnoresPluginPrefixesWhenMatchingModels)
{
	for (const auto* name : {"Roland SC-55 v1.21",
	                         "Nuked SC-55 — Roland SC-55 v1.21",
	                         "Nuked SC-55 - Roland SC-55 v1.21",
	                         "Different prefix: Roland SC-55 v1.21"}) {
		SCOPED_TRACE(name);

		const std::vector<Clap::PluginInfo> infos = {
		        {"plugins/xxxNuked-SC55.clap", 0,     "renamed",                 name},
		        {   "plugins/Nuked-SC55.clap", 0, "other-model", "Roland SC-55 v1.20"},
		        {   "plugins/Nuked-SC55.clap", 1,       "model",                 name}
                };

		const auto found = Clap::find_plugin(infos,
		                                     "Nuked-SC55.clap",
		                                     "Roland SC-55 v1.21",
		                                     true);

		ASSERT_TRUE(found);
		EXPECT_EQ(found->id, "model");
	}
}

TEST(ClapSelection, KeepsRenamedDedicatedLibrariesInGenericSearches)
{
	for (const auto* path : {"plugins/xxxNuked-SC55.clap",
	                         "plugins/Nuked-SC55.clap.backup.clap",
	                         "plugins/NUKED-SC55.CLAP",
	                         "Nuked-SC55.clap/Other.clap"}) {
		SCOPED_TRACE(path);

		const std::vector<Clap::PluginInfo> infos = {
		        {path, 0, "renamed", "Roland SC-55 v1.21"}
                };

		const auto generic_infos = Clap::filter_plugin_infos(
		        infos, Clap::PluginFilter::GenericSynthOnly);

		ASSERT_EQ(generic_infos.size(), 1);

		const auto generic_match = Clap::find_plugin(generic_infos, "", "SC-55");

		ASSERT_TRUE(generic_match);
		EXPECT_EQ(generic_match->id, "renamed");

		EXPECT_FALSE(Clap::find_plugin(
		        infos, "Nuked-SC55.clap", "Roland SC-55 v1.21", true));
	}
}

TEST(ClapSelection, ExcludesDedicatedSynthsFromGenericSearches)
{
	const std::vector<Clap::PluginInfo> infos = {
	        {"plugins/Nuked-SC55.clap", 0,    "sc55",   "Roland SC-55 v1.21"},
	        {     "plugins/Other.clap", 3, "generic", "SC-55-inspired Synth"}
        };

	const auto generic_infos = Clap::filter_plugin_infos(
	        infos, Clap::PluginFilter::GenericSynthOnly);

	EXPECT_FALSE(Clap::find_plugin(generic_infos, "Nuked-SC55", ""));
	EXPECT_FALSE(Clap::find_plugin(generic_infos, "Nuked-SC55", "0"));
	EXPECT_FALSE(Clap::find_plugin(generic_infos, "", "Roland SC-55"));

	const auto name_match = Clap::find_plugin(generic_infos, "", "SC-55");
	ASSERT_TRUE(name_match);
	EXPECT_EQ(name_match->id, "generic");

	const auto index_match = Clap::find_plugin(generic_infos, "Other", "3");
	ASSERT_TRUE(index_match);
	EXPECT_EQ(index_match->id, "generic");

	// Dedicated synths can still find their plugins in the full cache.
	const auto all_infos = Clap::filter_plugin_infos(infos,
	                                                 Clap::PluginFilter::All);
	ASSERT_EQ(all_infos.size(), infos.size());
	const auto dedicated_match = Clap::find_plugin(all_infos,
	                                               "Nuked-SC55.clap",
	                                               "Roland SC-55 v1.21",
	                                               true);

	ASSERT_TRUE(dedicated_match);
	EXPECT_EQ(dedicated_match->id, "sc55");
}

TEST(ClapSelection, HandlesEmptyDiscoveryList)
{
	EXPECT_FALSE(Clap::find_plugin({}, "", ""));
	EXPECT_FALSE(
	        Clap::find_plugin({}, "Nuked-SC55.clap", "Roland SC-55 v1.21", true));
}

} // namespace
