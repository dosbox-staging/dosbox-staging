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

#ifndef IIR1_CHEBYSHEVI_H
#define IIR1_CHEBYSHEVI_H

#include "Common.h"
#include "Cascade.h"
#include "PoleFilter.h"
#include "State.h"

namespace Iir {

/**
 * Filters with Chebyshev response characteristics. The last parameter
 * defines the passband ripple in decibel.
 **/
namespace ChebyshevI {

/**
 * Analog lowpass prototypes (s-plane)
 **/
class DllExport AnalogLowPass : public LayoutBase
{
public:
	AnalogLowPass ();

	void design (const int numPoles,
		     double rippleDb);

private:
	int m_numPoles = 0;
	double m_rippleDb = 0.0;
};

/**
 * Analog lowpass shelf prototype (s-plane)
 **/
class DllExport AnalogLowShelf : public LayoutBase
{
public:
	AnalogLowShelf ();

	void design (int numPoles,
		     double gainDb,
		     double rippleDb);

private:
	int m_numPoles = 0;
	double m_rippleDb = 0.0;
	double m_gainDb = 0.0;
};

//------------------------------------------------------------------------------

struct DllExport LowPassBase : PoleFilterBase <AnalogLowPass>
{
  void setup (int order,
              double cutoffFrequency,
              double rippleDb);
};

struct DllExport HighPassBase : PoleFilterBase <AnalogLowPass>
{
  void setup (int order,
              double cutoffFrequency,
              double rippleDb);
};

struct DllExport BandPassBase : PoleFilterBase <AnalogLowPass>
{
  void setup (int order,
              double centerFrequency,
              double widthFrequency,
              double rippleDb);
};

struct DllExport BandStopBase : PoleFilterBase <AnalogLowPass>
{
  void setup (int order,
              double centerFrequency,
              double widthFrequency,
              double rippleDb);
};

struct DllExport LowShelfBase : PoleFilterBase <AnalogLowShelf>
{
  void setup (int order,
              double cutoffFrequency,
              double gainDb,
              double rippleDb);
};

struct DllExport HighShelfBase : PoleFilterBase <AnalogLowShelf>
{
  void setup (int order,
              double cutoffFrequency,
              double gainDb,
              double rippleDb);
};

struct DllExport BandShelfBase : PoleFilterBase <AnalogLowShelf>
{
  void setup (int order,
              double centerFrequency,
              double widthFrequency,
              double gainDb,
              double rippleDb);
};

//------------------------------------------------------------------------------

//
// Userland filters
//

/**
 * ChebyshevI lowpass filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport LowPass : PoleFilter <LowPassBase, StateType, FilterOrder>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double rippleDb) {
			LowPassBase::setup (FilterOrder,
					    cutoffFrequency / sampleRate,
					    rippleDb);
		}
		
		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double cutoffFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			LowPassBase::setup (reqOrder,
					    cutoffFrequency / sampleRate,
					    rippleDb);
		}



		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(double cutoffFrequency,
			    double rippleDb) {
			LowPassBase::setup (FilterOrder,
					    cutoffFrequency,
					    rippleDb);
		}
		
		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(int reqOrder,
			    double cutoffFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			LowPassBase::setup (reqOrder,
					    cutoffFrequency,
					    rippleDb);
		}
};

/**
 * ChebyshevI highpass filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport HighPass : PoleFilter <HighPassBase, StateType, FilterOrder>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double rippleDb) {
			HighPassBase::setup (FilterOrder,
					     cutoffFrequency / sampleRate,
					     rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double cutoffFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			HighPassBase::setup (reqOrder,
					     cutoffFrequency / sampleRate,
					     rippleDb);
		}



		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(double cutoffFrequency,
			    double rippleDb) {
			HighPassBase::setup (FilterOrder,
					     cutoffFrequency,
					     rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(int reqOrder,
			    double cutoffFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			HighPassBase::setup (reqOrder,
					     cutoffFrequency,
					     rippleDb);
		}
};

/**
 * ChebyshevI bandpass filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport BandPass : PoleFilter <BandPassBase, StateType, FilterOrder, FilterOrder*2>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the bandpass
                 * \param widthFrequency Frequency with of the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
      		void setup (double sampleRate,
			    double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			BandPassBase::setup (FilterOrder,
			       centerFrequency / sampleRate,
			       widthFrequency / sampleRate,
			       rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the bandpass
                 * \param widthFrequency Frequency with of the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			BandPassBase::setup (reqOrder,
			       centerFrequency / sampleRate,
			       widthFrequency / sampleRate,
			       rippleDb);
		}



		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param centerFrequency Normalised center frequency (0..1/2) of the bandpass
                 * \param widthFrequency Frequency with of the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
      		void setupN(double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			BandPassBase::setup (FilterOrder,
			       centerFrequency,
			       widthFrequency,
			       rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param centerFrequency Normalised center frequency (0..1/2) of the bandpass
                 * \param widthFrequency Frequency with of the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(int reqOrder,
			    double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			BandPassBase::setup (reqOrder,
			       centerFrequency,
			       widthFrequency,
			       rippleDb);
		}
};

/**
 * ChebyshevI bandstop filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport BandStop : PoleFilter <BandStopBase, StateType, FilterOrder, FilterOrder*2>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the notch
                 * \param widthFrequency Frequency with of the notch
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			BandStopBase::setup (FilterOrder,
					     centerFrequency / sampleRate,
					     widthFrequency / sampleRate,
					     rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the notch
                 * \param widthFrequency Frequency with of the notch
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			BandStopBase::setup (reqOrder,
					     centerFrequency / sampleRate,
					     widthFrequency / sampleRate,
					     rippleDb);
		}



		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param centerFrequency Normalised centre frequency (0..1/2) of the notch
                 * \param widthFrequency Frequency width of the notch
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			BandStopBase::setup (FilterOrder,
					     centerFrequency,
					     widthFrequency,
					     rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param centerFrequency Normalised centre frequency (0..1/2) of the notch
                 * \param widthFrequency Frequency width of the notch
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(int reqOrder,
			    double centerFrequency,
			    double widthFrequency,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			BandStopBase::setup (reqOrder,
					     centerFrequency,
					     widthFrequency,
					     rippleDb);
		}

};

/**
 * ChebyshevI low shelf filter. Specified gain in the passband. Otherwise 0 dB.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport LowShelf : PoleFilter <LowShelfBase, StateType, FilterOrder>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			LowShelfBase::setup (FilterOrder,
					     cutoffFrequency / sampleRate,
					     gainDb,
					     rippleDb);
		}
	
		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			LowShelfBase::setup (reqOrder,
					     cutoffFrequency / sampleRate,
					     gainDb,
					     rippleDb);
		}



		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			LowShelfBase::setup (FilterOrder,
					     cutoffFrequency,
					     gainDb,
					     rippleDb);
		}
	
		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(int reqOrder,
			    double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			LowShelfBase::setup (reqOrder,
					     cutoffFrequency,
					     gainDb,
					     rippleDb);
		}
};

/**
 * ChebyshevI high shelf filter. Specified gain in the passband. Otherwise 0 dB.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport HighShelf : PoleFilter <HighShelfBase, StateType, FilterOrder>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			HighShelfBase::setup (FilterOrder,
			       cutoffFrequency / sampleRate,
			       gainDb,
			       rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency.
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			HighShelfBase::setup (reqOrder,
			       cutoffFrequency / sampleRate,
			       gainDb,
			       rippleDb);
		}
		



		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			HighShelfBase::setup (FilterOrder,
			       cutoffFrequency,
			       gainDb,
			       rippleDb);
		}

		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param gainDb Gain in the passband
                 * \param rippleDb Permitted ripples in dB in the passband
                 **/
		void setupN(int reqOrder,
			    double cutoffFrequency,
			    double gainDb,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			HighShelfBase::setup (reqOrder,
			       cutoffFrequency,
			       gainDb,
			       rippleDb);
		}
		
};

/**
 * ChebyshevI bandshelf filter. Specified gain in the passband. Otherwise 0 dB.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
	struct DllExport BandShelf : PoleFilter <BandShelfBase, StateType, FilterOrder, FilterOrder*2>
	{
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the passband
                 * \param widthFrequency Width of the passband.
                 * \param gainDb Gain in the passband. The stopband has 0 dB.
                 * \param rippleDb Permitted ripples in dB in the passband.
                 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double widthFrequency,
			    double gainDb,
			    double rippleDb) {
			BandShelfBase::setup (FilterOrder,
					      centerFrequency / sampleRate,
					      widthFrequency / sampleRate,
					      gainDb,
					      rippleDb);
			
		}
		
		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the passband
                 * \param widthFrequency Width of the passband.
                 * \param gainDb Gain in the passband. The stopband has 0 dB.
                 * \param rippleDb Permitted ripples in dB in the passband.
                 **/
		void setup (int reqOrder,
			    double sampleRate,
			    double centerFrequency,
			    double widthFrequency,
			    double gainDb,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			BandShelfBase::setup (reqOrder,
					      centerFrequency / sampleRate,
					      widthFrequency / sampleRate,
					      gainDb,
					      rippleDb);

		}



		
		/**
		 * Calculates the coefficients of the filter at the order FilterOrder
                 * \param centerFrequency Normalised centre frequency (0..1/2) of the passband
                 * \param widthFrequency Width of the passband.
                 * \param gainDb Gain in the passband. The stopband has 0 dB.
                 * \param rippleDb Permitted ripples in dB in the passband.
                 **/
		void setupN(double centerFrequency,
			    double widthFrequency,
			    double gainDb,
			    double rippleDb) {
			BandShelfBase::setup (FilterOrder,
					      centerFrequency,
					      widthFrequency,
					      gainDb,
					      rippleDb);
			
		}
		
		/**
		 * Calculates the coefficients of the filter at specified order
		 * \param reqOrder Actual order for the filter calculations
                 * \param centerFrequency Normalised centre frequency (0..1/2) of the passband
                 * \param widthFrequency Width of the passband.
                 * \param gainDb Gain in the passband. The stopband has 0 dB.
                 * \param rippleDb Permitted ripples in dB in the passband.
                 **/
		void setupN(int reqOrder,
			    double centerFrequency,
			    double widthFrequency,
			    double gainDb,
			    double rippleDb) {
			if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
			BandShelfBase::setup (reqOrder,
					      centerFrequency,
					      widthFrequency,
					      gainDb,
					      rippleDb);

		}

	};

}

}

#endif
