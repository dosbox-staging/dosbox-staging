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

#ifndef IIR1_CASCADE_H
#define IIR1_CASCADE_H

#include "Common.h"
#include "Biquad.h"
#include "Layout.h"
#include "MathSupplement.h"

namespace Iir {

/**
 * Holds coefficients for a cascade of second order sections.
 **/
        class DllExport Cascade
        {
        public:
        
        /**
         * Pointer to an array of Biquads
         **/
        struct DllExport Storage
        {
                /**
                 * Copy-constructor which receives the pointer to the Biquad array and the number of Biquads
                 * \param maxStages_ Number of biquads
                 * \param stageArray_ The array of the Biquads
                 **/
                Storage (int maxStages_, Biquad* const stageArray_)
                : maxStages (maxStages_)
                , stageArray (stageArray_)
                {
                }

                const int maxStages = 0;
                Biquad* const stageArray = nullptr;
        };

        /**
         * Returns the number of Biquads kept here
         **/
        int getNumStages () const
        {
                return m_numStages;
        }

        /**
         * Returns a reference to a biquad
         **/
        const Biquad& operator[] (int index)
        {
                if ((index < 0) || (index >= m_numStages))
                        throw_invalid_argument("Index out of bounds.");
                return m_stageArray[index];
        }

        /**
         * Calculate filter response at the given normalized frequency
         * \param normalizedFrequency Frequency from 0 to 0.5 (Nyquist)
         **/
        complex_t response (double normalizedFrequency) const;

        /**
         * Returns a vector with all pole/zero pairs of the whole Biqad cascade
         **/
        std::vector<PoleZeroPair> getPoleZeros () const;

        protected:
        Cascade ();

        void setCascadeStorage (const Storage& storage);

        void applyScale (double scale);

        void setLayout (const LayoutBase& proto);

        private:
        int m_numStages = 0;
        int m_maxStages = 0;
        Biquad* m_stageArray = nullptr;
        };


        
//------------------------------------------------------------------------------
        
/**
 * Storage for Cascade: This holds a chain of 2nd order filters
 * with its coefficients.
 **/
        template <int MaxStages,class StateType>
        class DllExport CascadeStages {
        public:
        /**
         * Resets all biquads (i.e. the delay lines but not the coefficients)
         **/
        void reset ()
        {
                for (auto &state: m_states)
                        state.reset();
        }
        
        public:
        /**
         * Sets the coefficients of the whole chain of
         * biquads.
         * \param sosCoefficients 2D array in Python style sos ordering: 0-2: FIR, 3-5: IIR coeff.
         **/
        void setup (const double (&sosCoefficients)[MaxStages][6]) {
                for (int i = 0; i < MaxStages; i++) {
                        m_stages[i].setCoefficients(
                                sosCoefficients[i][3],
                                sosCoefficients[i][4],
                                sosCoefficients[i][5],
                                sosCoefficients[i][0],
                                sosCoefficients[i][1],
                                sosCoefficients[i][2]);
                }
        }

        public:
        /**
         * Filters one sample through the whole chain of biquads and return the result
         * \param in Sample to be filtered
         * \return filtered sample
         **/
        template <typename Sample>
        inline Sample filter(const Sample in)
        {
                double out = in;
                StateType* state = m_states;
                for (const auto &stage: m_stages)
                        out = (state++)->filter(out, stage);
                return static_cast<Sample> (out);
        }

	/**
	 * Returns the coefficients of the entire Biquad chain
	 **/
        const Cascade::Storage getCascadeStorage()
        {
                return Cascade::Storage (MaxStages, m_stages);
        }
        
        private:
        Biquad m_stages[MaxStages] = {};
        StateType m_states[MaxStages] = {};
        };
        
}

#endif
