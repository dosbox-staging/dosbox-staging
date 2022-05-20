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
#include "RBJ.h"

namespace Iir {

namespace RBJ {

	void LowPass::setupN(double cutoffFrequency,
			    double q)
	{
		double w0 = 2 * doublePi * cutoffFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / (2 * q);
		double b0 = (1 - cs) / 2;
		double b1 =  1 - cs;
		double b2 = (1 - cs) / 2;
		double a0 =  1 + AL;
		double a1 = -2 * cs;
		double a2 =  1 - AL;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}

	void HighPass::setupN (double cutoffFrequency,
			      double q)
	{
		double w0 = 2 * doublePi * cutoffFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / ( 2 * q );
		double b0 =  (1 + cs) / 2;
		double b1 = -(1 + cs);
		double b2 =  (1 + cs) / 2;
		double a0 =  1 + AL;
		double a1 = -2 * cs;
		double a2 =  1 - AL;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}

	void BandPass1::setupN (double centerFrequency,
			       double bandWidth)
	{
		double w0 = 2 * doublePi * centerFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / ( 2 * bandWidth );
		double b0 = bandWidth * AL;// sn / 2;
		double b1 =  0;
		double b2 = -bandWidth * AL;//-sn / 2;
		double a0 =  1 + AL;
		double a1 = -2 * cs;
		double a2 =  1 - AL;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
	void BandPass2::setupN (double centerFrequency,
			       double bandWidth)
	{
		double w0 = 2 * doublePi * centerFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / ( 2 * bandWidth );
		double b0 =  AL;
		double b1 =  0;
		double b2 = -AL;
		double a0 =  1 + AL;
		double a1 = -2 * cs;
		double a2 =  1 - AL;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
	void BandStop::setupN (double centerFrequency,
			      double bandWidth)
	{
		double w0 = 2 * doublePi * centerFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / ( 2 * bandWidth );
		double b0 =  1;
		double b1 = -2 * cs;
		double b2 =  1;
		double a0 =  1 + AL;
		double a1 = -2 * cs;
		double a2 =  1 - AL;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
	void IIRNotch::setupN (double centerFrequency,
			      double q_factor)
	{
		double w0 = 2 * doublePi * centerFrequency;
		double cs = cos (w0);
		double r = exp(-(w0/2) / q_factor);
		double b0 =  1;
		double b1 = -2 * cs;
		double b2 =  1;
		double a0 =  1;
		double a1 = -2 * r * cs;
		double a2 =  r * r;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
	void LowShelf::setupN (double cutoffFrequency,
			      double gainDb,
			      double shelfSlope)
	{
		double A  = pow (10, gainDb/40);
		double w0 = 2 * doublePi * cutoffFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / 2 * ::std::sqrt ((A + 1/A) * (1/shelfSlope - 1) + 2);
		double sq = 2 * sqrt(A) * AL;
		double b0 =    A*( (A+1) - (A-1)*cs + sq );
		double b1 =  2*A*( (A-1) - (A+1)*cs );
		double b2 =    A*( (A+1) - (A-1)*cs - sq );
		double a0 =        (A+1) + (A-1)*cs + sq;
		double a1 =   -2*( (A-1) + (A+1)*cs );
		double a2 =        (A+1) + (A-1)*cs - sq;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
	
	void HighShelf::setupN (double cutoffFrequency,
			       double gainDb,
			       double shelfSlope)
	{
		double A  = pow (10, gainDb/40);
		double w0 = 2 * doublePi * cutoffFrequency;
		double cs = cos (w0);
		double sn = sin (w0);
		double AL = sn / 2 * ::std::sqrt ((A + 1/A) * (1/shelfSlope - 1) + 2);
		double sq = 2 * sqrt(A) * AL;
		double b0 =    A*( (A+1) + (A-1)*cs + sq );
		double b1 = -2*A*( (A-1) + (A+1)*cs );
		double b2 =    A*( (A+1) + (A-1)*cs - sq );
		double a0 =        (A+1) - (A-1)*cs + sq;
		double a1 =    2*( (A-1) - (A+1)*cs );
		double a2 =        (A+1) - (A-1)*cs - sq;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
	
	void BandShelf::setupN (double centerFrequency,
			       double gainDb,
			       double bandWidth)
	{
		double A  = pow (10, gainDb/40);
		double w0 = 2 * doublePi * centerFrequency;
		double cs = cos(w0);
		double sn = sin(w0);
		double AL = sn * sinh( doubleLn2/2 * bandWidth * w0/sn );
		if (Iir::is_nan (AL))
			throw_invalid_argument("No solution available for these parameters.\n");
		double b0 =  1 + AL * A;
		double b1 = -2 * cs;
		double b2 =  1 - AL * A;
		double a0 =  1 + AL / A;
		double a1 = -2 * cs;
		double a2 =  1 - AL / A;
		setCoefficients (a0, a1, a2, b0, b1, b2);
	}
	
void AllPass::setupN (double phaseFrequency,
                     double q)
{
	double w0 = 2 * doublePi * phaseFrequency;
	double cs = cos (w0);
	double sn = sin (w0);
	double AL = sn / ( 2 * q );
	double b0 =  1 - AL;
	double b1 = -2 * cs;
	double b2 =  1 + AL;
	double a0 =  1 + AL;
	double a1 = -2 * cs;
	double a2 =  1 - AL;
	setCoefficients (a0, a1, a2, b0, b1, b2);
}
	
}

}
