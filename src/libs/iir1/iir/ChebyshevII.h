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

#ifndef IIR1_CHEBYSHEVII_H
#define IIR1_CHEBYSHEVII_H

#include "Common.h"
#include "Cascade.h"
#include "PoleFilter.h"
#include "State.h"

namespace Iir {

/**
 * Filters with ChebyshevII response characteristics. The last parameter
 * defines the minimal stopband rejection requested. Generally there will be frequencies where
 * the rejection is much better but this parameter guarantees that the rejection is at least
 * as specified.
 *
 **/
namespace ChebyshevII {

/**
 * Analogue lowpass prototype (s-plane)
 **/
class DllExport AnalogLowPass : public LayoutBase
{
public:
	AnalogLowPass ();

	void design (const int numPoles,
		     double stopBandDb);

private:
	int m_numPoles = 0;
	double m_stopBandDb = 0.0;
};


/**
 * Analogue shelf lowpass prototype (s-plane)
 **/
class DllExport AnalogLowShelf : public LayoutBase
{
public:
	AnalogLowShelf ();

	void design (int numPoles,
		     double gainDb,
		     double stopBandDb);

private:
	int m_numPoles = 0;
	double m_stopBandDb = 0.0;
	double m_gainDb = 0.0;
};

//------------------------------------------------------------------------------

struct DllExport LowPassBase : PoleFilterBase <AnalogLowPass>
{
	void setup (int order,
		    double cutoffFrequency,
		    double stopBandDb);
};

struct DllExport HighPassBase : PoleFilterBase <AnalogLowPass>
{
	void setup (int order,
		    double cutoffFrequency,
		    double stopBandDb);
};

struct DllExport BandPassBase : PoleFilterBase <AnalogLowPass>
{
	void setup (int order,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb);
};

struct DllExport BandStopBase : PoleFilterBase <AnalogLowPass>
{
	void setup (int order,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb);
};

struct DllExport LowShelfBase : PoleFilterBase <AnalogLowShelf>
{
	void setup (int order,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb);
};

struct DllExport HighShelfBase : PoleFilterBase <AnalogLowShelf>
{
	void setup (int order,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb);
};

struct DllExport BandShelfBase : PoleFilterBase <AnalogLowShelf>
{
	void setup (int order,
		    double centerFrequency,
		    double widthFrequency,
		    double gainDb,
		    double stopBandDb);
};

//------------------------------------------------------------------------------

//
// Userland filters
//

/**
 * ChebyshevII lowpass filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport LowPass : PoleFilter <LowPassBase, StateType, FilterOrder>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double cutoffFrequency,
		    double stopBandDb) {
		LowPassBase::setup (FilterOrder,
				    cutoffFrequency / sampleRate,
				    stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double cutoffFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		LowPassBase::setup (reqOrder,
				    cutoffFrequency / sampleRate,
				    stopBandDb);
	}





	/**
	 * Calculates the coefficients of the filter
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double cutoffFrequency,
		    double stopBandDb) {
		LowPassBase::setup (FilterOrder,
				    cutoffFrequency,
				    stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double cutoffFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		LowPassBase::setup (reqOrder,
				    cutoffFrequency,
				    stopBandDb);
	}

};

/**
 * ChebyshevII highpass filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport HighPass : PoleFilter <HighPassBase, StateType, FilterOrder>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double cutoffFrequency,
		    double stopBandDb) {
		HighPassBase::setup (FilterOrder,
				     cutoffFrequency / sampleRate,
				     stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double cutoffFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		HighPassBase::setup (reqOrder,
				     cutoffFrequency / sampleRate,
				     stopBandDb);
	}




	/**
	 * Calculates the coefficients of the filter
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double cutoffFrequency,
		    double stopBandDb) {
		HighPassBase::setup (FilterOrder,
				     cutoffFrequency,
				     stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double cutoffFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		HighPassBase::setup (reqOrder,
				     cutoffFrequency,
				     stopBandDb);
	}

};

/**
 * ChebyshevII bandpass filter
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport BandPass : PoleFilter <BandPassBase, StateType, FilterOrder, FilterOrder*2>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param centerFrequency Center frequency of the bandpass
         * \param widthFrequency Width of the bandpass
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		BandPassBase::setup (FilterOrder,
				     centerFrequency / sampleRate,
				     widthFrequency / sampleRate,
				     stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param centerFrequency Center frequency of the bandpass
         * \param widthFrequency Width of the bandpass
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		BandPassBase::setup (reqOrder,
				     centerFrequency / sampleRate,
				     widthFrequency / sampleRate,
				     stopBandDb);
	}




	/**
	 * Calculates the coefficients of the filter
	 * \param centerFrequency Normalised centre frequency (0..1/2) of the bandpass
         * \param widthFrequency Width of the bandpass
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		BandPassBase::setup (FilterOrder,
				     centerFrequency,
				     widthFrequency,
				     stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param centerFrequency Normalised centre frequency (0..1/2) of the bandpass
         * \param widthFrequency Width of the bandpass
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		BandPassBase::setup (reqOrder,
				     centerFrequency,
				     widthFrequency,
				     stopBandDb);
	}
};

/**
 * ChebyshevII bandstop filter.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 */
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport BandStop : PoleFilter <BandStopBase, StateType, FilterOrder, FilterOrder*2>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param centerFrequency Center frequency of the bandstop
         * \param widthFrequency Width of the bandstop
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		BandStopBase::setup (FilterOrder,
				     centerFrequency / sampleRate,
				     widthFrequency / sampleRate,
				     stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param centerFrequency Center frequency of the bandstop
         * \param widthFrequency Width of the bandstop
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		BandStopBase::setup (reqOrder,
				     centerFrequency / sampleRate,
				     widthFrequency / sampleRate,
				     stopBandDb);
	}




