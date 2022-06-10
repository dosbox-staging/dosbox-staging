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

#include "TDA8425_emu.h"

#include <math.h>

#ifndef M_PI
#define M_PI (3.14159265358979323846264338327950288)
#endif

// ============================================================================

char const* TDA8425_GetVersion(void)
{
    return TDA8425_VERSION;
}

// ============================================================================

signed char const TDA8425_VolumeDecibel_Table[TDA8425_Volume_Data_Count] =
{
    -128,  //  0
    -128,  //  1
    -128,  //  2
    -128,  //  3
    -128,  //  4
    -128,  //  5
    -128,  //  6
    -128,  //  7
    -128,  //  8
    -128,  //  9
    -128,  // 10
    -128,  // 11
    -128,  // 12
    -128,  // 13
    -128,  // 14
    -128,  // 15
    -128,  // 16
    -128,  // 17
    -128,  // 18
    -128,  // 19
    -128,  // 20
    -128,  // 21
    -128,  // 22
    -128,  // 23
    -128,  // 24
    -128,  // 25
    -128,  // 26
    -128,  // 27
    - 64,  // 28
    - 62,  // 29
    - 60,  // 30
    - 58,  // 31
    - 56,  // 32
    - 54,  // 33
    - 52,  // 34
    - 50,  // 35
    - 48,  // 36
    - 46,  // 37
    - 44,  // 38
    - 42,  // 39
    - 40,  // 40
    - 38,  // 41
    - 36,  // 42
    - 34,  // 43
    - 32,  // 44
    - 30,  // 45
    - 28,  // 46
    - 26,  // 47
    - 24,  // 48
    - 22,  // 49
    - 20,  // 50
    - 18,  // 51
    - 16,  // 52
    - 14,  // 53
    - 12,  // 54
    - 10,  // 55
    -  8,  // 56
    -  6,  // 57
    -  4,  // 58
    -  2,  // 59
       0,  // 60
    +  2,  // 61
    +  4,  // 62
    +  6   // 63
};

// ----------------------------------------------------------------------------

signed char const TDA8425_BassDecibel_Table[TDA8425_Tone_Data_Count] =
{
    -12,  //  0
    -12,  //  1
    -12,  //  2
    - 9,  //  3
    - 6,  //  4
    - 3,  //  5
      0,  //  6
    + 3,  //  7
    + 6,  //  8
    + 9,  //  9
    +12,  // 10
    +15,  // 11
    +15,  // 12
    +15,  // 13
    +15,  // 14
    +15   // 15
};

// ----------------------------------------------------------------------------

signed char const TDA8425_TrebleDecibel_Table[TDA8425_Tone_Data_Count] =
{
    -12,  //  0
    -12,  //  1
    -12,  //  2
    - 9,  //  3
    - 6,  //  4
    - 3,  //  5
      0,  //  6
    + 3,  //  7
    + 6,  //  8
    + 9,  //  9
    +12,  // 10
    +12,  // 11
    +12,  // 12
    +12,  // 13
    +12,  // 14
    +12   // 15
};

// ----------------------------------------------------------------------------

TDA8425_Float const TDA8425_Pseudo_C1_Table[TDA8425_Pseudo_Preset_Count] =
{
    (TDA8425_Float)15.e-9,
    (TDA8425_Float)5.6e-9,
    (TDA8425_Float)5.6e-9
};

TDA8425_Float const TDA8425_Pseudo_C2_Table[TDA8425_Pseudo_Preset_Count] =
{
    (TDA8425_Float)15.e-9,
    (TDA8425_Float)47.e-9,
    (TDA8425_Float)68.e-9
};

// ----------------------------------------------------------------------------

TDA8425_Float TDA8425_RegisterToVolume(TDA8425_Register data)
{
    TDA8425_Register n = data & (TDA8425_Register)TDA8425_Volume_Data_Mask;
    double db = (double)TDA8425_VolumeDecibel_Table[n];
    double g = pow(10.0, db * 0.05);
    return (TDA8425_Float)g;
}

// ----------------------------------------------------------------------------

TDA8425_Float TDA8425_RegisterToBass(TDA8425_Register data)
{
    TDA8425_Register n = data & (TDA8425_Register)TDA8425_Tone_Data_Mask;
    double db = (double)TDA8425_BassDecibel_Table[n];
    double g = pow(10.0, db * 0.05);
    return (TDA8425_Float)g;
}

// ----------------------------------------------------------------------------

