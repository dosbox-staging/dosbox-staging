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

#ifndef IIR1_STATE_H
#define IIR1_STATE_H

#include "Common.h"
#include "Biquad.h"


#define DEFAULT_STATE DirectFormII

namespace Iir {

/**
 * State for applying a second order section to a sample using Direct Form I
 *
 * Difference equation:
 *
 *  y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2]
 *                      - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]  
 **/
	class DllExport DirectFormI
	{
	public:
	DirectFormI () = default;
	
	void reset ()
	{
		m_x1 = 0;
		m_x2 = 0;
		m_y1 = 0;
		m_y2 = 0;
	}
	
	inline double filter(const double in,
			     const Biquad& s)
	{
		const double out = s.m_b0*in + s.m_b1*m_x1 + s.m_b2*m_x2
		- s.m_a1*m_y1 - s.m_a2*m_y2;
		m_x2 = m_x1;
		m_y2 = m_y1;
		m_x1 = in;
		m_y1 = out;
		
		return out;
	}
	
	protected:
	double m_x2 = 0.0; // x[n-2]
	double m_y2 = 0.0; // y[n-2]
	double m_x1 = 0.0; // x[n-1]
	double m_y1 = 0.0; // y[n-1]
	};

//------------------------------------------------------------------------------

/**
 * State for applying a second order section to a sample using Direct Form II
 *
 * Difference equation:
 *
 *  v[n] =         x[n] - (a1/a0)*v[n-1] - (a2/a0)*v[n-2]
 *  y(n) = (b0/a0)*v[n] + (b1/a0)*v[n-1] + (b2/a0)*v[n-2]
 *
 **/
	class DllExport DirectFormII
	{
	public:
	DirectFormII () = default;
	
	void reset ()
	{
		m_v1 = 0.0;
		m_v2 = 0.0;
	}
	
	inline double filter(const double in,
			     const Biquad& s)
	{
		const double w   = in - s.m_a1*m_v1 - s.m_a2*m_v2;
		const double out =      s.m_b0*w    + s.m_b1*m_v1 + s.m_b2*m_v2;
		
		m_v2 = m_v1;
		m_v1 = w;
		
		return out;
	}
	
	private:
	double m_v1 = 0.0; // v[-1]
	double m_v2 = 0.0; // v[-2]
	};


//------------------------------------------------------------------------------
	
	class DllExport TransposedDirectFormII
	{
	public:
	TransposedDirectFormII() = default;
	
	void reset ()
	{
		m_s1 = 0.0;
		m_s1_1 = 0.0;
		m_s2 = 0.0;
		m_s2_1 = 0.0;
	}
	
	inline double filter(const double in,
			     const Biquad& s)
	{
		const double out = m_s1_1 + s.m_b0*in;
		m_s1 = m_s2_1 + s.m_b1*in - s.m_a1*out;
		m_s2 = s.m_b2*in - s.m_a2*out;
		m_s1_1 = m_s1;
		m_s2_1 = m_s2;
		
		return out;
	}
	
	private:
	double m_s1 = 0.0;
	double m_s1_1 = 0.0;
	double m_s2 = 0.0;
	double m_s2_1 = 0.0;
	};

}

#endif
