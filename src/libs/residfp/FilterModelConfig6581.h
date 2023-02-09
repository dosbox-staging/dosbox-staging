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

#include "FilterModelConfig.h"

#include <memory>

#include "Dac.h"

namespace reSIDfp
{

class Integrator6581;

/**
 * Calculate parameters for 6581 filter emulation.
 */
class FilterModelConfig6581 final : public FilterModelConfig
{
private:
    static const unsigned int DAC_BITS = 11;

private:
    static std::unique_ptr<FilterModelConfig6581> instance;
    // This allows access to the private constructor
    friend std::unique_ptr<FilterModelConfig6581>::deleter_type;

    /// Transistor parameters.
    //@{
    const double WL_vcr = 0;        ///< W/L for VCR
    const double WL_snake = 0;      ///< W/L for "snake"
    //@}

    /// DAC parameters.
    //@{
    const double dac_zero = 0;
    const double dac_scale = 0;
    //@}

    /// DAC lookup table
    Dac dac;

    /// VCR - 6581 only.
    //@{
    unsigned short vcr_nVg[1 << 16] = {};
    unsigned short vcr_n_Ids_term[1 << 16] = {};
    //@}

private:
    double getDacZero(double adjustment) const { return dac_zero + (1. - adjustment); }

    FilterModelConfig6581();
    ~FilterModelConfig6581() = default;
    FilterModelConfig6581(const FilterModelConfig6581&) = delete; // prevent copy
    FilterModelConfig6581 &operator=(const FilterModelConfig6581&) = delete; // prevent assignment

public:
    static FilterModelConfig6581* getInstance();

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

    inline unsigned short getVcr_nVg(int i) const { return vcr_nVg[i]; }
    inline unsigned short getVcr_n_Ids_term(int i) const { return vcr_n_Ids_term[i]; }
    // only used if SLOPE_FACTOR is defined
    inline double getUt() const { return Ut; }
    inline double getN16() const { return N16; }
};

} // namespace reSIDfp

#endif