TDA8425_Float TDA8425_RegisterToTreble(TDA8425_Register data)
{
    TDA8425_Register n = data & (TDA8425_Register)TDA8425_Tone_Data_Mask;
    double db = (double)TDA8425_TrebleDecibel_Table[n];
    double g = pow(10.0, db * 0.05);
    return (TDA8425_Float)g;
}

// ============================================================================

void TDA8425_BiQuadModel_SetupPseudo(
    TDA8425_BiQuadModel* model,
    TDA8425_Float sample_rate,
    TDA8425_Float pseudo_c1,
    TDA8425_Float pseudo_c2
)
{
    double fs = sample_rate;
    double k = 0.5 / fs;

    double c1 = pseudo_c1;
    double c2 = pseudo_c2;

    double r1 = (double)TDA8425_Pseudo_R1;
    double r2 = (double)TDA8425_Pseudo_R2;

    double t1 = c1 * r1;
    double t2 = c2 * r2;

    double kk = k * k;
    double t1_t2 = t1 * t2;
    double t1_t2_k = (t1 + t2) * k;

    double a0 = kk + t1_t2 + t1_t2_k;
    double a1 = (kk - t1_t2) * 2;
    double a2 = kk + t1_t2 - t1_t2_k;

    double b0 = a2;
    double b1 = a1;
    double b2 = a0;

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 *  ra0);
    model->b1 = (TDA8425_Float)(b1 *  ra0);
    model->b2 = (TDA8425_Float)(b2 *  ra0);

    model->a1 = (TDA8425_Float)(a1 * -ra0);
    model->a2 = (TDA8425_Float)(a2 * -ra0);
}

// ----------------------------------------------------------------------------

void TDA8425_BiQuadModel_SetupBass(
    TDA8425_BiQuadModel* model,
    TDA8425_Float sample_rate,
    TDA8425_Float bass_gain
)
{
    double g = sqrt(bass_gain);
    double fs = sample_rate;
    double k = 0.5 / fs;
    double w = (2 * M_PI) * (double)TDA8425_Bass_Frequency;

    double a0 = (k * w) + g;
    double a1 = (k * w) - g;

    double b0 = ((k * w) * (g * g)) + g;
    double b1 = ((k * w) * (g * g)) - g;

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 *  ra0);
    model->b1 = (TDA8425_Float)(b1 *  ra0);
    model->b2 = 0;

    model->a1 = (TDA8425_Float)(a1 * -ra0);
    model->a2 = 0;
}

// ----------------------------------------------------------------------------

void TDA8425_BiQuadModel_SetupTreble(
    TDA8425_BiQuadModel* model,
    TDA8425_Float sample_rate,
    TDA8425_Float treble_gain
)
{
    double g = sqrt(treble_gain);
    double fs = sample_rate;
    double k = 0.5 / fs;
    double w = (2 * M_PI) * (double)TDA8425_Treble_Frequency;

    double a0 = ((k * w) * g) + 1;
    double a1 = ((k * w) * g) - 1;

    double b0 = ((k * w) * g) + (g * g);
    double b1 = ((k * w) * g) - (g * g);

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 *  ra0);
    model->b1 = (TDA8425_Float)(b1 *  ra0);
    model->b2 = 0;

    model->a1 = (TDA8425_Float)(a1 * -ra0);
    model->a2 = 0;
}

// ----------------------------------------------------------------------------

void TDA8425_BiQuadModel_SetupTfilter(
    TDA8425_BiQuadModel* model,
    TDA8425_Float sample_rate,
    TDA8425_Float bass_gain
)
{
    double g = sqrt(bass_gain);
    double fs = sample_rate;
    double k = 0.5 / fs;
    double w = (2 * M_PI) * (double)TDA8425_Tfilter_Frequency;

    double log10_g = log10(g);
    double ang = log10_g * 0.85;
    double abs_sqrt_log10_g = sqrt(fabs(log10_g));
    double abs2_sqrt_log10_g = abs_sqrt_log10_g * abs_sqrt_log10_g;
    double kw = k * w;
    double m_k2w2 = (kw * kw) * -0.05;
    double sqrt_5 = 2.23606797749979;
    double ph = M_PI * 0.75;
    double h_sqrt_5_kw_abs_sqrt_log10_g = (0.2 * sqrt_5) * kw * abs_sqrt_log10_g;

    double a0 = ((m_k2w2 - abs2_sqrt_log10_g) +
                 (h_sqrt_5_kw_abs_sqrt_log10_g * cos(ang - ph)));

    double a1 = (m_k2w2 + abs2_sqrt_log10_g) * 2;

    double a2 = ((m_k2w2 - abs2_sqrt_log10_g) -
                 (h_sqrt_5_kw_abs_sqrt_log10_g * cos(ang - ph)));

    double b0 = ((m_k2w2 - abs2_sqrt_log10_g) +
                 (h_sqrt_5_kw_abs_sqrt_log10_g * cos(ang + ph)));

    double b1 = a1;

    double b2 = ((m_k2w2 - abs2_sqrt_log10_g) -
                (h_sqrt_5_kw_abs_sqrt_log10_g * cos(ang + ph)));

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 * ra0);
    model->b1 = (TDA8425_Float)(b1 * ra0);
    model->b2 = (TDA8425_Float)(b2 * ra0);

    model->a1 = (TDA8425_Float)(a1 * -ra0);
    model->a2 = (TDA8425_Float)(a2 * -ra0);
}

