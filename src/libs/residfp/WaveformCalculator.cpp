/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2022 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "WaveformCalculator.h"

#include <cmath>

namespace reSIDfp
{

WaveformCalculator* WaveformCalculator::getInstance()
{
    static WaveformCalculator instance;
    return &instance;
}

/**
 * Parameters derived with the Monte Carlo method based on
 * samplings by kevtris. Code and data available in the project repository [1].
 *
 * The score here reported is the acoustic error
 * calculated XORing the estimated and the sampled values.
 * In parentheses the number of mispredicted bits
 * on a total of 32768.
 *
 * [1] https://github.com/libsidplayfp/combined-waveforms
 */
const CombinedWaveformConfig config[2][4] =
{
    { /* kevtris chip G (6581 R2) */
        {0.90522f,  0.f,      0.f,       1.97506f,   1.66937f, 0.63482f }, // error  1687 (278)
        {0.93088f,  2.4843f,  0.f,       1.0353f,    1.1484f,  0.f      }, // error  6128 (130)
        {0.912142f, 2.32076f, 1.106015f, 0.053906f,  0.25143f, 0.f      }, // error 10567 (567)
        {0.901f,    1.0845f,  0.f,       1.056f,     1.1848f,  0.599f   }, // error    36 (12)
    },
    { /* kevtris chip V (8580 R5) */
        {0.94344f,  0.f,      0.976f,    1.6347f,    2.51537f, 0.73115f }, // error  1300 (184)
        {0.93303f,  1.7025f,  0.f,       1.0868f,    1.43527f, 0.f      }, // error  7981 (204)
        {0.95831f,  1.95269f, 0.992986f, 0.0077384f, 0.18408f, 0.f      }, // error  9596 (324)
        {0.94699f,  1.09668f, 0.99586f,  0.94167f,   2.0139f,  0.5633f  }, // error  2118 (54)
    },
};

typedef float (*distance_t)(float, int);

// Distance functions
static float exponentialDistance(float distance, int i)
{
    return powf(distance, static_cast<float>(-i));
}

static float linearDistance(float distance, int i)
{
    return 1.f / (1.f + i * distance);
}

static float quadraticDistance(float distance, int i)
{
    return 1.f / (1.f + (i*i) * distance);
}

/**
 * Generate bitstate based on emulation of combined waves.
 *
 * @param config model parameters matrix
 * @param waveform the waveform to emulate, 1 .. 7
 * @param accumulator the high bits of the accumulator value
 */
short calculateCombinedWaveform(const CombinedWaveformConfig& config, int waveform, int accumulator, bool is8580)
{
    float o[12];

    // Saw
    for (unsigned int i = 0; i < 12; i++)
    {
        o[i] = (accumulator & (1 << i)) != 0 ? 1.f : 0.f;
    }

    // If Saw is not selected the bits are XORed
    if ((waveform & 2) == 0)
    {
        const bool top = (accumulator & 0x800) != 0;

        for (int i = 11; i > 0; i--)
        {
            o[i] = top ? 1.0f - o[i - 1] : o[i - 1];
        }

        o[0] = 0.f;
    }

    // If both Saw and Triangle are selected the bits are interconnected
    else if ((waveform & 3) == 3)
    {
        // bottom bit is grounded via T waveform selector
        o[0] *= config.stmix;

        for (int i = 1; i < 12; i++)
        {
            /*
             * Enabling the S waveform pulls the XOR circuit selector transistor down
             * (which would normally make the descending ramp of the triangle waveform),
             * so ST does not actually have a sawtooth and triangle waveform combined,
             * but merely combines two sawtooths, one rising double the speed the other.
             *
             * http://www.lemon64.com/forum/viewtopic.php?t=25442&postdays=0&postorder=asc&start=165
             */
            o[i] = o[i - 1] * (1.f - config.stmix) + o[i] * config.stmix;
        }
    }

    // topbit for Saw
    if ((waveform & 2) == 2)
    {
        o[11] *= config.topbit;
    }

    // ST, P* waveforms

    const distance_t distFunc =
        (waveform & 1) == 1 ? exponentialDistance : is8580 ? quadraticDistance : linearDistance;

    float distancetable[12 * 2 + 1];
    distancetable[12] = 1.f;
    for (int i = 12; i > 0; i--)
    {
        distancetable[12-i] = distFunc(config.distance1, i);
        distancetable[12+i] = distFunc(config.distance2, i);
    }

    float tmp[12];

    for (int i = 0; i < 12; i++)
    {
        float avg = 0.f;
        float n = 0.f;

        for (int j = 0; j < 12; j++)
        {
            const float weight = distancetable[i - j + 12];
            avg += o[j] * weight;
            n += weight;
        }

        // pulse control bit
        if (waveform > 4)
        {
            const float weight = distancetable[i - 12 + 12];
            avg += config.pulsestrength * weight;
            n += weight;
        }

        tmp[i] = (o[i] + avg / n) * 0.5f;
    }

    for (int i = 0; i < 12; i++)
    {
        o[i] = tmp[i];
    }

    // Get the predicted value
    short value = 0;

    for (unsigned int i = 0; i < 12; i++)
    {
        if (o[i] > config.bias)
        {
            value |= 1 << i;
        }
    }

    return value;
}

matrix_t* WaveformCalculator::buildTable(ChipModel model)
{
    const bool is8580 = model != MOS6581;

    const CombinedWaveformConfig* cfgArray = config[is8580 ? 1 : 0];

    cw_cache_t::iterator lb = CACHE.lower_bound(cfgArray);

    if (lb != CACHE.end() && !(CACHE.key_comp()(cfgArray, lb->first)))
    {
        return &(lb->second);
    }

    matrix_t wftable(8, 4096);

    for (unsigned int idx = 0; idx < 1 << 12; idx++)
    {
        wftable[0][idx] = 0xfff;
        wftable[1][idx] = static_cast<short>((idx & 0x800) == 0 ? idx << 1 : (idx ^ 0xfff) << 1);
        wftable[2][idx] = static_cast<short>(idx);
        wftable[3][idx] = calculateCombinedWaveform(cfgArray[0], 3, idx, is8580);
        wftable[4][idx] = 0xfff;
        wftable[5][idx] = calculateCombinedWaveform(cfgArray[1], 5, idx, is8580);
        wftable[6][idx] = calculateCombinedWaveform(cfgArray[2], 6, idx, is8580);
        wftable[7][idx] = calculateCombinedWaveform(cfgArray[3], 7, idx, is8580);
    }
#ifdef HAVE_CXX11
    return &(CACHE.emplace_hint(lb, cw_cache_t::value_type(cfgArray, wftable))->second);
#else
    return &(CACHE.insert(lb, cw_cache_t::value_type(cfgArray, wftable))->second);
#endif
}

} // namespace reSIDfp
