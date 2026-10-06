// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "clap_test_plugin.h"

#include <string_view>

namespace {

ClapTest::LibraryState state = {};

#ifdef CLAP_TEST_INCOMPATIBLE_ENTRY
constexpr clap_version_t EntryVersion = {0, 9, 0};
#else
constexpr auto EntryVersion = CLAP_VERSION;
#endif

constexpr const char* Features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT,
                                    CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                    CLAP_PLUGIN_FEATURE_STEREO,
                                    nullptr};

constexpr clap_plugin_descriptor_t Descriptor = {.clap_version = CLAP_VERSION,
                                                 .id = "org.dosbox-staging.test",
                                                 .name        = "Test synth",
                                                 .vendor      = nullptr,
                                                 .url         = nullptr,
                                                 .manual_url  = nullptr,
                                                 .support_url = nullptr,
                                                 .version     = "1.0",
                                                 .description = "Test plugin library",
                                                 .features = Features};

const clap_plugin_factory_t Factory = {
        .get_plugin_count = [](const clap_plugin_factory_t*) -> uint32_t {
	        return static_cast<uint32_t>(state.descriptors.size());
        },
        .get_plugin_descriptor = [](const clap_plugin_factory_t*,
                                    uint32_t index) -> const clap_plugin_descriptor_t* {
	        ++state.descriptor_calls;

	        if (index >= state.descriptors.size() ||
	            (index == 0 && !state.has_first_descriptor)) {
		        return nullptr;
	        }

	        return &state.descriptors[index];
        },
        .create_plugin = [](const clap_plugin_factory_t*, const clap_host_t*,
                            const char*) -> const clap_plugin_t* {
	        ++state.create_calls;
	        return nullptr;
        }};

} // namespace

extern "C" CLAP_EXPORT ClapTest::LibraryState* CLAP_ABI ResetTestLibrary()
{
	state = {};

	state.descriptors.fill(Descriptor);
	state.descriptors[1].id   = "org.dosbox-staging.test.second";
	state.descriptors[1].name = "Second test synth";

	return &state;
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
        .clap_version = EntryVersion,
        .init =
                [](const char*) {
	                ++state.init_calls;
	                return state.init_succeeds;
                },
        .deinit      = []() { ++state.deinit_calls; },
        .get_factory = [](const char* id) -> const void* {
	        ++state.factory_calls;

	        return state.has_factory && std::string_view(id) == CLAP_PLUGIN_FACTORY_ID
	                     ? &Factory
	                     : nullptr;
        }};
