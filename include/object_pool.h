/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_OBJECT_POOL_H
#define DOSBOX_OBJECT_POOL_H

#include "dosbox.h"

// This class is a generic object re-use pool. Consider using it if your program
// performs tens of thousands (or more) large object heap allocations as part of
// its normal and ongoing runtime loop. Avoiding excessive heap allocations can
// save CPU time and reduce memory fragmentation.
//
// Ongoing runtime loop:
//     auto my_obj_ptr = new HugeType(arg1, arg2);
//     .. do a bit of work ..
//     delete my_obj_ptr;
//
// If you have this pattern, then the ObjectPool is a good candidate:
//
//  Instantiate a long-lived pool for your HugeType:
//     ObjectPool<HugeType> pool = {};
//
//  Ongoing runtime loop:
//     auto my_obj_ptr = pool.Acquire(arg1, arg2);
//     .. do a bit of work ..
//     pool.Release(my_obj_ptr);

#include <cassert>
#include <memory>

// Clang 15 for macOS 12 and 13 is missing C++17's <memory_resource>, however
// Clang 15 for macOS 14 has it. When the build system eventually uses macOS 14
// for all Apple builds then the '#else' portions can be removed. In the
// meantime we simply fallback to 'new' and 'delete' for this deficient compiler.
#ifdef HAVE_MEMORY_RESOURCE
	#include <memory_resource>
#endif

template <typename T>
class ObjectPool {
public:
	ObjectPool() = default;
	~ObjectPool() = default;

	// Acquire an object: allocate and construct it with the given arguments.
	template <typename... Args>
	constexpr T* Acquire(Args&&... args)
	{
#ifdef HAVE_MEMORY_RESOURCE
		auto ptr = ObjectTraits::allocate(allocator, OneItem);
		ObjectTraits::construct(allocator, ptr, std::forward<Args>(args)...);
#else
		auto ptr = new (std::nothrow) T(std::forward<Args>(args)...);
#endif
		assert(ptr);
		return ptr;
	}

	// Release the object: destruct it and deallocate its memory.
	constexpr void Release(T* ptr)
	{
#ifdef HAVE_MEMORY_RESOURCE
		assert(ptr);
		ObjectTraits::destroy(allocator, ptr);
		ObjectTraits::deallocate(allocator, ptr, OneItem);
#else
        delete ptr;
#endif
	}

#ifdef HAVE_MEMORY_RESOURCE
private:
	using ObjectAllocator = std::pmr::polymorphic_allocator<T>;
	using ObjectTraits = std::allocator_traits<ObjectAllocator>;

	constexpr static auto OneItem = 1;

	// We use C++'s off-the-shelf pool resource as the backing memory for the
	// object's allocator. The allocator is used in the above ObjectTraits::
	// calls, so when we allocate() and deallocate() an object the memory comes
	// from the pool (and not the heap).  More details:
	// - https://en.cppreference.com/w/cpp/memory/allocator_traits
	// - https://en.cppreference.com/w/cpp/memory/polymorphic_allocator
	//
	std::pmr::unsynchronized_pool_resource pool = {};
	ObjectAllocator allocator{&pool};

#endif
};

#endif
