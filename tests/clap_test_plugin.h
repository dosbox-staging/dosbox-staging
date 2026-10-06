// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CLAP_TEST_PLUGIN_H
#define DOSBOX_CLAP_TEST_PLUGIN_H

#include <array>

#include "clap/all.h"

namespace ClapTest {

struct LibraryState {
	std::array<clap_plugin_descriptor_t, 2> descriptors = {};

	bool init_succeeds        = true;
	bool has_factory          = true;
	bool has_first_descriptor = true;

	int init_calls       = 0;
	int deinit_calls     = 0;
	int factory_calls    = 0;
	int descriptor_calls = 0;
	int create_calls     = 0;
};

using ResetFunction = LibraryState*(CLAP_ABI*)();

} // namespace ClapTest

#endif // DOSBOX_CLAP_TEST_PLUGIN_H
