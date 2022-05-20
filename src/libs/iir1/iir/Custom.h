/**
 *
 * "A Collection of Useful C++ Classes for Digital Signal Processing"
 * By Vinnie Falco and Bernd Porr
 *
 * Official project location:
 * https://github.com/berndporr/iir1
 *
 * See Documentation.txt for contact information, notes, and bibliography.
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

#ifndef IIR1_CUSTOM_H
#define IIR1_CUSTOM_H

#include "Common.h"
#include "Biquad.h"
#include "Cascade.h"
#include "PoleFilter.h"
#include "State.h"


namespace Iir {

/**
 * Single pole, Biquad and cascade of Biquads with parameters allowing
 * for directly setting the parameters.
 **/

namespace Custom {

/**
 * Setting up a filter with with one real pole, real zero and scale it by the scale factor
 * \param scale Scale the FIR coefficients by this factor
 * \param pole Position of the pole on the real axis
 * \param zero Position of the zero on the real axis
 **/
struct OnePole : public Biquad
{
	void setup (double scale,
		    double pole,
		    double zero);
};

/**
 * Set a pole/zero pair in polar coordinates and scale the FIR filter coefficients
 * \param poleRho Radius of the pole
 * \param poleTheta Angle of the pole
 * \param zeroRho Radius of the zero
 * \param zeroTheta Angle of the zero
 **/
struct TwoPole : public Biquad
{
	void setup (double scale,
		    double poleRho,
		    double poleTheta,
		    double zeroRho,
		    double zeroTheta);
};

/**
 * A custom cascade of 2nd order (SOS / biquads) filters.
 * \param NSOS The number of 2nd order filters / biquads.
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int NSOS, class StateType = DEFAULT_STATE>
struct DllExport SOSCascade : CascadeStages<NSOS,StateType>
{
	/**
	 * Default constructor which creates a unity gain filter of NSOS biquads.
	 * Set the filter coefficients later with the setup() method.
	 **/
	SOSCascade() = default;
	/**
         * Python scipy.signal-friendly setting of coefficients.
	 * Initialises the coefficients of the whole chain of
	 * biquads / SOS. The argument is a 2D array where the 1st
         * dimension holds an array of 2nd order biquad / SOS coefficients.
         * The six SOS coefficients are ordered "Python" style with first
         * the FIR coefficients (B) and then the IIR coefficients (A).
         * The 2D const double array needs to have exactly the size [NSOS][6].
	 * \param sosCoefficients 2D array Python style sos[NSOS][6]. Indexing: 0-2: FIR-, 3-5: IIR-coefficients.
	 **/
	SOSCascade(const double (&sosCoefficients)[NSOS][6]) {
		CascadeStages<NSOS,StateType>::setup(sosCoefficients);
	}
	/**
         * Python scipy.signal-friendly setting of coefficients.
	 * Sets the coefficients of the whole chain of
	 * biquads / SOS. The argument is a 2D array where the 1st
         * dimension holds an array of 2nd order biquad / SOS coefficients.
         * The six SOS coefficients are ordered "Python" style with first
         * the FIR coefficients (B) and then the IIR coefficients (A).
         * The 2D const double array needs to have exactly the size [NSOS][6].
	 * \param sosCoefficients 2D array Python style sos[NSOS][6]. Indexing: 0-2: FIR-, 3-5: IIR-coefficients.
	 **/
	void setup (const double (&sosCoefficients)[NSOS][6]) {
		CascadeStages<NSOS,StateType>::setup(sosCoefficients);
	}
};

}

}

#endif