// ----------------------------------------------------------------------------

void TDA8425_BiQuadState_Clear(
    TDA8425_BiQuadState* state,
    TDA8425_Float output
)
{
    state->x0 = 0;
    state->x1 = 0;
    state->x2 = 0;

    state->y0 = output;
    state->y1 = output;
    state->y2 = output;
}

// ----------------------------------------------------------------------------

TDA8425_Float TDA8425_BiQuad_Process(
    TDA8425_BiQuadModel const* model,
    TDA8425_BiQuadState* state,
    TDA8425_Float input
)
{
    state->x2 = state->x1;
    state->x1 = state->x0;
    state->x0 = input;

    state->y2 = state->y1;
    state->y1 = state->y0;

    state->y0 = (state->x0 * model->b0 +
                 state->x1 * model->b1 +
                 state->x2 * model->b2
                 +
                 state->y1 * model->a1 +
                 state->y2 * model->a2);

    return state->y0;
}

// ============================================================================

void TDA8425_BiLinModel_SetupDCRemoval(
    TDA8425_BiLinModel* model,
    TDA8425_Float sample_rate
)
{
    double fs = sample_rate;
    double k = 0.5 / fs;
    double w = (2 * M_PI) * (double)TDA8425_Lowest_Frequency;

    double a0 = (k * w) + 1;
    double a1 = (k * w) - 1;

    double b0 = +1;
    double b1 = -1;

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 * ra0);
    model->b1 = (TDA8425_Float)(b1 * ra0);

    model->a1 = (TDA8425_Float)(a1 * -ra0);
}

// ----------------------------------------------------------------------------

void TDA8425_BiLinModel_SetupBass(
    TDA8425_BiLinModel* model,
    TDA8425_Float sample_rate,
    TDA8425_Float bass_gain
)
{
    double g = sqrt(bass_gain);
    double fs = sample_rate;
    double k = 0.5 / fs;
    double w = (2 * M_PI) * (double)TDA8425_Bass_Frequency;

    double a0 = (k * w) + g;
    double a1 = (k * w) - g;

    double b0 = ((k * w) * (g * g)) + g;
    double b1 = ((k * w) * (g * g)) - g;

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 *  ra0);
    model->b1 = (TDA8425_Float)(b1 *  ra0);

    model->a1 = (TDA8425_Float)(a1 * -ra0);
}

// ----------------------------------------------------------------------------

void TDA8425_BiLinModel_SetupTreble(
    TDA8425_BiLinModel* model,
    TDA8425_Float sample_rate,
    TDA8425_Float treble_gain
)
{
    double g = sqrt(treble_gain);
    double fs = sample_rate;
    double k = 0.5 / fs;
    double w = (2 * M_PI) * (double)TDA8425_Treble_Frequency;

    double a0 = ((k * w) * g) + 1;
    double a1 = ((k * w) * g) - 1;

    double b0 = ((k * w) * g) + (g * g);
    double b1 = ((k * w) * g) - (g * g);

    double ra0 = 1 / a0;

    model->b0 = (TDA8425_Float)(b0 *  ra0);
    model->b1 = (TDA8425_Float)(b1 *  ra0);

    model->a1 = (TDA8425_Float)(a1 * -ra0);
}

// ----------------------------------------------------------------------------

void TDA8425_BiLinState_Clear(
    TDA8425_BiLinState* state,
    TDA8425_Float output
)
{
    state->x0 = 0;
    state->x1 = 0;

    state->y0 = output;
    state->y1 = output;
}

