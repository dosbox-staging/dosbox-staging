/*
BSD 2-Clause License

Copyright (c) 2020, Andrea Zoppi
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "YM7128B_emu.h"

// ============================================================================

char const* YM7128B_GetVersion(void)
{
    return YM7128B_VERSION;
}

// ============================================================================

signed char const YM7128B_GainDecibel_Table[YM7128B_Gain_Data_Count / 2] =
{
    -128,  //  0 = -oo
    - 60,  //  1
    - 58,  //  2
    - 56,  //  3
    - 54,  //  4
    - 52,  //  5
    - 50,  //  6
    - 48,  //  7
    - 46,  //  8
    - 44,  //  9
    - 42,  // 10
    - 40,  // 11
    - 38,  // 12
    - 36,  // 13
    - 34,  // 14
    - 32,  // 15
    - 30,  // 16
    - 28,  // 17
    - 26,  // 18
    - 24,  // 19
    - 22,  // 20
    - 20,  // 21
    - 18,  // 22
    - 16,  // 23
    - 14,  // 24
    - 12,  // 25
    - 10,  // 26
    -  8,  // 27
    -  6,  // 28
    -  4,  // 29
    -  2,  // 30
       0   // 31
};

// ----------------------------------------------------------------------------

#ifdef GAIN
#undef GAIN
#endif

#define GAIN(real) \
    ((YM7128B_Fixed)((real) * YM7128B_Gain_Max) & (YM7128B_Fixed)YM7128B_Gain_Mask)

YM7128B_Fixed const YM7128B_GainFixed_Table[YM7128B_Gain_Data_Count] =
{
    // Pseudo-negative gains
    ~GAIN(0.000000000000000000),  // -oo dB-
    ~GAIN(0.001000000000000000),  // -60 dB-
    ~GAIN(0.001258925411794167),  // -58 dB-
    ~GAIN(0.001584893192461114),  // -56 dB-
    ~GAIN(0.001995262314968879),  // -54 dB-
    ~GAIN(0.002511886431509579),  // -52 dB-
    ~GAIN(0.003162277660168379),  // -50 dB-
    ~GAIN(0.003981071705534973),  // -48 dB-
    ~GAIN(0.005011872336272725),  // -46 dB-
    ~GAIN(0.006309573444801930),  // -44 dB-
    ~GAIN(0.007943282347242814),  // -42 dB-
    ~GAIN(0.010000000000000000),  // -40 dB-
    ~GAIN(0.012589254117941675),  // -38 dB-
    ~GAIN(0.015848931924611134),  // -36 dB-
    ~GAIN(0.019952623149688799),  // -34 dB-
    ~GAIN(0.025118864315095794),  // -32 dB-
    ~GAIN(0.031622776601683791),  // -30 dB-
    ~GAIN(0.039810717055349734),  // -28 dB-
    ~GAIN(0.050118723362727220),  // -26 dB-
    ~GAIN(0.063095734448019331),  // -24 dB-
    ~GAIN(0.079432823472428138),  // -22 dB-
    ~GAIN(0.100000000000000006),  // -20 dB-
    ~GAIN(0.125892541179416728),  // -18 dB-
    ~GAIN(0.158489319246111343),  // -16 dB-
    ~GAIN(0.199526231496887974),  // -14 dB-
    ~GAIN(0.251188643150958013),  // -12 dB-
    ~GAIN(0.316227766016837941),  // -10 dB-
    ~GAIN(0.398107170553497203),  // - 8 dB-
    ~GAIN(0.501187233627272244),  // - 6 dB-
    ~GAIN(0.630957344480193250),  // - 4 dB-
    ~GAIN(0.794328234724281490),  // - 2 dB-
    ~GAIN(1.000000000000000000),  // - 0 dB-

    // Positive gains
    +GAIN(0.000000000000000000),  // -oo dB+
    +GAIN(0.001000000000000000),  // -60 dB+
    +GAIN(0.001258925411794167),  // -58 dB+
    +GAIN(0.001584893192461114),  // -56 dB+
    +GAIN(0.001995262314968879),  // -54 dB+
    +GAIN(0.002511886431509579),  // -52 dB+
    +GAIN(0.003162277660168379),  // -50 dB+
    +GAIN(0.003981071705534973),  // -48 dB+
    +GAIN(0.005011872336272725),  // -46 dB+
    +GAIN(0.006309573444801930),  // -44 dB+
    +GAIN(0.007943282347242814),  // -42 dB+
    +GAIN(0.010000000000000000),  // -40 dB+
    +GAIN(0.012589254117941675),  // -38 dB+
    +GAIN(0.015848931924611134),  // -36 dB+
    +GAIN(0.019952623149688799),  // -34 dB+
    +GAIN(0.025118864315095794),  // -32 dB+
    +GAIN(0.031622776601683791),  // -30 dB+
    +GAIN(0.039810717055349734),  // -28 dB+
    +GAIN(0.050118723362727220),  // -26 dB+
    +GAIN(0.063095734448019331),  // -24 dB+
    +GAIN(0.079432823472428138),  // -22 dB+
    +GAIN(0.100000000000000006),  // -20 dB+
    +GAIN(0.125892541179416728),  // -18 dB+
    +GAIN(0.158489319246111343),  // -16 dB+
    +GAIN(0.199526231496887974),  // -14 dB+
    +GAIN(0.251188643150958013),  // -12 dB+
    +GAIN(0.316227766016837941),  // -10 dB+
    +GAIN(0.398107170553497203),  // - 8 dB+
    +GAIN(0.501187233627272244),  // - 6 dB+
    +GAIN(0.630957344480193250),  // - 4 dB+
    +GAIN(0.794328234724281490),  // - 2 dB+
    +GAIN(1.000000000000000000)   // - 0 dB+
};

// ----------------------------------------------------------------------------

#ifdef GAIN
#undef GAIN
#endif

#define GAIN(real) \
    ((YM7128B_Float)(real))

YM7128B_Float const YM7128B_GainFloat_Table[YM7128B_Gain_Data_Count] =
{
    // Negative gains
    -GAIN(0.000000000000000000),  // -oo dB-
    -GAIN(0.001000000000000000),  // -60 dB-
    -GAIN(0.001258925411794167),  // -58 dB-
    -GAIN(0.001584893192461114),  // -56 dB-
    -GAIN(0.001995262314968879),  // -54 dB-
    -GAIN(0.002511886431509579),  // -52 dB-
    -GAIN(0.003162277660168379),  // -50 dB-
    -GAIN(0.003981071705534973),  // -48 dB-
    -GAIN(0.005011872336272725),  // -46 dB-
    -GAIN(0.006309573444801930),  // -44 dB-
    -GAIN(0.007943282347242814),  // -42 dB-
    -GAIN(0.010000000000000000),  // -40 dB-
    -GAIN(0.012589254117941675),  // -38 dB-
    -GAIN(0.015848931924611134),  // -36 dB-
    -GAIN(0.019952623149688799),  // -34 dB-
    -GAIN(0.025118864315095794),  // -32 dB-
    -GAIN(0.031622776601683791),  // -30 dB-
    -GAIN(0.039810717055349734),  // -28 dB-
    -GAIN(0.050118723362727220),  // -26 dB-
    -GAIN(0.063095734448019331),  // -24 dB-
    -GAIN(0.079432823472428138),  // -22 dB-
    -GAIN(0.100000000000000006),  // -20 dB-
    -GAIN(0.125892541179416728),  // -18 dB-
    -GAIN(0.158489319246111343),  // -16 dB-
    -GAIN(0.199526231496887974),  // -14 dB-
    -GAIN(0.251188643150958013),  // -12 dB-
    -GAIN(0.316227766016837941),  // -10 dB-
    -GAIN(0.398107170553497203),  // - 8 dB-
    -GAIN(0.501187233627272244),  // - 6 dB-
    -GAIN(0.630957344480193250),  // - 4 dB-
    -GAIN(0.794328234724281490),  // - 2 dB-
    -GAIN(1.000000000000000000),  // - 0 dB-

    // Positive gains
    +GAIN(0.000000000000000000),  // -oo dB+
    +GAIN(0.001000000000000000),  // -60 dB+
    +GAIN(0.001258925411794167),  // -58 dB+
    +GAIN(0.001584893192461114),  // -56 dB+
    +GAIN(0.001995262314968879),  // -54 dB+
    +GAIN(0.002511886431509579),  // -52 dB+
    +GAIN(0.003162277660168379),  // -50 dB+
    +GAIN(0.003981071705534973),  // -48 dB+
    +GAIN(0.005011872336272725),  // -46 dB+
    +GAIN(0.006309573444801930),  // -44 dB+
    +GAIN(0.007943282347242814),  // -42 dB+
    +GAIN(0.010000000000000000),  // -40 dB+
    +GAIN(0.012589254117941675),  // -38 dB+
    +GAIN(0.015848931924611134),  // -36 dB+
    +GAIN(0.019952623149688799),  // -34 dB+
    +GAIN(0.025118864315095794),  // -32 dB+
    +GAIN(0.031622776601683791),  // -30 dB+
    +GAIN(0.039810717055349734),  // -28 dB+
    +GAIN(0.050118723362727220),  // -26 dB+
    +GAIN(0.063095734448019331),  // -24 dB+
    +GAIN(0.079432823472428138),  // -22 dB+
    +GAIN(0.100000000000000006),  // -20 dB+
    +GAIN(0.125892541179416728),  // -18 dB+
    +GAIN(0.158489319246111343),  // -16 dB+
    +GAIN(0.199526231496887974),  // -14 dB+
    +GAIN(0.251188643150958013),  // -12 dB+
    +GAIN(0.316227766016837941),  // -10 dB+
    +GAIN(0.398107170553497203),  // - 8 dB+
    +GAIN(0.501187233627272244),  // - 6 dB+
    +GAIN(0.630957344480193250),  // - 4 dB+
    +GAIN(0.794328234724281490),  // - 2 dB+
    +GAIN(1.000000000000000000)   // - 0 dB+
};

// ----------------------------------------------------------------------------

#ifdef GAIN
#undef GAIN
#endif

#define GAIN(real) \
    ((YM7128B_Fixed)((real) * YM7128B_Gain_Max))

YM7128B_Fixed const YM7128B_GainShort_Table[YM7128B_Gain_Data_Count] =
{
    // Negative gains
    -GAIN(0.000000000000000000),  // -oo dB-
    -GAIN(0.001000000000000000),  // -60 dB-
    -GAIN(0.001258925411794167),  // -58 dB-
    -GAIN(0.001584893192461114),  // -56 dB-
    -GAIN(0.001995262314968879),  // -54 dB-
    -GAIN(0.002511886431509579),  // -52 dB-
    -GAIN(0.003162277660168379),  // -50 dB-
    -GAIN(0.003981071705534973),  // -48 dB-
    -GAIN(0.005011872336272725),  // -46 dB-
    -GAIN(0.006309573444801930),  // -44 dB-
    -GAIN(0.007943282347242814),  // -42 dB-
    -GAIN(0.010000000000000000),  // -40 dB-
    -GAIN(0.012589254117941675),  // -38 dB-
    -GAIN(0.015848931924611134),  // -36 dB-
    -GAIN(0.019952623149688799),  // -34 dB-
    -GAIN(0.025118864315095794),  // -32 dB-
    -GAIN(0.031622776601683791),  // -30 dB-
    -GAIN(0.039810717055349734),  // -28 dB-
    -GAIN(0.050118723362727220),  // -26 dB-
    -GAIN(0.063095734448019331),  // -24 dB-
    -GAIN(0.079432823472428138),  // -22 dB-
    -GAIN(0.100000000000000006),  // -20 dB-
    -GAIN(0.125892541179416728),  // -18 dB-
    -GAIN(0.158489319246111343),  // -16 dB-
    -GAIN(0.199526231496887974),  // -14 dB-
    -GAIN(0.251188643150958013),  // -12 dB-
    -GAIN(0.316227766016837941),  // -10 dB-
    -GAIN(0.398107170553497203),  // - 8 dB-
    -GAIN(0.501187233627272244),  // - 6 dB-
    -GAIN(0.630957344480193250),  // - 4 dB-
    -GAIN(0.794328234724281490),  // - 2 dB-
    -GAIN(1.000000000000000000),  // - 0 dB-

    // Positive gains
    +GAIN(0.000000000000000000),  // -oo dB+
    +GAIN(0.001000000000000000),  // -60 dB+
    +GAIN(0.001258925411794167),  // -58 dB+
    +GAIN(0.001584893192461114),  // -56 dB+
    +GAIN(0.001995262314968879),  // -54 dB+
    +GAIN(0.002511886431509579),  // -52 dB+
    +GAIN(0.003162277660168379),  // -50 dB+
    +GAIN(0.003981071705534973),  // -48 dB+
    +GAIN(0.005011872336272725),  // -46 dB+
    +GAIN(0.006309573444801930),  // -44 dB+
    +GAIN(0.007943282347242814),  // -42 dB+
    +GAIN(0.010000000000000000),  // -40 dB+
    +GAIN(0.012589254117941675),  // -38 dB+
    +GAIN(0.015848931924611134),  // -36 dB+
    +GAIN(0.019952623149688799),  // -34 dB+
    +GAIN(0.025118864315095794),  // -32 dB+
    +GAIN(0.031622776601683791),  // -30 dB+
    +GAIN(0.039810717055349734),  // -28 dB+
    +GAIN(0.050118723362727220),  // -26 dB+
    +GAIN(0.063095734448019331),  // -24 dB+
    +GAIN(0.079432823472428138),  // -22 dB+
    +GAIN(0.100000000000000006),  // -20 dB+
    +GAIN(0.125892541179416728),  // -18 dB+
    +GAIN(0.158489319246111343),  // -16 dB+
    +GAIN(0.199526231496887974),  // -14 dB+
    +GAIN(0.251188643150958013),  // -12 dB+
    +GAIN(0.316227766016837941),  // -10 dB+
    +GAIN(0.398107170553497203),  // - 8 dB+
    +GAIN(0.501187233627272244),  // - 6 dB+
    +GAIN(0.630957344480193250),  // - 4 dB+
    +GAIN(0.794328234724281490),  // - 2 dB+
    +GAIN(1.000000000000000000)   // - 0 dB+
};

// ----------------------------------------------------------------------------

#ifdef TAP
#undef TAP
#endif

#define TAP(index) \
    ((YM7128B_Tap)(((index) * (YM7128B_Buffer_Length - 1)) / (YM7128B_Tap_Value_Count - 1)))

YM7128B_Tap const YM7128B_Tap_Table[YM7128B_Tap_Value_Count] =
{
    TAP( 0),  //   0.0 ms
    TAP( 1),  //   3.2 ms
    TAP( 2),  //   6.5 ms
    TAP( 3),  //   9.7 ms
    TAP( 4),  //  12.9 ms
    TAP( 5),  //  16.1 ms
    TAP( 6),  //  19.3 ms
    TAP( 7),  //  22.6 ms
    TAP( 8),  //  25.8 ms
    TAP( 9),  //  29.0 ms
    TAP(10),  //  32.3 ms
    TAP(11),  //  35.5 ms
    TAP(12),  //  38.7 ms
    TAP(13),  //  41.9 ms
    TAP(14),  //  45.2 ms
    TAP(15),  //  48.4 ms
    TAP(16),  //  51.6 ms
    TAP(17),  //  54.9 ms
    TAP(18),  //  58.1 ms
    TAP(19),  //  61.3 ms
    TAP(20),  //  64.5 ms
    TAP(21),  //  67.8 ms
    TAP(22),  //  71.0 ms
    TAP(23),  //  74.2 ms
    TAP(24),  //  77.4 ms
    TAP(25),  //  80.7 ms
    TAP(26),  //  83.9 ms
    TAP(27),  //  87.1 ms
    TAP(28),  //  90.4 ms
    TAP(29),  //  93.6 ms
    TAP(30),  //  96.8 ms
    TAP(31)   // 100.0 ms
};

// ============================================================================

#ifdef KERNEL
#undef KERNEL
#endif

#define KERNEL(real) \
    ((YM7128B_Fixed)((real) * YM7128B_Fixed_Max) & (YM7128B_Fixed)YM7128B_Coeff_Mask)

YM7128B_Fixed const YM7128B_OversamplerFixed_Kernel[YM7128B_Oversampler_Length] =
{
#if YM7128B_USE_MINPHASE
    // minimum phase
    KERNEL(+0.073585247514714749),
    KERNEL(+0.269340051166713890),
    KERNEL(+0.442535202999738531),
    KERNEL(+0.350129745841520346),
    KERNEL(+0.026195691646307945),
    KERNEL(-0.178423532471468610),
    KERNEL(-0.081176763571493171),
    KERNEL(+0.083194010466739091),
    KERNEL(+0.067960765530891545),
    KERNEL(-0.035840063980478287),
    KERNEL(-0.044393769145659796),
    KERNEL(+0.013156688603347873),
    KERNEL(+0.023451305043275420),
    KERNEL(-0.004374029821991059),
    KERNEL(-0.009480786001493536),
    KERNEL(+0.002700502551912207),
    KERNEL(+0.003347671274177581),
    KERNEL(-0.002391896275498628),
    KERNEL(+0.000483958628744376)
#else
    // linear phase
    KERNEL(+0.005969087803865891),
    KERNEL(-0.003826518613910499),
    KERNEL(-0.016623943725986926),
    KERNEL(+0.007053928712894589),
    KERNEL(+0.038895802111020034),
    KERNEL(-0.010501507751597486),
    KERNEL(-0.089238395139830201),
    KERNEL(+0.013171814880420758),
    KERNEL(+0.312314472963171053),
    KERNEL(+0.485820312497107776),
    KERNEL(+0.312314472963171053),
    KERNEL(+0.013171814880420758),
    KERNEL(-0.089238395139830201),
    KERNEL(-0.010501507751597486),
    KERNEL(+0.038895802111020034),
    KERNEL(+0.007053928712894589),
    KERNEL(-0.016623943725986926),
    KERNEL(-0.003826518613910499),
    KERNEL(+0.005969087803865891)
#endif
};

// ----------------------------------------------------------------------------

YM7128B_Fixed YM7128B_OversamplerFixed_Process(
    YM7128B_OversamplerFixed* self,
    YM7128B_Fixed input
)
{
    YM7128B_Accumulator accum = 0;

    for (YM7128B_Oversampler_Index i = 0; i < YM7128B_Oversampler_Length; ++i) {
        YM7128B_Fixed sample = self->buffer_[i];
        self->buffer_[i] = input;
        input = sample;
        YM7128B_Fixed kernel = YM7128B_OversamplerFixed_Kernel[i];
        YM7128B_Fixed oversampled = YM7128B_MulFixed(sample, kernel);
        accum += oversampled;
    }

    YM7128B_Fixed clamped = YM7128B_ClampFixed(accum);
    YM7128B_Fixed output = clamped & (YM7128B_Fixed)YM7128B_Signal_Mask;
    return output;
}

// ============================================================================

#ifdef KERNEL
#undef KERNEL
#endif

#define KERNEL(real) \
    ((YM7128B_Float)(real))

YM7128B_Float const YM7128B_OversamplerFloat_Kernel[YM7128B_Oversampler_Length] =
{
#if YM7128B_USE_MINPHASE
    // minimum phase
    KERNEL(+0.073585247514714749),
    KERNEL(+0.269340051166713890),
    KERNEL(+0.442535202999738531),
    KERNEL(+0.350129745841520346),
    KERNEL(+0.026195691646307945),
    KERNEL(-0.178423532471468610),
    KERNEL(-0.081176763571493171),
    KERNEL(+0.083194010466739091),
    KERNEL(+0.067960765530891545),
    KERNEL(-0.035840063980478287),
    KERNEL(-0.044393769145659796),
    KERNEL(+0.013156688603347873),
    KERNEL(+0.023451305043275420),
    KERNEL(-0.004374029821991059),
    KERNEL(-0.009480786001493536),
    KERNEL(+0.002700502551912207),
    KERNEL(+0.003347671274177581),
    KERNEL(-0.002391896275498628),
    KERNEL(+0.000483958628744376)
#else
    // linear phase
    KERNEL(+0.005969087803865891),
    KERNEL(-0.003826518613910499),
    KERNEL(-0.016623943725986926),
    KERNEL(+0.007053928712894589),
    KERNEL(+0.038895802111020034),
    KERNEL(-0.010501507751597486),
    KERNEL(-0.089238395139830201),
    KERNEL(+0.013171814880420758),
    KERNEL(+0.312314472963171053),
    KERNEL(+0.485820312497107776),
    KERNEL(+0.312314472963171053),
    KERNEL(+0.013171814880420758),
    KERNEL(-0.089238395139830201),
    KERNEL(-0.010501507751597486),
    KERNEL(+0.038895802111020034),
    KERNEL(+0.007053928712894589),
    KERNEL(-0.016623943725986926),
    KERNEL(-0.003826518613910499),
    KERNEL(+0.005969087803865891)
#endif
};

// ----------------------------------------------------------------------------

YM7128B_Float YM7128B_OversamplerFloat_Process(
    YM7128B_OversamplerFloat* self,
    YM7128B_Float input
)
{
    YM7128B_Float accum = 0;

    for (YM7128B_Oversampler_Index i = 0; i < YM7128B_Oversampler_Length; ++i) {
        YM7128B_Float sample = self->buffer_[i];
        self->buffer_[i] = input;
        input = sample;
        YM7128B_Float kernel = YM7128B_OversamplerFloat_Kernel[i];
        YM7128B_Float oversampled = YM7128B_MulFloat(sample, kernel);
        accum += oversampled;
    }

    YM7128B_Float output = YM7128B_ClampFloat(accum);
    return output;
}

// ============================================================================

void YM7128B_ChipFixed_Ctor(YM7128B_ChipFixed* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Dtor(YM7128B_ChipFixed* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Reset(YM7128B_ChipFixed* self)
{
    for (YM7128B_Address i = 0; i <= YM7128B_Address_Max; ++i) {
        self->regs_[i] = 0;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Start(YM7128B_ChipFixed* self)
{
    self->t0_d_ = 0;

    self->tail_ = 0;

    for (YM7128B_Tap i = 0; i < YM7128B_Buffer_Length; ++i) {
        self->buffer_[i] = 0;
    }

    for (YM7128B_Oversampler_Index i = 0; i < YM7128B_OutputChannel_Count; ++i) {
        YM7128B_OversamplerFixed_Reset(&self->oversampler_[i]);
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Stop(YM7128B_ChipFixed* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Process(
    YM7128B_ChipFixed* self,
    YM7128B_ChipFixed_Process_Data* data
)
{
    YM7128B_Fixed input = data->inputs[YM7128B_InputChannel_Mono];
    YM7128B_Fixed sample = input & (YM7128B_Fixed)YM7128B_Signal_Mask;

    YM7128B_Tap t0 = self->tail_ + self->taps_[0];
    YM7128B_Tap filter_head  = (t0 >= YM7128B_Buffer_Length) ? (t0 - YM7128B_Buffer_Length) : t0;
    YM7128B_Fixed filter_t0  = self->buffer_[filter_head];
    YM7128B_Fixed filter_d   = self->t0_d_;
    self->t0_d_ = filter_t0;
    YM7128B_Fixed filter_c0  = YM7128B_MulFixed(filter_t0, self->gains_[YM7128B_Reg_C0]);
    YM7128B_Fixed filter_c1  = YM7128B_MulFixed(filter_d, self->gains_[YM7128B_Reg_C1]);
    YM7128B_Fixed filter_sum = YM7128B_ClampAddFixed(filter_c0, filter_c1);
    YM7128B_Fixed filter_vc  = YM7128B_MulFixed(filter_sum, self->gains_[YM7128B_Reg_VC]);

    YM7128B_Fixed input_vm  = YM7128B_MulFixed(sample, self->gains_[YM7128B_Reg_VM]);
    YM7128B_Fixed input_sum = YM7128B_ClampAddFixed(input_vm, filter_vc);

    self->tail_ = self->tail_ ? (self->tail_ - 1) : (YM7128B_Buffer_Length - 1);
    self->buffer_[self->tail_] = input_sum;

    for (YM7128B_Register channel = 0; channel < YM7128B_OutputChannel_Count; ++channel) {
        YM7128B_Register gb = YM7128B_Reg_GL1 + (channel * YM7128B_Gain_Lane_Count);
        YM7128B_Accumulator accum = 0;

        for (YM7128B_Register tap = 1; tap < YM7128B_Tap_Count; ++tap) {
            YM7128B_Tap t = self->tail_ + self->taps_[tap];
            YM7128B_Tap head = (t >= YM7128B_Buffer_Length) ? (t - YM7128B_Buffer_Length) : t;
            YM7128B_Fixed buffered = self->buffer_[head];
            YM7128B_Fixed g = self->gains_[gb + tap - 1];
            YM7128B_Fixed buffered_g = YM7128B_MulFixed(buffered, g);
            accum += buffered_g;
        }

        YM7128B_Fixed total = YM7128B_ClampFixed(accum);
        YM7128B_Fixed v = self->gains_[YM7128B_Reg_VL + channel];
        YM7128B_Fixed total_v = YM7128B_MulFixed(total, v);

        YM7128B_OversamplerFixed* oversampler = &self->oversampler_[channel];
        YM7128B_Fixed* outputs = &data->outputs[channel][0];

        outputs[0] = YM7128B_OversamplerFixed_Process(oversampler, total_v);
        for (YM7128B_Register j = 1; j < YM7128B_Oversampling; ++j) {
            outputs[j] = YM7128B_OversamplerFixed_Process(oversampler, 0);
        }
    }
}

// ----------------------------------------------------------------------------

YM7128B_Register YM7128B_ChipFixed_Read(
    YM7128B_ChipFixed const* self,
    YM7128B_Address address
)
{
    if (address < YM7128B_Reg_C0) {
        return self->regs_[address] & YM7128B_Gain_Data_Mask;
    }
    else if (address < YM7128B_Reg_T0) {
        return self->regs_[address] & YM7128B_Coeff_Value_Mask;
    }
    else if (address < YM7128B_Reg_Count) {
        return self->regs_[address] & YM7128B_Tap_Value_Mask;
    }
    return 0;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Write(
    YM7128B_ChipFixed* self,
    YM7128B_Address address,
    YM7128B_Register data
)
{
    if (address < YM7128B_Reg_C0) {
        self->regs_[address] = data & YM7128B_Gain_Data_Mask;
        self->gains_[address] = YM7128B_RegisterToGainFixed(data);
    }
    else if (address < YM7128B_Reg_T0) {
        self->regs_[address] = data & YM7128B_Coeff_Value_Mask;
        self->gains_[address] = YM7128B_RegisterToCoeffFixed(data);
    }
    else if (address < YM7128B_Reg_Count) {
        self->regs_[address] = data & YM7128B_Tap_Value_Mask;
        self->taps_[address - YM7128B_Reg_T0] = YM7128B_RegisterToTap(data);
    }
}

// ============================================================================

void YM7128B_ChipFloat_Ctor(YM7128B_ChipFloat* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Dtor(YM7128B_ChipFloat* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Reset(YM7128B_ChipFloat* self)
{
    for (YM7128B_Address i = 0; i <= YM7128B_Address_Max; ++i) {
        self->regs_[i] = 0;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Start(YM7128B_ChipFloat* self)
{
    self->t0_d_ = 0;

    self->tail_ = 0;

    for (YM7128B_Tap i = 0; i < YM7128B_Buffer_Length; ++i) {
        self->buffer_[i] = 0;
    }

    for (YM7128B_Oversampler_Index i = 0; i < YM7128B_OutputChannel_Count; ++i) {
        YM7128B_OversamplerFloat_Reset(&self->oversampler_[i]);
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Stop(YM7128B_ChipFloat* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Process(
    YM7128B_ChipFloat* self,
    YM7128B_ChipFloat_Process_Data* data
)
{
    YM7128B_Float input = data->inputs[YM7128B_InputChannel_Mono];
    YM7128B_Float sample = input;

    YM7128B_Tap t0 = self->tail_ + self->taps_[0];
    YM7128B_Tap filter_head  = (t0 >= YM7128B_Buffer_Length) ? (t0 - YM7128B_Buffer_Length) : t0;
    YM7128B_Float filter_t0  = self->buffer_[filter_head];
    YM7128B_Float filter_d   = self->t0_d_;
    self->t0_d_ = filter_t0;
    YM7128B_Float filter_c0  = YM7128B_MulFloat(filter_t0, self->gains_[YM7128B_Reg_C0]);
    YM7128B_Float filter_c1  = YM7128B_MulFloat(filter_d, self->gains_[YM7128B_Reg_C1]);
    YM7128B_Float filter_sum = YM7128B_ClampAddFloat(filter_c0, filter_c1);
    YM7128B_Float filter_vc  = YM7128B_MulFloat(filter_sum, self->gains_[YM7128B_Reg_VC]);

    YM7128B_Float input_vm  = YM7128B_MulFloat(sample, self->gains_[YM7128B_Reg_VM]);
    YM7128B_Float input_sum = YM7128B_ClampAddFloat(input_vm, filter_vc);

    self->tail_ = self->tail_ ? (self->tail_ - 1) : (YM7128B_Buffer_Length - 1);
    self->buffer_[self->tail_] = input_sum;

    for (YM7128B_Register channel = 0; channel < YM7128B_OutputChannel_Count; ++channel) {
        YM7128B_Register gb = YM7128B_Reg_GL1 + (channel * YM7128B_Gain_Lane_Count);
        YM7128B_Float accum = 0;

        for (YM7128B_Register tap = 1; tap < YM7128B_Tap_Count; ++tap) {
            YM7128B_Tap t = self->tail_ + self->taps_[tap];
            YM7128B_Tap head = (t >= YM7128B_Buffer_Length) ? (t - YM7128B_Buffer_Length) : t;
            YM7128B_Float buffered = self->buffer_[head];
            YM7128B_Float g = self->gains_[gb + tap - 1];
            YM7128B_Float buffered_g = YM7128B_MulFloat(buffered, g);
            accum += buffered_g;
        }

        YM7128B_Float total = YM7128B_ClampFloat(accum);
        YM7128B_Float v = self->gains_[YM7128B_Reg_VL + channel];
        YM7128B_Float total_v = YM7128B_MulFloat(total, v);

        YM7128B_OversamplerFloat* oversampler = &self->oversampler_[channel];
        YM7128B_Float* outputs = &data->outputs[channel][0];

        outputs[0] = YM7128B_OversamplerFloat_Process(oversampler, total_v);
        for (YM7128B_Register j = 1; j < YM7128B_Oversampling; ++j) {
            outputs[j] = YM7128B_OversamplerFloat_Process(oversampler, 0);
        }
    }
}

// ----------------------------------------------------------------------------

YM7128B_Register YM7128B_ChipFloat_Read(
    YM7128B_ChipFloat const* self,
    YM7128B_Address address
)
{
    if (address < YM7128B_Reg_C0) {
        return self->regs_[address] & YM7128B_Gain_Data_Mask;
    }
    else if (address < YM7128B_Reg_T0) {
        return self->regs_[address] & YM7128B_Coeff_Value_Mask;
    }
    else if (address < YM7128B_Reg_Count) {
        return self->regs_[address] & YM7128B_Tap_Value_Mask;
    }
    return 0;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Write(
    YM7128B_ChipFloat* self,
    YM7128B_Address address,
    YM7128B_Register data
)
{
    if (address < YM7128B_Reg_C0) {
        self->regs_[address] = data & YM7128B_Gain_Data_Mask;
        self->gains_[address] = YM7128B_RegisterToGainFloat(data);
    }
    else if (address < YM7128B_Reg_T0) {
        self->regs_[address] = data & YM7128B_Coeff_Value_Mask;
        self->gains_[address] = YM7128B_RegisterToCoeffFloat(data);
    }
    else if (address < YM7128B_Reg_Count) {
        self->regs_[address] = data & YM7128B_Tap_Value_Mask;
        self->taps_[address - YM7128B_Reg_T0] = YM7128B_RegisterToTap(data);
    }
}

// ============================================================================

void YM7128B_ChipIdeal_Ctor(YM7128B_ChipIdeal* self)
{
    self->buffer_ = NULL;
    self->length_ = 0;
    self->sample_rate_ = 0;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Dtor(YM7128B_ChipIdeal* self)
{
    if (self->buffer_) {
        free(self->buffer_);
        self->buffer_ = NULL;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Reset(YM7128B_ChipIdeal* self)
{
    for (YM7128B_Address i = 0; i <= YM7128B_Address_Max; ++i) {
        self->regs_[i] = 0;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Start(YM7128B_ChipIdeal* self)
{
    self->t0_d_ = 0;

    self->tail_ = 0;

    if (self->buffer_) {
        for (YM7128B_TapIdeal i = 0; i < self->length_; ++i) {
            self->buffer_[i] = 0;
        }
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Stop(YM7128B_ChipIdeal* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Process(
    YM7128B_ChipIdeal* self,
    YM7128B_ChipIdeal_Process_Data* data
)
{
    if (self->buffer_ == NULL) {
        return;
    }

    YM7128B_Float input = data->inputs[YM7128B_InputChannel_Mono];
    YM7128B_Float sample = input;

    YM7128B_TapIdeal t0 = self->tail_ + self->taps_[0];
    YM7128B_TapIdeal filter_head = (t0 >= self->length_) ? (t0 - self->length_) : t0;
    YM7128B_Float filter_t0  = self->buffer_[filter_head];
    YM7128B_Float filter_d   = self->t0_d_;
    self->t0_d_ = filter_t0;
    YM7128B_Float filter_c0  = YM7128B_MulFloat(filter_t0, self->gains_[YM7128B_Reg_C0]);
    YM7128B_Float filter_c1  = YM7128B_MulFloat(filter_d, self->gains_[YM7128B_Reg_C1]);
    YM7128B_Float filter_sum = YM7128B_AddFloat(filter_c0, filter_c1);
    YM7128B_Float filter_vc  = YM7128B_MulFloat(filter_sum, self->gains_[YM7128B_Reg_VC]);

    YM7128B_Float input_vm  = YM7128B_MulFloat(sample, self->gains_[YM7128B_Reg_VM]);
    YM7128B_Float input_sum = YM7128B_AddFloat(input_vm, filter_vc);

    self->tail_ = self->tail_ ? (self->tail_ - 1) : (self->length_ - 1);
    self->buffer_[self->tail_] = input_sum;

    for (YM7128B_Register channel = 0; channel < YM7128B_OutputChannel_Count; ++channel) {
        YM7128B_Register gb = YM7128B_Reg_GL1 + (channel * YM7128B_Gain_Lane_Count);
        YM7128B_Float accum = 0;

        for (YM7128B_Register tap = 1; tap < YM7128B_Tap_Count; ++tap) {
            YM7128B_TapIdeal t = self->tail_ + self->taps_[tap];
            YM7128B_TapIdeal head = (t >= self->length_) ? (t - self->length_) : t;
            YM7128B_Float buffered = self->buffer_[head];
            YM7128B_Float g = self->gains_[gb + tap - 1];
            YM7128B_Float buffered_g = YM7128B_MulFloat(buffered, g);
            accum += buffered_g;
        }

        YM7128B_Float total = accum;
        YM7128B_Float v = self->gains_[YM7128B_Reg_VL + channel];
        YM7128B_Float total_v = YM7128B_MulFloat(total, v);
        YM7128B_Float og = 1 / (YM7128B_Float)YM7128B_Oversampling;
        YM7128B_Float oversampled = YM7128B_MulFloat(total_v, og);
        data->outputs[channel] = oversampled;
    }
}

// ----------------------------------------------------------------------------

YM7128B_Register YM7128B_ChipIdeal_Read(
    YM7128B_ChipIdeal const* self,
    YM7128B_Address address
)
{
    if (address < YM7128B_Reg_C0) {
        return self->regs_[address] & YM7128B_Gain_Data_Mask;
    }
    else if (address < YM7128B_Reg_T0) {
        return self->regs_[address] & YM7128B_Coeff_Value_Mask;
    }
    else if (address < YM7128B_Reg_Count) {
        return self->regs_[address] & YM7128B_Tap_Value_Mask;
    }
    return 0;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Write(
    YM7128B_ChipIdeal* self,
    YM7128B_Address address,
    YM7128B_Register data
)
{
    if (address < YM7128B_Reg_C0) {
        self->regs_[address] = data & YM7128B_Gain_Data_Mask;
        self->gains_[address] = YM7128B_RegisterToGainFloat(data);
    }
    else if (address < YM7128B_Reg_T0) {
        self->regs_[address] = data & YM7128B_Coeff_Value_Mask;
        self->gains_[address] = YM7128B_RegisterToCoeffFloat(data);
    }
    else if (address < YM7128B_Reg_Count) {
        self->regs_[address] = data & YM7128B_Tap_Value_Mask;
        self->taps_[address - YM7128B_Reg_T0] = YM7128B_RegisterToTapIdeal(data, self->sample_rate_);
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Setup(
    YM7128B_ChipIdeal* self,
    YM7128B_TapIdeal sample_rate
)
{
    if (self->sample_rate_ != sample_rate) {
        self->sample_rate_ = sample_rate;

        if (self->buffer_) {
            free(self->buffer_);
            self->buffer_ = NULL;
        }

        if (sample_rate >= 10) {
            self->length_ = (sample_rate / 10) + 1;
            self->buffer_ = (YM7128B_Float*)calloc(self->length_, sizeof(YM7128B_Float));

            for (YM7128B_Address i = 0; i < YM7128B_Tap_Count; ++i)
            {
                YM7128B_Register data = self->regs_[i + YM7128B_Reg_T0];
                self->taps_[i] = YM7128B_RegisterToTapIdeal(data, self->sample_rate_);
            }
        }
        else {
            self->length_ = 0;
        }
    }
}

// ============================================================================

void YM7128B_ChipShort_Ctor(YM7128B_ChipShort* self)
{
    self->buffer_ = NULL;
    self->length_ = 0;
    self->sample_rate_ = 0;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Dtor(YM7128B_ChipShort* self)
{
    if (self->buffer_) {
        free(self->buffer_);
        self->buffer_ = NULL;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Reset(YM7128B_ChipShort* self)
{
    for (YM7128B_Address i = 0; i <= YM7128B_Address_Max; ++i) {
        self->regs_[i] = 0;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Start(YM7128B_ChipShort* self)
{
    self->t0_d_ = 0;

    self->tail_ = 0;

    if (self->buffer_) {
        for (YM7128B_TapIdeal i = 0; i < self->length_; ++i) {
            self->buffer_[i] = 0;
        }
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Stop(YM7128B_ChipShort* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Process(
    YM7128B_ChipShort* self,
    YM7128B_ChipShort_Process_Data* data
)
{
    if (self->buffer_ == NULL) {
        return;
    }

    YM7128B_Fixed input = data->inputs[YM7128B_InputChannel_Mono];
    YM7128B_Fixed sample = input;

    YM7128B_TapIdeal t0 = self->tail_ + self->taps_[0];
    YM7128B_TapIdeal filter_head = (t0 >= self->length_) ? (t0 - self->length_) : t0;
    YM7128B_Fixed filter_t0  = self->buffer_[filter_head];
    YM7128B_Fixed filter_d   = self->t0_d_;
    self->t0_d_ = filter_t0;
    YM7128B_Fixed filter_c0  = YM7128B_MulShort(filter_t0, self->gains_[YM7128B_Reg_C0]);
    YM7128B_Fixed filter_c1  = YM7128B_MulShort(filter_d, self->gains_[YM7128B_Reg_C1]);
    YM7128B_Fixed filter_sum = YM7128B_ClampAddShort(filter_c0, filter_c1);
    YM7128B_Fixed filter_vc  = YM7128B_MulShort(filter_sum, self->gains_[YM7128B_Reg_VC]);

    YM7128B_Fixed input_vm  = YM7128B_MulShort(sample, self->gains_[YM7128B_Reg_VM]);
    YM7128B_Fixed input_sum = YM7128B_ClampAddShort(input_vm, filter_vc);

    self->tail_ = self->tail_ ? (self->tail_ - 1) : (self->length_ - 1);
    self->buffer_[self->tail_] = input_sum;

    for (YM7128B_Register channel = 0; channel < YM7128B_OutputChannel_Count; ++channel) {
        YM7128B_Register gb = YM7128B_Reg_GL1 + (channel * YM7128B_Gain_Lane_Count);
        YM7128B_Fixed accum = 0;

        for (YM7128B_Register tap = 1; tap < YM7128B_Tap_Count; ++tap) {
            YM7128B_TapIdeal t = self->tail_ + self->taps_[tap];
            YM7128B_TapIdeal head = (t >= self->length_) ? (t - self->length_) : t;
            YM7128B_Fixed buffered = self->buffer_[head];
            YM7128B_Fixed g = self->gains_[gb + tap - 1];
            YM7128B_Fixed buffered_g = YM7128B_MulShort(buffered, g);
            accum += buffered_g;
        }

        YM7128B_Fixed total = accum;
        YM7128B_Fixed v = self->gains_[YM7128B_Reg_VL + channel];
        YM7128B_Fixed total_v = YM7128B_MulShort(total, v);
        YM7128B_Fixed oversampled = total_v / (YM7128B_Fixed)YM7128B_Oversampling;
        data->outputs[channel] = oversampled;
    }
}

// ----------------------------------------------------------------------------

YM7128B_Register YM7128B_ChipShort_Read(
    YM7128B_ChipShort const* self,
    YM7128B_Address address
)
{
    if (address < YM7128B_Reg_C0) {
        return self->regs_[address] & YM7128B_Gain_Data_Mask;
    }
    else if (address < YM7128B_Reg_T0) {
        return self->regs_[address] & YM7128B_Coeff_Value_Mask;
    }
    else if (address < YM7128B_Reg_Count) {
        return self->regs_[address] & YM7128B_Tap_Value_Mask;
    }
    return 0;
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Write(
    YM7128B_ChipShort* self,
    YM7128B_Address address,
    YM7128B_Register data
)
{
    if (address < YM7128B_Reg_C0) {
        self->regs_[address] = data & YM7128B_Gain_Data_Mask;
        YM7128B_Fixed gain = YM7128B_RegisterToGainShort(data);
        self->gains_[address] = gain;
    }
    else if (address < YM7128B_Reg_T0) {
        self->regs_[address] = data & YM7128B_Coeff_Value_Mask;
        YM7128B_Fixed coeff = YM7128B_RegisterToCoeffShort(data);
        self->gains_[address] = coeff;
    }
    else if (address < YM7128B_Reg_Count) {
        self->regs_[address] = data & YM7128B_Tap_Value_Mask;
        YM7128B_TapIdeal tap = YM7128B_RegisterToTapIdeal(data, self->sample_rate_);
        self->taps_[address - YM7128B_Reg_T0] = tap;
    }
}

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Setup(
    YM7128B_ChipShort* self,
    YM7128B_TapIdeal sample_rate
)
{
    if (self->sample_rate_ != sample_rate) {
        self->sample_rate_ = sample_rate;

        if (self->buffer_) {
            free(self->buffer_);
            self->buffer_ = NULL;
        }

        if (sample_rate >= 10) {
            self->length_ = (sample_rate / 10) + 1;
            self->buffer_ = (YM7128B_Fixed*)calloc(self->length_, sizeof(YM7128B_Fixed));

            for (YM7128B_Address i = 0; i < YM7128B_Tap_Count; ++i)
            {
                YM7128B_Register data = self->regs_[i + YM7128B_Reg_T0];
                self->taps_[i] = YM7128B_RegisterToTapIdeal(data, self->sample_rate_);
            }
        }
        else {
            self->length_ = 0;
        }
    }
}