	/**
	 * Calculates the coefficients of the filter
	 * \param centerFrequency Normalised centre frequency (0..1/2) of the bandstop
         * \param widthFrequency Width of the bandstop
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		BandStopBase::setup (FilterOrder,
				     centerFrequency,
				     widthFrequency,
				     stopBandDb);
	}

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param centerFrequency Normalised centre frequency (0..1/2) of the bandstop
         * \param widthFrequency Width of the bandstop
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double centerFrequency,
		    double widthFrequency,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		BandStopBase::setup (reqOrder,
				     centerFrequency,
				     widthFrequency,
				     stopBandDb);
	}
};

/**
 * ChebyshevII low shelf filter. Specified gain in the passband and 0dB in the stopband.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport LowShelf : PoleFilter <LowShelfBase, StateType, FilterOrder>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
         * \param gainDb Gain of the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		LowShelfBase::setup (FilterOrder,
				     cutoffFrequency / sampleRate,
				     gainDb,
				     stopBandDb);
	}
	
	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency
         * \param gainDb Gain of the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		LowShelfBase::setup (reqOrder,
				     cutoffFrequency / sampleRate,
				     gainDb,
				     stopBandDb);
	}
	




	/**
	 * Calculates the coefficients of the filter
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
         * \param gainDb Gain of the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		LowShelfBase::setup (FilterOrder,
				     cutoffFrequency,
				     gainDb,
				     stopBandDb);
	}
	
	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
         * \param gainDb Gain the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		LowShelfBase::setup (reqOrder,
				     cutoffFrequency,
				     gainDb,
				     stopBandDb);
	}
	
};

/**
 * ChebyshevII high shelf filter. Specified gain in the passband and 0dB in the stopband.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport HighShelf : PoleFilter <HighShelfBase, StateType, FilterOrder>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
         * \param gainDb Gain the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		HighShelfBase::setup (FilterOrder,
				      cutoffFrequency / sampleRate,
				      gainDb,
				      stopBandDb);
	}
	
	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param cutoffFrequency Cutoff frequency.
         * \param gainDb Gain the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		HighShelfBase::setup (reqOrder,
				      cutoffFrequency / sampleRate,
				      gainDb,
				      stopBandDb);
	}
	





	/**
	 * Calculates the coefficients of the filter
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
         * \param gainDb Gain the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		HighShelfBase::setup (FilterOrder,
				      cutoffFrequency,
				      gainDb,
				      stopBandDb);
	}
	
	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
         * \param gainDb Gain the passbard. The stopband has 0 dB gain.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double cutoffFrequency,
		    double gainDb,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		HighShelfBase::setup (reqOrder,
				      cutoffFrequency,
				      gainDb,
				      stopBandDb);
	}
	
};

/**
 * ChebyshevII bandshelf filter. Bandpass with specified gain and 0 dB gain in the stopband.
 * \param FilterOrder Reserves memory for a filter of the order FilterOrder
 * \param StateType The filter topology: DirectFormI, DirectFormII, ...
 **/
template <int FilterOrder = DEFAULT_FILTER_ORDER, class StateType = DEFAULT_STATE>
struct DllExport BandShelf : PoleFilter <BandShelfBase, StateType, FilterOrder, FilterOrder*2>
{
	/**
	 * Calculates the coefficients of the filter
	 * \param sampleRate Sampling rate
	 * \param centerFrequency Center frequency of the bandpass
         * \param widthFrequency Width of the bandpass
         * \param gainDb Gain in the passband. The stopband has always 0dB.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (double sampleRate,
		    double centerFrequency,
		    double widthFrequency,
		    double gainDb,
		    double stopBandDb) {
		BandShelfBase::setup (FilterOrder,
				      centerFrequency / sampleRate,
				      widthFrequency / sampleRate,
				      gainDb,
				      stopBandDb);
	}
	  

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param sampleRate Sampling rate
	 * \param centerFrequency Center frequency of the bandpass
         * \param widthFrequency Width of the bandpass
         * \param gainDb Gain in the passband. The stopband has always 0dB.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setup (int reqOrder,
		    double sampleRate,
		    double centerFrequency,
		    double widthFrequency,
		    double gainDb,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		BandShelfBase::setup (reqOrder,
				      centerFrequency / sampleRate,
				      widthFrequency / sampleRate,
				      gainDb,
				      stopBandDb);
	}
	  






	/**
	 * Calculates the coefficients of the filter
	 * \param centerFrequency Normalised centre frequency (0..1/2) of the bandpass
         * \param widthFrequency Width of the bandpass
         * \param gainDb Gain in the passband. The stopband has always 0dB.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(double centerFrequency,
		    double widthFrequency,
		    double gainDb,
		    double stopBandDb) {
		BandShelfBase::setup (FilterOrder,
				      centerFrequency,
				      widthFrequency,
				      gainDb,
				      stopBandDb);
	}
	  

	/**
	 * Calculates the coefficients of the filter
	 * \param reqOrder Requested order which can be less than the instantiated one
	 * \param centerFrequency Normalised centre frequency (0..1/2) of the bandpass
         * \param widthFrequency Width of the bandpass
         * \param gainDb Gain in the passband. The stopband has always 0dB.
	 * \param stopBandDb Permitted ripples in dB in the stopband
	 **/
	void setupN(int reqOrder,
		    double centerFrequency,
		    double widthFrequency,
		    double gainDb,
		    double stopBandDb) {
		if (reqOrder > FilterOrder) throw_invalid_argument(orderTooHigh);
		BandShelfBase::setup (reqOrder,
				      centerFrequency,
				      widthFrequency,
				      gainDb,
				      stopBandDb);
	}
	  

};

}

}

#endif