// ----------------------------------------------------------------------------

TDA8425_Float TDA8425_BiLin_Process(
    TDA8425_BiLinModel const* model,
    TDA8425_BiLinState* state,
    TDA8425_Float input
)
{
    state->x1 = state->x0;
    state->x0 = input;

    state->y1 = state->y0;

    state->y0 = (state->x0 * model->b0 +
                 state->x1 * model->b1
                 +
                 state->y1 * model->a1);

    return state->y0;
}

// ============================================================================

void TDA8425_DCRemoval_Process(
    TDA8425_Float stereo[TDA8425_Stereo_Count],
    TDA8425_BiLinModel const* model,
    TDA8425_BiLinState state[TDA8425_Stereo_Count]
)
{
    for (int i = 0; i < TDA8425_Stereo_Count; ++i) {
        stereo[i] = TDA8425_BiLin_Process(
            model,
            &state[i],
            stereo[i]
        );
    }
}

// ----------------------------------------------------------------------------

void TDA8425_ForcedMono_Process(
    TDA8425_Float stereo[TDA8425_Stereo_Count]
)
{
    TDA8425_Float m = stereo[TDA8425_Stereo_L] + stereo[TDA8425_Stereo_R];
    stereo[TDA8425_Stereo_L] = m;
    stereo[TDA8425_Stereo_R] = m;
}

// ----------------------------------------------------------------------------

void TDA8425_PseudoStereo_Process(
    TDA8425_Float stereo[TDA8425_Stereo_Count],
    TDA8425_BiQuadModel const* model,
    TDA8425_BiQuadState* state
)
{
    stereo[TDA8425_Stereo_L] = TDA8425_BiQuad_Process(
        model,
        state,
        stereo[TDA8425_Stereo_L]
    );
}

// ----------------------------------------------------------------------------

void TDA8425_SpatialStereo_Process(
    TDA8425_Float stereo[TDA8425_Stereo_Count]
)
{
    TDA8425_Float l = stereo[TDA8425_Stereo_L];
    TDA8425_Float r = stereo[TDA8425_Stereo_R];
    TDA8425_Float const k = ((TDA8425_Float)TDA8425_Spatial_Crosstalk / 100);
    stereo[TDA8425_Stereo_L] = l + (l - r) * k;
    stereo[TDA8425_Stereo_R] = r + (r - l) * k;
}

// ============================================================================

