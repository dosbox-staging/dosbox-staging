/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2021 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2004, 2010 Dag Lem <resid@nimrod.no>
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

#ifndef INTEGRATOR6581_H
#define INTEGRATOR6581_H

#include <stdint.h>
#include <cassert>
#ifdef SLOPE_FACTOR
#  include <cmath>
#endif

#include "siddefs-fp.h"

namespace reSIDfp
{

/**
 * Find output voltage in inverting integrator SID op-amp circuits, using a
 * single fixpoint iteration step.
 *
 * A circuit diagram of a MOS 6581 integrator is shown below.
 *
 *                   +---C---+
 *                   |       |
 *     vi --o--Rw--o-o--[A>--o-- vo
 *          |      | vx
 *          +--Rs--+
 *
 * From Kirchoff's current law it follows that
 *
 *     IRw + IRs + ICr = 0
 *
 * Using the formula for current through a capacitor, i = C*dv/dt, we get
 *
 *     IRw + IRs + C*(vc - vc0)/dt = 0
 *     dt/C*(IRw + IRs) + vc - vc0 = 0
 *     vc = vc0 - n*(IRw(vi,vx) + IRs(vi,vx))
 *
 * which may be rewritten as the following iterative fixpoint function:
 *
 *     vc = vc0 - n*(IRw(vi,g(vc)) + IRs(vi,g(vc)))
 *
 * To accurately calculate the currents through Rs and Rw, we need to use
 * transistor models. Rs has a gate voltage of Vdd = 12V, and can be
 * assumed to always be in triode mode. For Rw, the situation is rather
 * more complex, as it turns out that this transistor will operate in
 * both subthreshold, triode, and saturation modes.
 *
 * The Shichman-Hodges transistor model routinely used in textbooks may
 * be written as follows:
 *
 *     Ids = 0                          , Vgst < 0               (subthreshold mode)
 *     Ids = K*W/L*(2*Vgst - Vds)*Vds   , Vgst >= 0, Vds < Vgst  (triode mode)
 *     Ids = K*W/L*Vgst^2               , Vgst >= 0, Vds >= Vgst (saturation mode)
 *
 * where
 *     K   = u*Cox/2 (transconductance coefficient)
 *     W/L = ratio between substrate width and length
 *     Vgst = Vg - Vs - Vt (overdrive voltage)
 *
 * This transistor model is also called the quadratic model.
 *
 * Note that the equation for the triode mode can be reformulated as
 * independent terms depending on Vgs and Vgd, respectively, by the
 * following substitution:
 *
 *     Vds = Vgst - (Vgst - Vds) = Vgst - Vgdt
 *
 *     Ids = K*W/L*(2*Vgst - Vds)*Vds
 *         = K*W/L*(2*Vgst - (Vgst - Vgdt)*(Vgst - Vgdt)
 *         = K*W/L*(Vgst + Vgdt)*(Vgst - Vgdt)
 *         = K*W/L*(Vgst^2 - Vgdt^2)
 *
 * This turns out to be a general equation which covers both the triode
 * and saturation modes (where the second term is 0 in saturation mode).
 * The equation is also symmetrical, i.e. it can calculate negative
 * currents without any change of parameters (since the terms for drain
 * and source are identical except for the sign).
 *
 * FIXME: Subthreshold as function of Vgs, Vgd.
 *
 *     Ids = I0*W/L*e^(Vgst/(Ut/k))   , Vgst < 0               (subthreshold mode)
 *
 * where
 *     I0 = (2 * uCox * Ut^2) / k
 *
 * The remaining problem with the textbook model is that the transition
 * from subthreshold the triode/saturation is not continuous.
 *
 * Realizing that the subthreshold and triode/saturation modes may both
 * be defined by independent (and equal) terms of Vgs and Vds,
 * respectively, the corresponding terms can be blended into (equal)
 * continuous functions suitable for table lookup.
 *
 * The EKV model (Enz, Krummenacher and Vittoz) essentially performs this
 * blending using an elegant mathematical formulation:
 *
 *     Ids = Is * (if - ir)
 *     Is = ((2 * u*Cox * Ut^2)/k) * W/L
 *     if = ln^2(1 + e^((k*(Vg - Vt) - Vs)/(2*Ut))
 *     ir = ln^2(1 + e^((k*(Vg - Vt) - Vd)/(2*Ut))
 *
 * For our purposes, the EKV model preserves two important properties
 * discussed above:
 *
 * - It consists of two independent terms, which can be represented by
 *   the same lookup table.
 * - It is symmetrical, i.e. it calculates current in both directions,
 *   facilitating a branch-free implementation.
 *
 * Rw in the circuit diagram above is a VCR (voltage controlled resistor),
 * as shown in the circuit diagram below.
 *
 *
 *                        Vdd
 *                           |
 *              Vdd         _|_
 *                 |    +---+ +---- Vw
 *                _|_   |
 *             +--+ +---o Vg
 *             |      __|__
 *             |      -----  Rw
 *             |      |   |
 *     vi -----o------+   +-------- vo
 *
 *
 * In order to calculalate the current through the VCR, its gate voltage
 * must be determined.
 *
 * Assuming triode mode and applying Kirchoff's current law, we get the
 * following equation for Vg:
 *
 *     u*Cox/2*W/L*((Vddt - Vg)^2 - (Vddt - vi)^2 + (Vddt - Vg)^2 - (Vddt - Vw)^2) = 0
 *     2*(Vddt - Vg)^2 - (Vddt - vi)^2 - (Vddt - Vw)^2 = 0
 *     (Vddt - Vg) = sqrt(((Vddt - vi)^2 + (Vddt - Vw)^2)/2)
 *
 *     Vg = Vddt - sqrt(((Vddt - vi)^2 + (Vddt - Vw)^2)/2)
 */
class Integrator6581
{
private:
    const unsigned short* vcr_Vg;
    const unsigned short* vcr_n_Ids_term;
    const unsigned short* opamp_rev;

