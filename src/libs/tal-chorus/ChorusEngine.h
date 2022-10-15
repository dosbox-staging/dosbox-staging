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

#if !defined(__ChorusEngine_h)
#define __ChorusEngine_h

#include <memory>

#include "Chorus.h"
#include "Params.h"
#include "DCBlock.h"


class ChorusEngine
{
public:
    std::unique_ptr<Chorus> chorus1L = {};
    std::unique_ptr<Chorus> chorus1R = {};
    std::unique_ptr<Chorus> chorus2L = {};
    std::unique_ptr<Chorus> chorus2R = {};

    DCBlock dcBlock1L = {};
    DCBlock dcBlock1R = {};
    DCBlock dcBlock2L = {};
    DCBlock dcBlock2R = {};

    bool isChorus1Enabled = false;
    bool isChorus2Enabled = false;

	// prevent copying
	ChorusEngine(const ChorusEngine &) = delete;
	// prevent assignment
	ChorusEngine &operator=(const ChorusEngine &) = delete;

    ChorusEngine(float sampleRate)
    {
        setUpChorus(sampleRate);
    }

    void setSampleRate(float sampleRate)
    {
        setUpChorus(sampleRate);
        setEnablesChorus(false, false);
    }

    void setEnablesChorus(bool isChorus1Enabled, bool isChorus2Enabled)
    {
        this->isChorus1Enabled = isChorus1Enabled;
        this->isChorus2Enabled = isChorus2Enabled;
    }

    void setUpChorus(float sampleRate)
    {
        chorus1L= std::make_unique<Chorus>(sampleRate, 1.0f, 0.5f, 7.0f);
        chorus1R= std::make_unique<Chorus>(sampleRate, 0.0f, 0.5f, 7.0f);
        chorus2L= std::make_unique<Chorus>(sampleRate, 0.0f, 0.83f, 7.0f);
        chorus2R= std::make_unique<Chorus>(sampleRate, 1.0f, 0.83f, 7.0f);
    }

    inline void process(float *sampleL, float *sampleR)
    {
        float resultR= 0.0f;
        float resultL= 0.0f;
        if (isChorus1Enabled)
        {
            resultL+= chorus1L->process(sampleL);
            resultR+= chorus1R->process(sampleR);
            dcBlock1L.tick(&resultL, 0.01f);
            dcBlock1R.tick(&resultR, 0.01f);
        }
        if (isChorus2Enabled)
        {
            resultL+= chorus2L->process(sampleL);
            resultR+= chorus2R->process(sampleR);
            dcBlock2L.tick(&resultL, 0.01f);
            dcBlock2R.tick(&resultR, 0.01f);
        }
        *sampleL= *sampleL+resultL*1.4f;
        *sampleR= *sampleR+resultR*1.4f;
    }
};

#endif

