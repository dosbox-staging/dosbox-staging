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

#ifndef IIR1_RBJ_H
#define IIR1_RBJ_H

#include "Common.h"
#include "Biquad.h"
#include "State.h"

namespace Iir {

/**
 * Filter realizations based on Robert Bristol-Johnson formulae:
 *
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *
 * These are all 2nd order filters which are tuned with the Q (or Quality factor).
 * The Q factor causes a resonance at the cutoff frequency. The higher the Q
 * factor the higher the responance. If 0.5 < Q < 1/sqrt(2) then there is no resonance peak.
 * Above 1/sqrt(2) the peak becomes more and more pronounced. For bandpass and stopband
 * the Q factor is replaced by the width of the filter. The higher Q the more narrow
 * the bandwidth of the notch or bandpass.
 *
 **/

#define ONESQRT2 (1/sqrt(2))
	
namespace RBJ {

	/** 
         * The base class of all RBJ filters
         **/
	struct DllExport RBJbase : Biquad
	{
	public:
		/// filter operation
		template <typename Sample>
			inline Sample filter(Sample s) {
			return static_cast<Sample>(state.filter((double)s,*this));
		}
		/// resets the delay lines to zero
		void reset() {
			state.reset();
		}
		/// gets the delay lines (=state) of the filter
		const DirectFormI& getState() {
			return state;
		}
	private:
		DirectFormI state;
	};

	/**
         * Lowpass.
         **/
	struct DllExport LowPass : RBJbase
	{
		/**
                 * Calculates the coefficients
                 * \param cutoffFrequency Normalised cutoff frequency
                 * \param q Q factor determines the resonance peak at the cutoff.
                 **/
		void setupN(double cutoffFrequency,
			    double q = ONESQRT2);
		
		/**
                 * Calculates the coefficients
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency
                 * \param q Q factor determines the resonance peak at the cutoff.
                 **/
		void setup(double sampleRate,
			   double cutoffFrequency,
			   double q = ONESQRT2) {
			setupN(cutoffFrequency / sampleRate, q);
		}
	};

