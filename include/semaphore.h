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

#include <condition_variable>
#include <mutex>

/*
 * A Semaphore class for synchronizing threads.
 *
 * This Semaphore implementation uses a count to represent the number of 
 * available resources, along with a mutex and a condition variable to 
 * handle synchronization between threads.
 */
class Semaphore {
public:
    /**
     * Constructs a new Semaphore.
     *
     * @param count The initial count. Defaults to 0.
     */
    Semaphore(int count = 0) : count(count) {}

    /**
     * Decrements (acquires) the semaphore. If the count is 0, this will block
     * until another thread calls notify().
     */
    void wait() {
        std::unique_lock lock(mtx);
        while (count == 0) {
            cv.wait(lock);
        }
        --count;
    }

    /**
     * Increments (releases) the semaphore, potentially unblocking a thread
     * currently waiting on wait().
     */
    void notify() {
        std::unique_lock lock(mtx);
        ++count;
        cv.notify_one();
    }

private:
    std::mutex mtx             = {};    // Mutex to protect count.
    std::condition_variable cv = {};    // Condition variable for the count.
    int count                  = 0;     // Current count.
};
