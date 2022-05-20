/**
 *
 * "A Collection of Useful C++ Classes for Digital Signal Processing"
 * By Vinnie Falco and Bernd Porr
 *
 * Official project location:
 * https://github.com/berndporr/iir1
 *
 * See Documentation.cpp for contact information, notes, and bibliography.
 * 
 * -----------------------------------------------------------------
 *
 * License: MIT License (http://www.opensource.org/licenses/mit-license.php)
 * Copyright (c) 2009 by Vinnie Falco
 * Copyright (c) 2011-2021 by Bernd Porr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef IIR1_COMMON_H
#define IIR1_COMMON_H

//
// This must be the first file included in every DspFilters header and source
//

#ifdef _MSC_VER
#  pragma warning (disable: 4100)
#endif

// This exports the classes/structures to the windows DLL
#ifdef _WIN32
#  define DllExport   __declspec( dllexport )
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#else
#  define DllExport
#endif

#include <stdlib.h>

#include <cassert>
#include <cfloat>
#include <cmath>
#include <complex>
#include <cstring>
#include <string>
#include <limits>
#include <vector>
#include <stdexcept> // for invalid_argument

static const char orderTooHigh[] = "Requested order is too high. Provide a higher order for the template.";

#define DEFAULT_FILTER_ORDER 4

/**
 * @brief Throw invalid argument exception if exceptions are enabled, otherwise abort.
 *
 * @param msg Error message
 */
inline void throw_invalid_argument(const char* msg) {

#ifndef IIR1_NO_EXCEPTIONS
    throw std::invalid_argument(msg);
#else
    (void) msg; // Discard parameter
    abort();
#endif

}

#endif
