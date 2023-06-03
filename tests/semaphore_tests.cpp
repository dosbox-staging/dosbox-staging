/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

#include <gtest/gtest.h>
#include <thread>
#include "semaphore.h"  // assuming the semaphore class is defined in this header

class SemaphoreTest : public ::testing::Test {
protected:
    Semaphore semaphore{0};
    bool done = false;
};

TEST_F(SemaphoreTest, TestNotify) {
    std::thread worker([&]() {
        semaphore.wait();
        // At this point, the semaphore should have been notified by the main thread.
        done = true;
    });

    // The worker thread should be waiting on the semaphore at this point.
    semaphore.notify();

    worker.join();

    EXPECT_TRUE(done);
}