    unsigned int Vddt_Vw_2;
    mutable int vx;
    mutable int vc;
#ifdef SLOPE_FACTOR
    // Slope factor n = 1/k
    // where k is the gate coupling coefficient
    // k = Cox/(Cox+Cdep) ~ 0.7
    mutable double n;
#else
    const int n;
#endif
    const double N16;
    const unsigned short Vddt;
    const unsigned short nVt;
    const unsigned short nVmin;
    const unsigned short n_snake;

public:
    Integrator6581(const unsigned short* vcr_Vg, const unsigned short* vcr_n_Ids_term,
               const unsigned short* opamp_rev, unsigned short Vddt, unsigned short nVt,
               unsigned short nVmin, unsigned short n_snake, double N16) :
        vcr_Vg(vcr_Vg),
        vcr_n_Ids_term(vcr_n_Ids_term),
        opamp_rev(opamp_rev),
        Vddt_Vw_2(0),
        vx(0),
        vc(0),
#ifdef SLOPE_FACTOR
        n(1.4),
#else
        n(1),
#endif
        N16(N16),
        Vddt(Vddt),
        nVt(nVt),
        nVmin(nVmin),
        n_snake(n_snake) {}

    void setVw(unsigned short Vw) { Vddt_Vw_2 = ((Vddt - Vw) * (Vddt - Vw)) >> 1; }

    int solve(int vi) const;
};

} // namespace reSIDfp

#if RESID_INLINING || defined(INTEGRATOR_CPP)

namespace reSIDfp
{

RESID_INLINE
int Integrator6581::solve(int vi) const
{
    // Make sure Vgst>0 so we're not in subthreshold mode
    assert(vx < Vddt);

    // Check that transistor is actually in triode mode
    // Vds < Vgs - Vth
    assert(vi < Vddt);

    // "Snake" voltages for triode mode calculation.
    const unsigned int Vgst = Vddt - vx;
    const unsigned int Vgdt = Vddt - vi;

    const unsigned int Vgst_2 = Vgst * Vgst;
    const unsigned int Vgdt_2 = Vgdt * Vgdt;

    // "Snake" current, scaled by (1/m)*2^13*m*2^16*m*2^16*2^-15 = m*2^30
    const int n_I_snake = n_snake * (static_cast<int>(Vgst_2 - Vgdt_2) >> 15);

    // VCR gate voltage.       // Scaled by m*2^16
    // Vg = Vddt - sqrt(((Vddt - Vw)^2 + Vgdt^2)/2)
    const int Vg = static_cast<int>(vcr_Vg[(Vddt_Vw_2 + (Vgdt_2 >> 1)) >> 16]);
    const int Vp = (Vg - nVt) / n; // Pinch-off voltage
    const int kVg = static_cast<int>(Vp) - nVmin;

    // VCR voltages for EKV model table lookup.
    const int Vgs = (vx < kVg) ? kVg - vx : 0;
    assert(Vgs < (1 << 16));
    const int Vgd = (vi < kVg) ? kVg - vi : 0;
    assert(Vgd < (1 << 16));

    // VCR current, scaled by m*2^15*2^15 = m*2^30
    const unsigned int If = static_cast<unsigned int>(vcr_n_Ids_term[Vgs]) << 15;
    const unsigned int Ir = static_cast<unsigned int>(vcr_n_Ids_term[Vgd]) << 15;
    const int n_I_vcr = (If - Ir) * n;

#ifdef SLOPE_FACTOR
    // estimate new slope factor based on gate voltage
    const double gamma = 1.0;   // body effect factor
    const double phi = 0.8;     // bulk Fermi potential
    const double Ut = 26.0e-3;  // Thermal voltage
    const double nVp = Vp / N16;
    n = 1. + (gamma / (2 * sqrt(nVp + phi + 4*Ut)));
    assert((n > 1.2) && (n < 1.8));
#endif

    // Change in capacitor charge.
    vc += n_I_snake + n_I_vcr;

    // vx = g(vc)
    const int tmp = (vc >> 15) + (1 << 15);
    assert(tmp < (1 << 16));
    vx = opamp_rev[tmp];

    // Return vo.
    return vx - (vc >> 14);
}

} // namespace reSIDfp

#endif

#endif