void TDA8425_Chip_Ctor(TDA8425_Chip* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Dtor(TDA8425_Chip* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Setup(
    TDA8425_Chip* self,
    TDA8425_Float sample_rate,
    TDA8425_Float pseudo_c1,
    TDA8425_Float pseudo_c2,
    TDA8425_Tfilter_Mode tfilter_mode
)
{
    (void)tfilter_mode;
#if TDA8425_USE_TFILTER
    self->tfilter_mode_ = tfilter_mode;
#endif  // TDA8425_USE_TFILTER

    self->sample_rate_ = sample_rate;

#if TDA8425_USE_DC_REMOVAL
    TDA8425_BiLinModel_SetupDCRemoval(
        &self->dcremoval_model_,
        self->sample_rate_
    );
#endif  // TDA8425_USE_DC_REMOVAL

    TDA8425_BiQuadModel_SetupPseudo(
        &self->pseudo_model_,
        sample_rate,
        pseudo_c1,
        pseudo_c2
    );

#if TDA8425_USE_MODEL_CACHE
    for (TDA8425_Register i = 0; i < TDA8425_Tone_Data_Count; ++i) {
        TDA8425_Float bass_gain = TDA8425_RegisterToBass(i);

        TDA8425_BiLinModel_SetupBass(
            &self->bass_model_cache_[i],
            self->sample_rate_,
            bass_gain
        );

#if TDA8425_USE_TFILTER
        TDA8425_BiQuadModel_SetupTfilter(
            &self->tfilter_model_cache_[i],
            self->sample_rate_,
            bass_gain
        );
#endif  // TDA8425_USE_TFILTER

        TDA8425_Float treble_gain = TDA8425_RegisterToTreble(i);

        TDA8425_BiLinModel_SetupTreble(
            &self->treble_model_cache_[i],
            self->sample_rate_,
            treble_gain
        );
    }
#endif  // TDA8425_USE_MODEL_CACHE

    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_BA, self->reg_ba_);
    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_TR, self->reg_tr_);
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Reset(TDA8425_Chip* self)
{
    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_VL, 0);
    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_VR, 0);
    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_BA, 0);
    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_TR, 0);
    TDA8425_Chip_Write(self, (TDA8425_Address)TDA8425_Reg_SF, 0);
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Start(TDA8425_Chip* self)
{
    TDA8425_BiQuadState_Clear(&self->pseudo_state_, 0);

    for (int i = 0; i < TDA8425_Stereo_Count; ++i) {
        TDA8425_BiLinState_Clear(&self->bass_state_[i], 0);
        TDA8425_BiLinState_Clear(&self->treble_state_[i], 0);
#if TDA8425_USE_TFILTER
        TDA8425_BiQuadState_Clear(&self->tfilter_state_[i], 0);
#endif  // TDA8425_USE_TFILTER
    }
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Stop(TDA8425_Chip* self)
{
    (void)self;
}

// ----------------------------------------------------------------------------

static void TDA8425_Chip_ProcessSelector(
    TDA8425_Chip* self,
    TDA8425_Chip_Process_Data* data,
    TDA8425_Float stereo[TDA8425_Stereo_Count]
)
{
    TDA8425_Source const S1 = TDA8425_Source_1;
    TDA8425_Source const S2 = TDA8425_Source_2;
    TDA8425_Stereo const L = TDA8425_Stereo_L;
    TDA8425_Stereo const R = TDA8425_Stereo_R;

    switch (self->selector_)
    {
    case TDA8425_Selector_Sound_A_1:
        stereo[L] = data->inputs[S1][L];
        stereo[R] = data->inputs[S1][L];
        break;

    case TDA8425_Selector_Sound_A_2:
        stereo[L] = data->inputs[S2][L];
        stereo[R] = data->inputs[S2][L];
        break;

    case TDA8425_Selector_Sound_B_1:
        stereo[L] = data->inputs[S1][R];
        stereo[R] = data->inputs[S1][R];
        break;

    case TDA8425_Selector_Sound_B_2:
        stereo[L] = data->inputs[S2][L];
        stereo[R] = data->inputs[S2][R];
        break;

    case TDA8425_Selector_Stereo_2:
        stereo[L] = data->inputs[S2][L];
        stereo[R] = data->inputs[S2][R];
        break;

    case TDA8425_Selector_Stereo_1:
    default:
        stereo[L] = data->inputs[S1][L];
        stereo[R] = data->inputs[S1][R];
        break;
    }
}

// ----------------------------------------------------------------------------

static void TDA8425_Chip_ProcessMode(
    TDA8425_Chip* self,
    TDA8425_Float stereo[TDA8425_Stereo_Count]
)
{
    switch (self->mode_)
    {
    case TDA8425_Mode_ForcedMono:
        TDA8425_ForcedMono_Process(stereo);
        break;

    case TDA8425_Mode_LinearStereo:
        break;

    case TDA8425_Mode_PseudoStereo:
        TDA8425_PseudoStereo_Process(
            stereo,
            &self->pseudo_model_,
            &self->pseudo_state_
        );
        break;

    case TDA8425_Mode_SpatialStereo:
        TDA8425_SpatialStereo_Process(stereo);
        break;

    default:
        break;
    }
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Process(
    TDA8425_Chip* self,
    TDA8425_Chip_Process_Data* data
)
{
    TDA8425_Float stereo[TDA8425_Stereo_Count];

    TDA8425_Chip_ProcessSelector(self, data, stereo);

#if TDA8425_USE_DC_REMOVAL
    TDA8425_DCRemoval_Process(
        stereo,
        &self->dcremoval_model_,
        self->dcremoval_state_
    );
#endif  // TDA8425_USE_DC_REMOVAL

    TDA8425_Chip_ProcessMode(self, stereo);

    for (int channel = 0; channel < TDA8425_Stereo_Count; ++channel) {
        TDA8425_Float sample = self->volume_[channel] * stereo[channel];

        sample = TDA8425_BiLin_Process(
            &self->bass_model_,
            &self->bass_state_[channel],
            sample
        );

        sample = TDA8425_BiLin_Process(
            &self->treble_model_,
            &self->treble_state_[channel],
            sample
        );

#if TDA8425_USE_TFILTER
        if (self->tfilter_mode_ == TDA8425_Tfilter_Mode_Disabled) {
            data->outputs[channel] = sample;  // shortcut
        }
        else {
            sample = TDA8425_BiQuad_Process(
                &self->tfilter_model_,
                &self->tfilter_state_[channel],
                sample
            );

            data->outputs[channel] = sample;
        }
#else  // TDA8425_USE_TFILTER
        data->outputs[channel] = sample;
#endif  // TDA8425_USE_TFILTER
    }
}

// ----------------------------------------------------------------------------

TDA8425_Register TDA8425_Chip_Read(
    TDA8425_Chip const* self,
    TDA8425_Address address
)
{
    switch ((TDA8425_Reg)address)
    {
    case TDA8425_Reg_VL:
        return self->reg_vl_ | (TDA8425_Register)~TDA8425_Volume_Data_Mask;

    case TDA8425_Reg_VR:
        return self->reg_vr_ | (TDA8425_Register)~TDA8425_Volume_Data_Mask;

    case TDA8425_Reg_BA:
        return self->reg_ba_ | (TDA8425_Register)~TDA8425_Tone_Data_Mask;

    case TDA8425_Reg_TR:
        return self->reg_tr_ | (TDA8425_Register)~TDA8425_Tone_Data_Mask;

    case TDA8425_Reg_SF:
        return self->reg_sf_ | (TDA8425_Register)~TDA8425_Switch_Data_Mask;

    default:
        return (TDA8425_Register)~0;
    }
}

// ----------------------------------------------------------------------------

void TDA8425_Chip_Write(
    TDA8425_Chip* self,
    TDA8425_Address address,
    TDA8425_Register data
)
{
    switch ((TDA8425_Reg)address)
    {
    case TDA8425_Reg_VL:
        data |= (TDA8425_Register)~TDA8425_Volume_Data_Mask;
        self->reg_vl_ = data;

        self->volume_[TDA8425_Stereo_L] = TDA8425_RegisterToVolume(data);
        break;

    case TDA8425_Reg_VR:
        data |= (TDA8425_Register)~TDA8425_Volume_Data_Mask;
        self->reg_vr_ = data;

        self->volume_[TDA8425_Stereo_R] = TDA8425_RegisterToVolume(data);
        break;

    case TDA8425_Reg_BA:
        data |= (TDA8425_Register)~TDA8425_Tone_Data_Mask;
        self->reg_ba_ = data;

#if TDA8425_USE_MODEL_CACHE
        data &= (TDA8425_Register)TDA8425_Tone_Data_Mask;
        self->bass_model_ = self->bass_model_cache_[data];
#if TDA8425_USE_TFILTER
        self->tfilter_model_ = self->tfilter_model_cache_[data];
#endif  // TDA8425_USE_TFILTER
#else  // TDA8425_USE_MODEL_CACHE
        TDA8425_Float bass_gain = TDA8425_RegisterToBass(data);

        TDA8425_BiLinModel_SetupBass(
            &self->bass_model_,
            self->sample_rate_,
            bass_gain
        );

#if TDA8425_USE_TFILTER
        if (self->tfilter_mode_) {
            TDA8425_BiQuadModel_SetupTfilter(
                &self->tfilter_model_,
                self->sample_rate_,
                bass_gain
            );
        }
#endif  // TDA8425_USE_TFILTER
#endif  // TDA8425_USE_MODEL_CACHE
        break;

    case TDA8425_Reg_TR:
        data |= (TDA8425_Register)~TDA8425_Tone_Data_Mask;
        self->reg_tr_ = data;

#if TDA8425_USE_MODEL_CACHE
        data &= (TDA8425_Register)TDA8425_Tone_Data_Mask;
        self->treble_model_ = self->treble_model_cache_[data];
#else  // TDA8425_USE_MODEL_CACHE
        TDA8425_Float treble_gain = TDA8425_RegisterToTreble(data);

        TDA8425_BiLinModel_SetupTreble(
            &self->treble_model_,
            self->sample_rate_,
            treble_gain
        );
#endif  // TDA8425_USE_MODEL_CACHE
        break;

    case TDA8425_Reg_SF:
        data |= (TDA8425_Register)~TDA8425_Switch_Data_Mask;
        self->reg_sf_ = data;

        self->selector_ = (TDA8425_Selector)(
            self->reg_sf_
            & (TDA8425_Register)TDA8425_Selector_Mask
        );

        self->mode_ = (TDA8425_Mode)(
            (self->reg_sf_ >> TDA8425_Reg_SF_STL)
            & (TDA8425_Register)TDA8425_Mode_Mask
        );
        break;

    default:
        break;
    }
}
