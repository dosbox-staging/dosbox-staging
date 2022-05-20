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

#ifndef IIR1_BIQUAD_H
#define IIR1_BIQUAD_H

#include "Common.h"
#include "MathSupplement.h"
#include "Types.h"

namespace Iir {

	struct DllExport BiquadPoleState;

/*
 * Holds coefficients for a second order Infinite Impulse Response
 * digital filter. This is the building block for all IIR filters.
 *
 */
	class DllExport Biquad
	{
	public:
		/**
		 * Calculate filter response at the given normalized frequency
		 * and return the complex response.
		 **/
		complex_t response (double normalizedFrequency) const;

		/**
                 * Returns the pole / zero Pairs as a vector.
                 **/
		std::vector<PoleZeroPair> getPoleZeros () const;

		/**
                 * Returns 1st IIR coefficient (usually one)
                 **/
		double getA0 () const { return m_a0; }

		/**
                 * Returns 2nd IIR coefficient
                 **/
		double getA1 () const { return m_a1*m_a0; }

		/**
                 * Returns 3rd IIR coefficient
                 **/
		double getA2 () const { return m_a2*m_a0; }

		/**
                 * Returns 1st FIR coefficient
                 **/
		double getB0 () const { return m_b0*m_a0; }

		/**
                 * Returns 2nd FIR coefficient
                 **/
		double getB1 () const { return m_b1*m_a0; }

		/**
                 * Returns 3rd FIR coefficient
                 **/
		double getB2 () const { return m_b2*m_a0; }

		/** 
                 * Filter a sample with the coefficients provided here and the State provided as an argument.
                 * \param s The sample to be filtered.
                 * \param state The Delay lines (instance of a state from State.h)
		 * \return The filtered sample.
                 **/
		template <class StateType>
			inline double filter(double s, StateType& state) const
		{
			return state.filter(s, *this);
		}

	public:
		/**
                 * Sets all coefficients
                 * \param a0 1st IIR coefficient
                 * \param a1 2nd IIR coefficient
                 * \param a2 3rd IIR coefficient
                 * \param b0 1st FIR coefficient
                 * \param b1 2nd FIR coefficient
                 * \param b2 3rd FIR coefficient
                 **/
		void setCoefficients (double a0, double a1, double a2,
				      double b0, double b1, double b2);

		/**
                 * Sets one (real) pole and zero. Throws exception if imaginary components.
                 **/
		void setOnePole (complex_t pole, complex_t zero);

		/**
                 * Sets two poles/zoes as a pair. Needs to be complex conjugate.
                 **/
		void setTwoPole (complex_t pole1, complex_t zero1,
				 complex_t pole2, complex_t zero2);

		/**
		 * Sets a complex conjugate pair
                 **/
		void setPoleZeroPair (const PoleZeroPair& pair)
		{
			if (pair.isSinglePole ())
				setOnePole (pair.poles.first, pair.zeros.first);
			else
				setTwoPole (pair.poles.first, pair.zeros.first,
					    pair.poles.second, pair.zeros.second);
		}

		void setPoleZeroForm (const BiquadPoleState& bps);

		/**
                 * Sets the coefficiens as pass through. (b0=1,a0=1, rest zero)
                 **/
		void setIdentity ();

		/**
                 * Performs scaling operation on the FIR coefficients
                 * \param scale Mulitplies the coefficients b0,b1,b2 with the scaling factor scale.
                 **/
		void applyScale (double scale);

	public:
		double m_a0 = 1.0;
		double m_a1 = 0.0;
		double m_a2 = 0.0;
		double m_b1 = 0.0;
		double m_b2 = 0.0;
		double m_b0 = 1.0;
	};

//------------------------------------------------------------------------------

	
/** 
 * Expresses a biquad as a pair of pole/zeros, with gain
 * values so that the coefficients can be reconstructed precisely.
 **/
	struct DllExport BiquadPoleState : PoleZeroPair
	{
		BiquadPoleState () = default;

		explicit BiquadPoleState (const Biquad& s);

		double gain = 1.0;
	};

}
	
#endif
