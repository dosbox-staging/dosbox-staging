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

#include "Common.h"
#include "MathSupplement.h"
#include "Biquad.h"

namespace Iir {

	BiquadPoleState::BiquadPoleState (const Biquad& s)
	{
		const double a0 = s.getA0 ();
		const double a1 = s.getA1 ();
		const double a2 = s.getA2 ();
		const double b0 = s.getB0 ();
		const double b1 = s.getB1 ();
		const double b2 = s.getB2 ();

		if (a2 == 0 && b2 == 0)
		{
			// single pole
			poles.first = -a1;
			zeros.first = -b0 / b1;
			poles.second = 0;
			zeros.second = 0;
		}
		else
		{
			{
				const complex_t c = sqrt (complex_t (a1 * a1 - 4 * a0 * a2, 0));
				double d = 2. * a0;
				poles.first = -(a1 + c) / d;
				poles.second =  (c - a1) / d;
				if (poles.is_nan()) throw_invalid_argument("poles are NaN");
			}

			{
				const complex_t c = sqrt (complex_t (
								  b1 * b1 - 4 * b0 * b2, 0));
				double d = 2. * b0;
				zeros.first = -(b1 + c) / d;
				zeros.second =  (c - b1) / d;
				if (zeros.is_nan()) throw_invalid_argument("zeros are NaN");
			}
		}

		gain = b0 / a0;
	}

//------------------------------------------------------------------------------

/**
 * Gets the frequency response of the Biquad
 * \param normalizedFrequency Normalised frequency (0 to 0.5)
 **/
	complex_t Biquad::response (double normalizedFrequency) const
	{
		const double a0 = getA0 ();
		const double a1 = getA1 ();
		const double a2 = getA2 ();
		const double b0 = getB0 ();
		const double b1 = getB1 ();
		const double b2 = getB2 ();

		const double w = 2 * doublePi * normalizedFrequency;
		const complex_t czn1 = std::polar (1., -w);
		const complex_t czn2 = std::polar (1., -2 * w);
		complex_t ch (1);
		complex_t cbot (1);

		complex_t ct (b0/a0);
		complex_t cb (1);
		ct = addmul (ct, b1/a0, czn1);
		ct = addmul (ct, b2/a0, czn2);
		cb = addmul (cb, a1/a0, czn1);
		cb = addmul (cb, a2/a0, czn2);
		ch   *= ct;
		cbot *= cb;

		return ch / cbot;
	}

	std::vector<PoleZeroPair> Biquad::getPoleZeros () const
	{
		std::vector<PoleZeroPair> vpz;
		BiquadPoleState bps (*this);
		vpz.push_back (bps);
		return vpz;
	}

	void Biquad::setCoefficients (double a0, double a1, double a2,
					  double b0, double b1, double b2)
	{
		if (Iir::is_nan (a0)) throw_invalid_argument("a0 is NaN");
		if (Iir::is_nan (a1)) throw_invalid_argument("a1 is NaN");
		if (Iir::is_nan (a2)) throw_invalid_argument("a2 is NaN");
		if (Iir::is_nan (b0)) throw_invalid_argument("b0 is NaN");
		if (Iir::is_nan (b1)) throw_invalid_argument("b1 is NaN");
		if (Iir::is_nan (b2)) throw_invalid_argument("b2 is NaN");

		m_a0 = a0;
		m_a1 = a1/a0;
		m_a2 = a2/a0;
		m_b0 = b0/a0;
		m_b1 = b1/a0;
		m_b2 = b2/a0;
	}

	void Biquad::setOnePole (complex_t pole, complex_t zero)
	{
		if (pole.imag() != 0) throw_invalid_argument("Imaginary part of pole is non-zero.");
		if (zero.imag() != 0) throw_invalid_argument("Imaginary part of zero is non-zero.");

		const double a0 = 1;
		const double a1 = -pole.real();
		const double a2 = 0;
		const double b0 = -zero.real();
		const double b1 = 1;
		const double b2 = 0;

		setCoefficients (a0, a1, a2, b0, b1, b2);
	}

	void Biquad::setTwoPole (complex_t pole1, complex_t zero1,
				     complex_t pole2, complex_t zero2)
	{
		const double a0 = 1;
		double a1;
		double a2;
		const char errMsgPole[] = "imaginary parts of both poles need to be 0 or complex conjugate";
		const char errMsgZero[] = "imaginary parts of both zeros need to be 0 or complex conjugate";

		if (pole1.imag() != 0)
		{
			if (pole2 != std::conj (pole1))
				throw_invalid_argument(errMsgPole);
			a1 = -2 * pole1.real();
			a2 = std::norm (pole1);
		}
		else
		{
			if (pole2.imag() != 0)
				throw_invalid_argument(errMsgPole);
			a1 = -(pole1.real() + pole2.real());
			a2 =   pole1.real() * pole2.real();
		}

		const double b0 = 1;
		double b1;
		double b2;

		if (zero1.imag() != 0)
		{
			if (zero2 != std::conj (zero1))
				throw_invalid_argument(errMsgZero);
			b1 = -2 * zero1.real();
			b2 = std::norm (zero1);
		}
		else
		{
			if (zero2.imag() != 0)
				throw_invalid_argument(errMsgZero);

			b1 = -(zero1.real() + zero2.real());
			b2 =   zero1.real() * zero2.real();
		}

		setCoefficients (a0, a1, a2, b0, b1, b2);
	}

	void Biquad::setPoleZeroForm (const BiquadPoleState& bps)
	{
		setPoleZeroPair (bps);
		applyScale (bps.gain);
	}

	void Biquad::setIdentity ()
	{
		setCoefficients (1, 0, 0, 1, 0, 0);
	}

	void Biquad::applyScale (double scale)
	{
		m_b0 *= scale;
		m_b1 *= scale;
		m_b2 *= scale;
	}

}
