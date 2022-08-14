/*
	==============================================================================
	This file is part of Tal-NoiseMaker by Patrick Kunz.

	Copyright(c) 2005-2010 Patrick Kunz, TAL
	Togu Audio Line, Inc.
	http://kunz.corrupt.ch

	This file may be licensed under the terms of of the
	GNU General Public License Version 2 (the ``GPL'').

	Software distributed under the License is distributed
	on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
	express or implied. See the GPL for the specific language
	governing rights and limitations.

	You should have received a copy of the GPL along with this
	program. If not, go to http://www.gnu.org/licenses/gpl.html
	or write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
	==============================================================================
 */

#if !defined(__Chorus_h)
#define __Chorus_h

#include "Lfo.h"
#include "OnePoleLP.h"
#include "math.h"

class Chorus
{
public:
	float sampleRate = {};
	float delayTime = {};

	Lfo *lfo = {};
	OnePoleLP *lp = {};

	int delayLineLength = {};
	float *delayLineStart = {};
	float *delayLineEnd = {};
	float *writePtr = {};
	float delayLineOutput = {};

	float rate;

	// Runtime variables
	float offset = {};
	float diff = {};
	float frac = {};
	float *ptr = {};
	float *ptr2 = {};

	int readPos = {};

	float z1 = {};
	float z2 = {};
	float mult = {};
	float sign = {};

	// lfo
	float lfoPhase = {};
	float lfoStepSize = {};
	float lfoSign = {};

	// prevent copying
	Chorus(const Chorus &) = delete;
	// prevent assignment
	Chorus &operator=(const Chorus &) = delete;

	Chorus(float _sampleRate, float phase, float _rate, float _delayTime) :
		sampleRate(_sampleRate),
		delayTime(_delayTime),

		lfo(new Lfo(sampleRate)),
		lp(new OnePoleLP()),

		delayLineLength((int) floorf(delayTime * sampleRate * 0.001f) * 2),

		//compute required buffer size for desired delay and allocate for it
		//add extra point to aid in interpolation later
		delayLineStart(new float[delayLineLength]),

		//set up pointers for delay line
		delayLineEnd(delayLineStart + delayLineLength),

		writePtr(delayLineStart),

		delayLineOutput(0.0f),

		rate(_rate),
		z1(0.0f),
		z2(0.0f),
		sign(0.0f),
		lfoPhase(phase * 2.0f - 1.0f),
		lfoStepSize(4.0f * rate / sampleRate),
		lfoSign(1.0f)
	{
		lfo->phase= phase;
		lfo->setRate(rate);

		//zero out the buffer (silence)
		do {
			*writePtr= 0.0f;
		}
		while (++writePtr < delayLineEnd);

		writePtr = delayLineStart;
	}

    ~Chorus()
    {
        delete[] delayLineStart;
        delete lp;
        delete lfo;
    }

	float process(float *sample)
	{
		// Get delay time
		offset= (nextLFO()*0.3f+0.4f)*delayTime*sampleRate*0.001f;

		// Compute the largest read pointer based on the offset.  If ptr
		// Is before the first delayline location, wrap around end point
		ptr = writePtr-(int)floorf(offset);
		if (ptr<delayLineStart)
			ptr+= delayLineLength;

		assert(ptr >= delayLineStart);
		assert(ptr < delayLineEnd);

		ptr2= ptr-1;
		if (ptr2<delayLineStart)
			ptr2+= delayLineLength;

		assert(ptr2 >= delayLineStart);
		assert(ptr2 < delayLineEnd);

		frac= offset-(int)floorf(offset);
		delayLineOutput= *ptr2+*ptr*(1-frac)-(1-frac)*z1;
		z1 = delayLineOutput;

		// Low pass
		lp->tick(&delayLineOutput, 0.95f);

		// Write the input sample and any feedback to delayline
		assert(writePtr >= delayLineStart);
		assert(writePtr < delayLineEnd);
		*writePtr= *sample;

		// Increment buffer index and wrap if necesary
		if (++writePtr >= delayLineEnd)
			writePtr = delayLineStart;

		return delayLineOutput;
	}

	inline float nextLFO()
	{
		if (lfoPhase>=1.0f)
		{
			lfoSign= -1.0f;
		}
		else if (lfoPhase<=-1.0f)
		{
			lfoSign= +1.0f;
		}
		lfoPhase+= lfoStepSize*lfoSign;
		return lfoPhase;
	}
};

#endif
