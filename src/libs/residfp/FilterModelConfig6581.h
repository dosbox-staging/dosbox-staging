/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2020 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2004,2010 Dag Lem
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

#ifndef FILTERMODELCONFIG6581_H
#define FILTERMODELCONFIG6581_H

#include <memory>

#include "Dac.h"
#include "Spline.h"

#include "sidcxx11.h"

namespace reSIDfp
{

class Integrator6581;

/**
 * Calculate parameters for 6581 filter emulation.
 */
class FilterModelConfig6581
{
private:
    static const unsigned int DAC_BITS = 11;

private:
    static std::unique_ptr<FilterModelConfig6581> instance;
    // This allows access to the private constructor
#ifdef HAVE_CXX11
    friend std::unique_ptr<FilterModelConfig6581>::deleter_type;
#else
    friend class std::auto_ptr<FilterModelConfig6581>;
#endif

    const double voice_voltage_range;
    const double voice_DC_voltage;

    /// Capacitor value.
    const double C;

    /// Transistor parameters.
    //@{
    const double Vdd;
    const double Vth;           ///< Threshold voltage
    const double Ut;            ///< Thermal voltage: Ut = kT/q = 8.61734315e-5*T ~ 26mV
    const double uCox;          ///< Transconductance coefficient: u*Cox
    const double WL_vcr;        ///< W/L for VCR
    const double WL_snake;      ///< W/L for "snake"
    const double Vddt;          ///< Vdd - Vth
    //@}

    /// DAC parameters.
    //@{
    const double dac_zero;
    const double dac_scale;
    //@}

    // Derived stuff
    const double vmin, vmax;
    const double denorm, norm;

    /// Fixed point scaling for 16 bit op-amp output.
    const double N16;

    /// Lookup tables for gain and summer op-amps in output stage / filter.
    //@{
    unsigned short* mixer[8];
    unsigned short* summer[5];
    unsigned short* gain[16];
    //@}

    /// DAC lookup table
    Dac dac;

    /// VCR - 6581 only.
    //@{
    unsigned short vcr_Vg[1 << 16];
    unsigned short vcr_n_Ids_term[1 << 16];
    //@}

    /// Reverse op-amp transfer function.
    unsigned short opamp_rev[1 << 16];

private:
    double getDacZero(double adjustment) const { return dac_zero + (1. - adjustment); }

    FilterModelConfig6581();
    ~FilterModelConfig6581();

public:
    static FilterModelConfig6581* getInstance();

    /**
     * The digital range of one voice is 20 bits; create a scaling term
     * for multiplication which fits in 11 bits.
     */
    int getVoiceScaleS11() const { return static_cast<int>((norm * ((1 << 11) - 1)) * voice_voltage_range); }

    /**
     * The "zero" output level of the voices.
     */
    int getVoiceDC() const { return static_cast<int>(N16 * (voice_DC_voltage - vmin)); }

    unsigned short** getGain() { return gain; }

    unsigned short** getSummer() { return summer; }

    unsigned short** getMixer() { return mixer; }

    /**
     * Construct an 11 bit cutoff frequency DAC output voltage table.
     * Ownership is transferred to the requester which becomes responsible
     * of freeing the object when done.
     *
     * @param adjustment
     * @return the DAC table
     */
    unsigned short* getDAC(double adjustment) const;

    /**
     * Construct an integrator solver.
     *
     * @return the integrator
     */
    std::unique_ptr<Integrator6581> buildIntegrator();
};

} // namespace reSIDfp

#endif