	/**
         * Highpass.
         **/
	struct DllExport HighPass : RBJbase
	{
		/**
                 * Calculates the coefficients
                 * \param cutoffFrequency Normalised cutoff frequency (0..1/2)
                 * \param q Q factor determines the resonance peak at the cutoff.
                 **/
		void setupN(double cutoffFrequency,
			    double q = ONESQRT2);
		/**
                 * Calculates the coefficients
                 * \param sampleRate Sampling rate
                 * \param cutoffFrequency Cutoff frequency
                 * \param q Q factor determines the resonance peak at the cutoff.
                 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double q = ONESQRT2) {
			setupN(cutoffFrequency / sampleRate, q);
		}
	};

	/**
         * Bandpass with constant skirt gain
         **/
	struct DllExport BandPass1 : RBJbase
	{
		/**
                 * Calculates the coefficients
                 * \param centerFrequency Center frequency of the bandpass
                 * \param bandWidth Bandwidth in octaves
                 **/
		void setupN(double centerFrequency,
			    double bandWidth);
		/**
                 * Calculates the coefficients
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the bandpass
                 * \param bandWidth Bandwidth in octaves
                 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double bandWidth) {
			setupN(centerFrequency / sampleRate, bandWidth);
		}
	};

	/**
         * Bandpass with constant 0 dB peak gain
         **/
	struct DllExport BandPass2 : RBJbase
	{
		/**
                 * Calculates the coefficients
                 * \param centerFrequency Normalised centre frequency of the bandpass
                 * \param bandWidth Bandwidth in octaves
                 **/
		void setupN(double centerFrequency,
			    double bandWidth);
		/**
                 * Calculates the coefficients
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the bandpass
                 * \param bandWidth Bandwidth in octaves
                 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double bandWidth) {
			setupN(centerFrequency / sampleRate, bandWidth);
		}
	};

	/**
         * Bandstop filter. Warning: the bandwidth might not be accurate
         * for narrow notches.
         **/
	struct DllExport BandStop : RBJbase
	{
		/**
                 * Calculates the coefficients
                 * \param centerFrequency Normalised Centre frequency of the bandstop
                 * \param bandWidth Bandwidth in octaves
                 **/
		void setupN(double centerFrequency,
			    double bandWidth);
		/**
                 * Calculates the coefficients
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the bandstop
                 * \param bandWidth Bandwidth in octaves
                 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double bandWidth) {
			setupN(centerFrequency / sampleRate, bandWidth);
		}
	};

	/**
         * Bandstop with Q factor: the higher the Q factor the more narrow is
         * the notch. 
         * However, a narrow notch has a long impulse response ( = ringing)
         * and numerical problems might prevent perfect damping. Practical values
         * of the Q factor are about Q = 10 to 20. In terms of the design
         * the Q factor defines the radius of the
         * poles as r = exp(- pi*(centerFrequency/sampleRate)/q_factor) whereas
         * the angles of the poles/zeros define the bandstop frequency. The higher
         * Q the closer r moves towards the unit circle.
         **/
	struct DllExport IIRNotch : RBJbase
	{
		/**
                 * Calculates the coefficients
                 * \param centerFrequency Normalised centre frequency of the notch
                 * \param q_factor Q factor of the notch (1 to ~20)
                 **/
		void setupN(double centerFrequency,
			    double q_factor = 10);
		/**
                 * Calculates the coefficients
                 * \param sampleRate Sampling rate
                 * \param centerFrequency Center frequency of the notch
                 * \param q_factor Q factor of the notch (1 to ~20)
                 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double q_factor = 10) {
			setupN(centerFrequency / sampleRate, q_factor);
		}
	};

	/**
         * Low shelf: 0db in the stopband and gainDb in the passband.
         **/
	struct DllExport LowShelf : RBJbase
	{
		/**
		 * Calculates the coefficients
		 * \param cutoffFrequency Normalised cutoff frequency
		 * \param gainDb Gain in the passband
                 * \param shelfSlope Slope between stop/passband. 1 = as steep as it can.
		 **/
		void setupN(double cutoffFrequency,
			    double gainDb,
			    double shelfSlope = 1);
		/**
		 * Calculates the coefficients
		 * \param sampleRate Sampling rate
		 * \param cutoffFrequency Cutoff frequency
		 * \param gainDb Gain in the passband
                 * \param shelfSlope Slope between stop/passband. 1 = as steep as it can.
		 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double gainDb,
			    double shelfSlope = 1) {
			setupN(	cutoffFrequency / sampleRate, gainDb, shelfSlope);
		}
	};

	/**
         * High shelf: 0db in the stopband and gainDb in the passband.
         **/
	struct DllExport HighShelf : RBJbase
	{
		/**
		 * Calculates the coefficients
		 * \param cutoffFrequency Normalised cutoff frequency
		 * \param gainDb Gain in the passband
                 * \param shelfSlope Slope between stop/passband. 1 = as steep as it can.
		 **/
		void setupN(double cutoffFrequency,
			    double gainDb,
			    double shelfSlope = 1);
		/**
		 * Calculates the coefficients
		 * \param sampleRate Sampling rate
		 * \param cutoffFrequency Cutoff frequency
		 * \param gainDb Gain in the passband
                 * \param shelfSlope Slope between stop/passband. 1 = as steep as it can.
		 **/
		void setup (double sampleRate,
			    double cutoffFrequency,
			    double gainDb,
			    double shelfSlope = 1) {
			setupN(	cutoffFrequency / sampleRate, gainDb, shelfSlope);
		}
	};

	/**
         * Band shelf: 0db in the stopband and gainDb in the passband.
         **/
	struct DllExport BandShelf : RBJbase
	{
		/**
		 * Calculates the coefficients
		 * \param centerFrequency Normalised centre frequency
		 * \param gainDb Gain in the passband
                 * \param bandWidth Bandwidth in octaves
		 **/
		void setupN(double centerFrequency,
			    double gainDb,
			    double bandWidth);
		/**
		 * Calculates the coefficients
		 * \param sampleRate Sampling rate
		 * \param centerFrequency frequency
		 * \param gainDb Gain in the passband
                 * \param bandWidth Bandwidth in octaves
		 **/
		void setup (double sampleRate,
			    double centerFrequency,
			    double gainDb,
			    double bandWidth) {
			setupN(centerFrequency / sampleRate, gainDb, bandWidth);
		}
	};

	/**
	 * Allpass filter
	 **/
	struct DllExport AllPass : RBJbase
	{
		/**
		 * Calculates the coefficients
		 * \param phaseFrequency Normalised frequency where the phase flips
		 * \param q Q-factor
		 **/
		void setupN(double phaseFrequency,
			    double q  = ONESQRT2);
		
		/**
		 * Calculates the coefficients
		 * \param sampleRate Sampling rate
		 * \param phaseFrequency Frequency where the phase flips
		 * \param q Q-factor
		 **/
		void setup (double sampleRate,
			    double phaseFrequency,
			    double q  = ONESQRT2) {
			setupN(	phaseFrequency / sampleRate, q);
		}
	};
	
}

}


#endif
