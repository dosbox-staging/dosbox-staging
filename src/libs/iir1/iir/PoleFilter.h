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

#ifndef IIR1_POLEFILTER_H
#define IIR1_POLEFILTER_H

#include "Common.h"
#include "MathSupplement.h"
#include "Cascade.h"
#include "State.h"

namespace Iir {

/***
 * Base for filters designed via algorithmic placement of poles and zeros.
 *
 * Typically, the filter is first designed as a half-band low pass or
 * low shelf analog filter (s-plane). Then, using a transformation such
 * as the ones from Constantinides, the poles and zeros of the analog filter
 * are calculated in the z-plane.
 *
 ***/

/**
 * Factored implementations to reduce template instantiations
 **/
class DllExport PoleFilterBase2 : public Cascade
{
public:
  // This gets the poles/zeros directly from the digital
  // prototype. It is used to double check the correctness
  // of the recovery of pole/zeros from biquad coefficients.
  //
  // It can also be used to accelerate the interpolation
  // of pole/zeros for parameter modulation, since a pole
  // filter already has them calculated

  std::vector<PoleZeroPair> getPoleZeros () const
  {
    std::vector<PoleZeroPair> vpz;
    const int pairs = (m_digitalProto.getNumPoles () + 1) / 2;
    for (int i = 0; i < pairs; ++i)
      vpz.push_back (m_digitalProto[i]);
    return vpz;
  }

protected:
  LayoutBase m_digitalProto = {};
};


/**
 * Serves a container to hold the analog prototype
 * and the digital pole/zero layout.
 **/
template <class AnalogPrototype>
class DllExport PoleFilterBase : public PoleFilterBase2
{
protected:
  void setPrototypeStorage (const LayoutBase& analogStorage,
                            const LayoutBase& digitalStorage)
  {
    m_analogProto.setStorage (analogStorage);
    m_digitalProto = digitalStorage;
  }

protected:
  AnalogPrototype m_analogProto = {};
};

//------------------------------------------------------------------------------

/**
 * Storage for pole filters
 **/
template <class BaseClass,
	  class StateType,
          int MaxAnalogPoles,
	  int MaxDigitalPoles = MaxAnalogPoles>
	struct PoleFilter : BaseClass
	, CascadeStages <(MaxDigitalPoles + 1) / 2 , StateType>
{
  PoleFilter ()
  {
    // This glues together the factored base classes
    // with the templatized storage classes.
    BaseClass::setCascadeStorage (this->getCascadeStorage());
    BaseClass::setPrototypeStorage (m_analogStorage, m_digitalStorage);
  }

private:
  Layout <MaxAnalogPoles> m_analogStorage = {};
  Layout <MaxDigitalPoles> m_digitalStorage = {};
};

//------------------------------------------------------------------------------

/**
 * s-plane to z-plane transforms
 *
 * For pole filters, an analog prototype is created via placement of
 * poles and zeros in the s-plane. The analog prototype is either
 * a halfband low pass or a halfband low shelf. The poles, zeros,
 * and normalization parameters are transformed into the z-plane
 * using variants of the bilinear transformation.
 *
 **/

/** 
 * low pass to low pass 
 **/
class DllExport LowPassTransform
{
public:
  LowPassTransform (double fc,
                    LayoutBase& digital,
                    LayoutBase const& analog);

private:
  complex_t transform (complex_t c);

  double f = 0.0;
};

//------------------------------------------------------------------------------

/**
 * low pass to high pass
 **/
class DllExport HighPassTransform
{
public:
  HighPassTransform (double fc,
                     LayoutBase& digital,
                     LayoutBase const& analog);

private:
  complex_t transform (complex_t c);

  double f = 0.0;
};

//------------------------------------------------------------------------------

/**
 * low pass to band pass transform
 **/
class DllExport BandPassTransform
{

public:
  BandPassTransform (double fc,
                     double fw,
                     LayoutBase& digital,
                     LayoutBase const& analog);

private:
  ComplexPair transform (complex_t c);

  double wc = 0.0;
  double wc2 = 0.0;
  double a = 0.0;
  double b = 0.0;
  double a2 = 0.0;
  double b2 = 0.0;
  double ab = 0.0;
  double ab_2 = 0.0;
};

//------------------------------------------------------------------------------

/** 
 * low pass to band stop transform
 **/
class DllExport BandStopTransform
{
public:
  BandStopTransform (double fc,
                     double fw,
                     LayoutBase& digital,
                     LayoutBase const& analog);

private:
  ComplexPair transform (complex_t c);

  double wc = 0.0;
  double wc2 = 0.0;
  double a = 0.0;
  double b = 0.0;
  double a2 = 0.0;
  double b2 = 0.0;
};

}

#endif
