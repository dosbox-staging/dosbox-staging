/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

// A custom test main that allows us to initialize the tests with any common
// global state that we need.

#include "../src/libs/loguru/loguru.hpp"
#include <gmock/gmock.h>

int main(int argc, char **argv)
{
    testing::InitGoogleMock(&argc, argv);

    // Turn off logging to stderr, to avoid interfering with the test output
    loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
    // Init loguru to still allow logging to be enabled by command line.
    loguru::init(argc, argv);

    return RUN_ALL_TESTS();
}