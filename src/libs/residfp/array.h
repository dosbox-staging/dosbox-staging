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

/**
 * Counter.
 */
class counter
{
private:
    unsigned int c = 0;

public:
    counter() : c(1) {}
    void increase() { ++c; }
    unsigned int decrease() { return --c; }
};

/**
 * Reference counted pointer to matrix wrapper, for use with standard containers.
 */
template<typename T>
class matrix
{
private:
    T* data = nullptr;
    counter count = {};
    const unsigned int x = 0;
    const unsigned int y = 0;

public:
    matrix(unsigned int x, unsigned int y) :
        data(new T[x * y]),
        x(x),
        y(y) {}

    matrix(const matrix& p) :
        data(p.data),
        count(p.count),
        x(p.x),
        y(p.y) { count.increase(); }

    ~matrix() { if (count.decrease() == 0) { delete [] data; data = nullptr; } }

    matrix &operator=(const matrix&) = delete; // prevent assignment

    unsigned int length() const { return x * y; }

    T* operator[](unsigned int a) { return &data[a * y]; }

    T const* operator[](unsigned int a) const { return &data[a * y]; }
};

typedef matrix<short> matrix_t;

#endif
