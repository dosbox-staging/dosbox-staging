/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2016 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2004 Dag Lem <resid@nimrod.no>
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

#define SID_CPP

#include "SID.h"

#include <limits>

#include "array.h"
#include "Filter6581.h"
#include "Filter8580.h"
#include "Potentiometer.h"
#include "WaveformCalculator.h"
#include "resample/TwoPassSincResampler.h"
#include "resample/ZeroOrderResampler.h"

namespace reSIDfp
{

/**
 * Bus value stays alive for some time after each operation.
 * Values differs between chip models, the timings used here
 * are taken from VICE [1].
 * See also the discussion "How do I reliably detect 6581/8580 sid?" on CSDb [2].
 *
 *   Results from real C64 (testprogs/SID/bitfade/delayfrq0.prg):
 *
 *   (new SID) (250469/8580R5) (250469/8580R5)
 *   delayfrq0    ~7a000        ~108000
 *
 *   (old SID) (250407/6581)
 *   delayfrq0    ~01d00
 *
 * [1]: http://sourceforge.net/p/vice-emu/patches/99/
 * [2]: http://noname.c64.org/csdb/forums/?roomid=11&topicid=29025&showallposts=1
 */
 //@{
int constexpr BUS_TTL_6581 = 0x01d00;
int constexpr BUS_TTL_8580 = 0xa2000;
//@}

SID::SID() :
    filter6581(new Filter6581()),
    filter8580(new Filter8580()),
    externalFilter(new ExternalFilter()),
    resampler(nullptr),
    potX(new Potentiometer()),
    potY(new Potentiometer())
{
    voice[0].reset(new Voice());
    voice[1].reset(new Voice());
    voice[2].reset(new Voice());

    muted[0] = muted[1] = muted[2] = false;

    reset();
    setChipModel(MOS8580);
}

SID::~SID()
{
    // Needed to delete auto_ptr with complete type
}

void SID::setFilter6581Curve(double filterCurve)
{
    filter6581->setFilterCurve(filterCurve);
}

void SID::setFilter8580Curve(double filterCurve)
{
    filter8580->setFilterCurve(filterCurve);
}

void SID::enableFilter(bool enable)
{
    filter6581->enable(enable);
    filter8580->enable(enable);
}

void SID::voiceSync(bool sync)
{
    if (sync)
    {
        // Synchronize the 3 waveform generators.
        for (int i = 0; i < 3; i++)
        {
            voice[i]->wave()->synchronize(voice[(i + 1) % 3]->wave(), voice[(i + 2) % 3]->wave());
        }
    }

    // Calculate the time to next voice sync
    nextVoiceSync = std::numeric_limits<int>::max();

    for (int i = 0; i < 3; i++)
    {
        const unsigned int freq = voice[i]->wave()->readFreq();

        if (voice[i]->wave()->readTest() || freq == 0 || !voice[(i + 1) % 3]->wave()->readSync())
        {
            continue;
        }

        const unsigned int accumulator = voice[i]->wave()->readAccumulator();
        const unsigned int thisVoiceSync = ((0x7fffff - accumulator) & 0xffffff) / freq + 1;

        if (thisVoiceSync < nextVoiceSync)
        {
            nextVoiceSync = thisVoiceSync;
        }
    }
}

void SID::setChipModel(ChipModel model)
{
    switch (model)
    {
    case MOS6581:
        filter = filter6581.get();
        modelTTL = BUS_TTL_6581;
        break;

    case MOS8580:
        filter = filter8580.get();
        modelTTL = BUS_TTL_8580;
        break;

    default:
        throw SIDError("Unknown chip type");
    }

    this->model = model;

    // calculate waveform-related tables, feed them to the generator
    matrix_t* tables = WaveformCalculator::getInstance()->buildTable(model);

    // update voice offsets
    for (int i = 0; i < 3; i++)
    {
        voice[i]->envelope()->setChipModel(model);
        voice[i]->wave()->setChipModel(model);
        voice[i]->wave()->setWaveformModels(tables);
    }
}

void SID::reset()
{
    for (int i = 0; i < 3; i++)
    {
        voice[i]->reset();
    }

    filter6581->reset();
    filter8580->reset();
    externalFilter->reset();

    if (resampler.get())
    {
        resampler->reset();
    }

    busValue = 0;
    busValueTtl = 0;
    voiceSync(false);
}

void SID::input(int value)
{
    filter6581->input(value);
    filter8580->input(value);
}

unsigned char SID::read(int offset)
{
    switch (offset)
    {
    case 0x19: // X value of paddle
        busValue = potX->readPOT();
        busValueTtl = modelTTL;
        break;

    case 0x1a: // Y value of paddle
        busValue = potY->readPOT();
        busValueTtl = modelTTL;
        break;

    case 0x1b: // Voice #3 waveform output
        busValue = voice[2]->wave()->readOSC();
        busValueTtl = modelTTL;
        break;

    case 0x1c: // Voice #3 ADSR output
        busValue = voice[2]->envelope()->readENV();
        busValueTtl = modelTTL;
        break;

    default:
        // Reading from a write-only or non-existing register
        // makes the bus discharge faster.
        // Emulate this by halving the residual TTL.
        busValueTtl /= 2;
        break;
    }

    return busValue;
}

void SID::write(int offset, unsigned char value)
{
    busValue = value;
    busValueTtl = modelTTL;

    switch (offset)
    {
    case 0x00: // Voice #1 frequency (Low-byte)
        voice[0]->wave()->writeFREQ_LO(value);
        break;

    case 0x01: // Voice #1 frequency (High-byte)
        voice[0]->wave()->writeFREQ_HI(value);
        break;

    case 0x02: // Voice #1 pulse width (Low-byte)
        voice[0]->wave()->writePW_LO(value);
        break;

    case 0x03: // Voice #1 pulse width (bits #8-#15)
        voice[0]->wave()->writePW_HI(value);
        break;

    case 0x04: // Voice #1 control register
        voice[0]->writeCONTROL_REG(muted[0] ? 0 : value);
        break;

    case 0x05: // Voice #1 Attack and Decay length
        voice[0]->envelope()->writeATTACK_DECAY(value);
        break;

    case 0x06: // Voice #1 Sustain volume and Release length
        voice[0]->envelope()->writeSUSTAIN_RELEASE(value);
        break;

    case 0x07: // Voice #2 frequency (Low-byte)
        voice[1]->wave()->writeFREQ_LO(value);
        break;

    case 0x08: // Voice #2 frequency (High-byte)
        voice[1]->wave()->writeFREQ_HI(value);
        break;

    case 0x09: // Voice #2 pulse width (Low-byte)
        voice[1]->wave()->writePW_LO(value);
        break;

    case 0x0a: // Voice #2 pulse width (bits #8-#15)
        voice[1]->wave()->writePW_HI(value);
        break;

    case 0x0b: // Voice #2 control register
        voice[1]->writeCONTROL_REG(muted[1] ? 0 : value);
        break;

    case 0x0c: // Voice #2 Attack and Decay length
        voice[1]->envelope()->writeATTACK_DECAY(value);
        break;

    case 0x0d: // Voice #2 Sustain volume and Release length
        voice[1]->envelope()->writeSUSTAIN_RELEASE(value);
        break;

    case 0x0e: // Voice #3 frequency (Low-byte)
        voice[2]->wave()->writeFREQ_LO(value);
        break;

    case 0x0f: // Voice #3 frequency (High-byte)
        voice[2]->wave()->writeFREQ_HI(value);
        break;

    case 0x10: // Voice #3 pulse width (Low-byte)
        voice[2]->wave()->writePW_LO(value);
        break;

    case 0x11: // Voice #3 pulse width (bits #8-#15)
        voice[2]->wave()->writePW_HI(value);
        break;

    case 0x12: // Voice #3 control register
        voice[2]->writeCONTROL_REG(muted[2] ? 0 : value);
        break;

    case 0x13: // Voice #3 Attack and Decay length
        voice[2]->envelope()->writeATTACK_DECAY(value);
        break;

    case 0x14: // Voice #3 Sustain volume and Release length
        voice[2]->envelope()->writeSUSTAIN_RELEASE(value);
        break;

    case 0x15: // Filter cut off frequency (bits #0-#2)
        filter6581->writeFC_LO(value);
        filter8580->writeFC_LO(value);
        break;

    case 0x16: // Filter cut off frequency (bits #3-#10)
        filter6581->writeFC_HI(value);
        filter8580->writeFC_HI(value);
        break;

    case 0x17: // Filter control
        filter6581->writeRES_FILT(value);
        filter8580->writeRES_FILT(value);
        break;

    case 0x18: // Volume and filter modes
        filter6581->writeMODE_VOL(value);
        filter8580->writeMODE_VOL(value);
        break;

    default:
        break;
    }

    // Update voicesync just in case.
    voiceSync(false);
}

void SID::setSamplingParameters(double clockFrequency, SamplingMethod method, double samplingFrequency, double highestAccurateFrequency)
{
    externalFilter->setClockFrequency(clockFrequency);

    switch (method)
    {
    case DECIMATE:
        resampler.reset(new ZeroOrderResampler(clockFrequency, samplingFrequency));
        break;

    case RESAMPLE:
        resampler.reset(TwoPassSincResampler::create(clockFrequency, samplingFrequency, highestAccurateFrequency));
        break;

    default:
        throw SIDError("Unknown sampling method");
    }
}

void SID::clockSilent(unsigned int cycles)
{
    ageBusValue(cycles);

    while (cycles != 0)
    {
        int delta_t = std::min(nextVoiceSync, cycles);

        if (delta_t > 0)
        {
            for (int i = 0; i < delta_t; i++)
            {
                // clock waveform generators (can affect OSC3)
                voice[0]->wave()->clock();
                voice[1]->wave()->clock();
                voice[2]->wave()->clock();

                voice[0]->wave()->output(voice[2]->wave());
                voice[1]->wave()->output(voice[0]->wave());
                voice[2]->wave()->output(voice[1]->wave());

                // clock ENV3 only
                voice[2]->envelope()->clock();
            }

            cycles -= delta_t;
            nextVoiceSync -= delta_t;
        }

        if (nextVoiceSync == 0)
        {
            voiceSync(true);
        }
    }
}

} // namespace reSIDfp
