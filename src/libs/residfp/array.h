/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 *  Copyright (C) 2011-2014 Leandro Nini
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ARRAY_H
#define ARRAY_H

#include <cassert>
#include <memory>
#include <vector>

/*
 * Matrix wrapper using a shared pointer to data, for use with standard containers.
 */
template<typename T>
class matrix
{
private:
    using data_t = std::vector<T>;
    using data_ptr_t = std::shared_ptr<data_t>;

    const data_ptr_t data_ptr = {};
    const unsigned int x = 0;
    const unsigned int y = 0;

    void check_dimensions() {
       // debug-only check
       assert(data_ptr);
       assert(x * y == data_ptr->size());
    }

public:
    matrix(unsigned int x, unsigned int y) :
        data_ptr(std::make_shared<data_t>(x * y, 0)),
        x(x),
        y(y) { check_dimensions(); }

    // copy-constructor
    matrix(const matrix& p) :
        data_ptr(p.data_ptr), // data is reference-counted
        x(p.x),
        y(p.y) { check_dimensions(); }

    matrix() = delete; // prevent default-construction
    matrix &operator=(const matrix&) = delete; // prevent assignment

    unsigned int length() const { return static_cast<unsigned int>(data_ptr->size()); }

    T* operator[](unsigned int a) {
        const auto index = a * y;
        assert(index < length());
        return data_ptr->data() + index;
    }

    // Reuse the [] operate for const access
    T const* operator[](unsigned int a) const { return this[a * y]; }
};

typedef matrix<short> matrix_t;

#endif
