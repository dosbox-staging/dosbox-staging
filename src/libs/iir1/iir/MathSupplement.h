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
 * Copyright (c) 2011 by Bernd Porr
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

#ifndef IIR1_MATHSUPPLEMENT_H
#define IIR1_MATHSUPPLEMENT_H

#include "Common.h"

#include<complex>

#ifdef _MSC_VER
 // Under Unix these have already default instantiations but not under Vis Studio
template class DllExport std::complex<double>;
template class DllExport std::complex<float>;
#endif

namespace Iir {

const double doublePi	=3.1415926535897932384626433832795028841971;
const double doublePi_2	=1.5707963267948966192313216916397514420986;
const double doubleLn2  =0.69314718055994530941723212145818;
const double doubleLn10	=2.3025850929940456840179914546844;

typedef std::complex<double> complex_t;
typedef std::pair<complex_t, complex_t> complex_pair_t;

template<typename Real>
inline std::complex<Real> solve_quadratic_1 (Real a, Real b, Real c)
{
  return (-b + sqrt (std::complex<Real> (b*b - 4*a*c, 0))) / (2. * a);
}

template<typename Real>
inline std::complex<Real> solve_quadratic_2 (Real a, Real b, Real c)
{
  return (-b - sqrt (std::complex<Real> (b*b - 4*a*c, 0))) / (2. * a);
}

inline const complex_t infinity()
{
  return complex_t (std::numeric_limits<double>::infinity());
}

inline const complex_t adjust_imag (const complex_t& c)
{
  if (fabs (c.imag()) < 1e-30)
    return complex_t (c.real(), 0);
  else
    return c;
}

template <typename Ty, typename To>
inline std::complex<Ty> addmul (const std::complex<Ty>& c,
                                Ty v,
                                const std::complex<To>& c1)
{
  return std::complex <Ty> (
    c.real() + v * c1.real(), c.imag() + v * c1.imag());
}

template <typename Ty>
inline std::complex<Ty> recip (const std::complex<Ty>& c)
{
  Ty n = 1.0 / std::norm (c);
  
  return std::complex<Ty> (n * c.real(), n * c.imag());
}

template <typename Ty>
inline Ty asinh (Ty x)
{
  return log (x + std::sqrt (x * x + 1 ));
}

template <typename Ty>
inline Ty acosh (Ty x)
{
  return log (x + std::sqrt (x * x - 1));
}

template <typename Ty>
inline bool is_nan (Ty v)
{
  return !(v == v);
}

template <>
inline bool is_nan<complex_t> (complex_t v)
{
  return Iir::is_nan (v.real()) || Iir::is_nan (v.imag());
}

}

#endif
