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

#ifndef _YM7128B_EMU_H_
#define _YM7128B_EMU_H_

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef YM7128B_INLINE
#define YM7128B_INLINE static inline
#endif

#ifndef YM7128B_FLOAT
#define YM7128B_FLOAT float         //!< Floating point data type
#define YM7128B_Float_Min   (-1)    //!< Maximum floating point value
#define YM7128B_Float_Max   (+1)    //!< Minimum floating point value
#endif

#ifndef YM7128B_USE_MINPHASE
#define YM7128B_USE_MINPHASE 0          //!< Enables minimum-phase oversampler kernel
#endif

// ============================================================================

#define YM7128B_VERSION "0.1.1"

char const* YM7128B_GetVersion(void);

// ============================================================================

typedef uint8_t  YM7128B_Address;   //!< Address data type
typedef uint8_t  YM7128B_Register;  //!< Register data type
typedef uint16_t YM7128B_Tap;       //!< Tap data type

//! Registers enumerator
typedef enum YM7128B_Reg {
    YM7128B_Reg_GL1 = 0,
    YM7128B_Reg_GL2,
    YM7128B_Reg_GL3,
    YM7128B_Reg_GL4,
    YM7128B_Reg_GL5,
    YM7128B_Reg_GL6,
    YM7128B_Reg_GL7,
    YM7128B_Reg_GL8,

    YM7128B_Reg_GR1,
    YM7128B_Reg_GR2,
    YM7128B_Reg_GR3,
    YM7128B_Reg_GR4,
    YM7128B_Reg_GR5,
    YM7128B_Reg_GR6,
    YM7128B_Reg_GR7,
    YM7128B_Reg_GR8,

    YM7128B_Reg_VM,
    YM7128B_Reg_VC,
    YM7128B_Reg_VL,
    YM7128B_Reg_VR,

    YM7128B_Reg_C0,
    YM7128B_Reg_C1,

    YM7128B_Reg_T0,
    YM7128B_Reg_T1,
    YM7128B_Reg_T2,
    YM7128B_Reg_T3,
    YM7128B_Reg_T4,
    YM7128B_Reg_T5,
    YM7128B_Reg_T6,
    YM7128B_Reg_T7,
    YM7128B_Reg_T8,

    YM7128B_Reg_Count
} YM7128B_Reg;

//! Input channels enumerator
typedef enum YM7128B_InputChannel {
    YM7128B_InputChannel_Mono = 0,
    YM7128B_InputChannel_Count
} YM7128B_InputChannel;

//! Output channels enumerator
typedef enum YM7128B_OutputChannel {
    YM7128B_OutputChannel_Left = 0,
    YM7128B_OutputChannel_Right,
    YM7128B_OutputChannel_Count
} YM7128B_OutputChannel;

//! Datasheet specifications
enum YM7128B_DatasheetSpecs {
    //! Clock rate [Hz]
    YM7128B_Clock_Rate        = 7159090,

    //! Register write rate [Hz]
    YM7128B_Write_Rate        = (YM7128B_Clock_Rate / 8) / (8 + 1 + 8 + 1),

    //! Input sample rate [Hz]
    YM7128B_Input_Rate        = (YM7128B_Clock_Rate + (304 / 2)) / 304,

    //! Output oversampling factor
    YM7128B_Oversampling      = 2,

    //! Output sample rate
    YM7128B_Output_Rate       = YM7128B_Input_Rate * YM7128B_Oversampling,

    //! Maximum register address
    YM7128B_Address_Max       = YM7128B_Reg_Count - 1,

    //! Nominal delay line buffer length
    YM7128B_Buffer_Length     = (YM7128B_Input_Rate / 10) + 1,

    // Delay line taps
    YM7128B_Tap_Count         = 9,
    YM7128B_Tap_Value_Bits    = 5,
    YM7128B_Tap_Value_Count   = 1 << YM7128B_Tap_Value_Bits,
    YM7128B_Tap_Value_Mask    = YM7128B_Tap_Value_Count - 1,

    // Gain coefficients
    YM7128B_Gain_Lane_Count   = 8,
    YM7128B_Gain_Data_Bits    = 6,
    YM7128B_Gain_Data_Count   = 1 << YM7128B_Gain_Data_Bits,
    YM7128B_Gain_Data_Mask    = YM7128B_Gain_Data_Count - 1,
    YM7128B_Gain_Data_Sign    = 1 << (YM7128B_Gain_Data_Bits - 1),

    // Feedback coefficients
    YM7128B_Coeff_Count       = 2,
    YM7128B_Coeff_Value_Bits  = 6,
    YM7128B_Coeff_Value_Count = 1 << YM7128B_Coeff_Value_Bits,
    YM7128B_Coeff_Value_Mask  = YM7128B_Coeff_Value_Count - 1
};

// ============================================================================

typedef int16_t       YM7128B_Fixed;        //!< Fixed point data type
typedef int_fast32_t  YM7128B_Accumulator;  //!< Fixed point accumulator data type
typedef YM7128B_FLOAT YM7128B_Float;        //!< Floating point data type
typedef size_t        YM7128B_TapIdeal;     //!< Ideal chip tap data type

enum YM7128B_ImplementationSpecs {
    // Fixed point specs
    YM7128B_Fixed_Bits          = sizeof(YM7128B_Fixed) * CHAR_BIT,
    YM7128B_Fixed_Mask          = (1 << YM7128B_Fixed_Bits) - 1,
    YM7128B_Fixed_Decimals      = YM7128B_Fixed_Bits - 1,
    YM7128B_Fixed_Rounding      = 1 << (YM7128B_Fixed_Decimals - 1),
    YM7128B_Fixed_Max           = (1 << YM7128B_Fixed_Decimals) - 1,
    YM7128B_Fixed_Min           = -YM7128B_Fixed_Max,

    // Signal specs
    YM7128B_Signal_Bits         = 14,
    YM7128B_Signal_Clear_Bits   = YM7128B_Fixed_Bits - YM7128B_Signal_Bits,
    YM7128B_Signal_Clear_Mask   = (1 << YM7128B_Signal_Clear_Bits) - 1,
    YM7128B_Signal_Mask         = YM7128B_Fixed_Mask - YM7128B_Signal_Clear_Mask,
    YM7128B_Signal_Decimals     = YM7128B_Signal_Bits - 1,

    // Signal multiplication operand specs
    YM7128B_Operand_Bits        = YM7128B_Fixed_Bits,//TBV 14,
    YM7128B_Operand_Clear_Bits  = YM7128B_Fixed_Bits - YM7128B_Operand_Bits,
    YM7128B_Operand_Clear_Mask  = (1 << YM7128B_Operand_Clear_Bits) - 1,
    YM7128B_Operand_Mask        = YM7128B_Fixed_Mask - YM7128B_Operand_Clear_Mask,
    YM7128B_Operand_Decimals    = YM7128B_Operand_Bits - 1,

    // Gain multiplication operand specs
    YM7128B_Gain_Bits           = 12,
    YM7128B_Gain_Clear_Bits     = YM7128B_Fixed_Bits - YM7128B_Gain_Bits,
    YM7128B_Gain_Clear_Mask     = (1 << YM7128B_Gain_Clear_Bits) - 1,
    YM7128B_Gain_Mask           = YM7128B_Fixed_Mask - YM7128B_Gain_Clear_Mask,
    YM7128B_Gain_Decimals       = YM7128B_Gain_Bits - 1,
    YM7128B_Gain_Max            = (1 << (YM7128B_Fixed_Bits - 1)) - 1,
    YM7128B_Gain_Min            = -YM7128B_Gain_Max,

    // Feedback coefficient multiplication operand specs
    YM7128B_Coeff_Bits          = YM7128B_Gain_Bits,
    YM7128B_Coeff_Clear_Bits    = YM7128B_Fixed_Bits - YM7128B_Coeff_Bits,
    YM7128B_Coeff_Clear_Mask    = (1 << YM7128B_Coeff_Clear_Bits) - 1,
    YM7128B_Coeff_Mask          = YM7128B_Fixed_Mask - YM7128B_Coeff_Clear_Mask,
    YM7128B_Coeff_Decimals      = YM7128B_Coeff_Bits - 1
};

typedef enum YM7128B_ChipEngine {
    YM7128B_ChipEngine_Fixed = 0,
    YM7128B_ChipEngine_Float,
    YM7128B_ChipEngine_Ideal,
    YM7128B_ChipEngine_Short,
    YM7128B_ChipEngine_Count
} YM7128B_ChipEngine;

// ----------------------------------------------------------------------------

extern signed char const YM7128B_GainDecibel_Table[YM7128B_Gain_Data_Count / 2];
extern YM7128B_Fixed const YM7128B_GainFixed_Table[YM7128B_Gain_Data_Count];
extern YM7128B_Float const YM7128B_GainFloat_Table[YM7128B_Gain_Data_Count];
extern YM7128B_Fixed const YM7128B_GainShort_Table[YM7128B_Gain_Data_Count];
extern YM7128B_Tap const YM7128B_Tap_Table[YM7128B_Tap_Value_Count];

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Tap YM7128B_RegisterToTap(YM7128B_Register data)
{
    YM7128B_Register i = data & YM7128B_Tap_Value_Mask;
    YM7128B_Tap t = YM7128B_Tap_Table[i];
    return t;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_TapIdeal YM7128B_RegisterToTapIdeal(
    YM7128B_Register data,
    YM7128B_TapIdeal sample_rate
)
{
    YM7128B_Register i = data & YM7128B_Tap_Value_Mask;
    YM7128B_TapIdeal t = (YM7128B_TapIdeal)((i * (sample_rate / 10)) / (YM7128B_Tap_Value_Count - 1));
    return t;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_RegisterToGainFixed(YM7128B_Register data)
{
    YM7128B_Register i = data & YM7128B_Gain_Data_Mask;
    YM7128B_Fixed g = YM7128B_GainFixed_Table[i];
    return g;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Float YM7128B_RegisterToGainFloat(YM7128B_Register data)
{
    YM7128B_Register i = data & YM7128B_Gain_Data_Mask;
    YM7128B_Float g = YM7128B_GainFloat_Table[i];
    return g;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_RegisterToGainShort(YM7128B_Register data)
{
    YM7128B_Register i = data & YM7128B_Gain_Data_Mask;
    YM7128B_Fixed g = YM7128B_GainShort_Table[i];
    return g;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_RegisterToCoeffFixed(YM7128B_Register data)
{
    YM7128B_Register r = data & YM7128B_Coeff_Value_Mask;
    YM7128B_Register sh = YM7128B_Fixed_Bits - YM7128B_Coeff_Value_Bits;
    YM7128B_Fixed c = (YM7128B_Fixed)r << sh;
    return c;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Float YM7128B_RegisterToCoeffFloat(YM7128B_Register data)
{
    YM7128B_Fixed k = YM7128B_RegisterToCoeffFixed(data);
    YM7128B_Float c = (YM7128B_Float)k * (1 / (YM7128B_Float)YM7128B_Gain_Max);
    return c;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_RegisterToCoeffShort(YM7128B_Register data)
{
    YM7128B_Register r = data & YM7128B_Coeff_Value_Mask;
    YM7128B_Register sh = YM7128B_Fixed_Bits - YM7128B_Coeff_Value_Bits;
    YM7128B_Fixed c = (YM7128B_Fixed)r << sh;
    return c;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_ClampFixed(YM7128B_Accumulator signal)
{
    if (signal < YM7128B_Fixed_Min) {
        signal = YM7128B_Fixed_Min;
    }
    if (signal > YM7128B_Fixed_Max) {
        signal = YM7128B_Fixed_Max;
    }
    return (YM7128B_Fixed)signal & (YM7128B_Fixed)YM7128B_Operand_Mask;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Float YM7128B_ClampFloat(YM7128B_Float signal)
{
    if (signal < YM7128B_Float_Min) {
        return YM7128B_Float_Min;
    }
    if (signal > YM7128B_Float_Max) {
        return YM7128B_Float_Max;
    }
    return signal;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_ClampShort(YM7128B_Accumulator signal)
{
    if (signal < YM7128B_Fixed_Min) {
        return (YM7128B_Fixed)YM7128B_Fixed_Min;
    }
    if (signal > YM7128B_Fixed_Max) {
        return (YM7128B_Fixed)YM7128B_Fixed_Max;
    }
    return (YM7128B_Fixed)signal;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_ClampAddFixed(YM7128B_Fixed a, YM7128B_Fixed b)
{
    YM7128B_Accumulator aa = a & (YM7128B_Fixed)YM7128B_Operand_Mask;
    YM7128B_Accumulator bb = b & (YM7128B_Fixed)YM7128B_Operand_Mask;
    YM7128B_Accumulator ss = aa + bb;
    YM7128B_Fixed y = YM7128B_ClampFixed(ss);
    return y;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Float YM7128B_ClampAddFloat(YM7128B_Float a, YM7128B_Float b)
{
    YM7128B_Float y = YM7128B_ClampFloat(a + b);
    return y;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_ClampAddShort(YM7128B_Fixed a, YM7128B_Fixed b)
{
    YM7128B_Accumulator aa = a;
    YM7128B_Accumulator bb = b;
    YM7128B_Accumulator ss = aa + bb;
    YM7128B_Fixed y = YM7128B_ClampShort(ss);
    return y;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Float YM7128B_AddFloat(YM7128B_Float a, YM7128B_Float b)
{
    YM7128B_Float y = a + b;
    return y;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_MulFixed(YM7128B_Fixed a, YM7128B_Fixed b)
{
    YM7128B_Accumulator aa = a & (YM7128B_Fixed)YM7128B_Operand_Mask;
    YM7128B_Accumulator bb = b & (YM7128B_Fixed)YM7128B_Operand_Mask;
    YM7128B_Accumulator mm = aa * bb;
    YM7128B_Fixed x = (YM7128B_Fixed)(mm >> YM7128B_Fixed_Decimals);
    YM7128B_Fixed y = x & (YM7128B_Fixed)YM7128B_Operand_Mask;
    return y;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Float YM7128B_MulFloat(YM7128B_Float a, YM7128B_Float b)
{
    YM7128B_Float y = a * b;
    return y;
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
YM7128B_Fixed YM7128B_MulShort(YM7128B_Fixed a, YM7128B_Fixed b)
{
    YM7128B_Accumulator aa = a;
    YM7128B_Accumulator bb = b;
    YM7128B_Accumulator mm = aa * bb;
    YM7128B_Fixed y = (YM7128B_Fixed)(mm >> YM7128B_Fixed_Decimals);
    return y;
}

// ============================================================================

typedef uint_fast8_t YM7128B_Oversampler_Index;

enum YM7128B_OversamplerSpecs {
    YM7128B_Oversampler_Factor = YM7128B_Oversampling,
    YM7128B_Oversampler_Length = 19,
};

// ----------------------------------------------------------------------------

typedef struct YM7128B_OversamplerFixed
{
    YM7128B_Fixed buffer_[YM7128B_Oversampler_Length];
} YM7128B_OversamplerFixed;

extern YM7128B_Fixed const YM7128B_OversamplerFixed_Kernel[YM7128B_Oversampler_Length];

// ----------------------------------------------------------------------------

YM7128B_INLINE
void YM7128B_OversamplerFixed_Clear(
    YM7128B_OversamplerFixed* self,
    YM7128B_Fixed input
)
{
    for (YM7128B_Oversampler_Index index = 0; index < YM7128B_Oversampler_Length; ++index) {
        self->buffer_[index] = input;
    }
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
void YM7128B_OversamplerFixed_Reset(YM7128B_OversamplerFixed* self)
{
    YM7128B_OversamplerFixed_Clear(self, 0);
}

// ----------------------------------------------------------------------------

YM7128B_Fixed YM7128B_OversamplerFixed_Process(
    YM7128B_OversamplerFixed* self,
    YM7128B_Fixed input
);

// ============================================================================

typedef struct YM7128B_OversamplerFloat
{
    YM7128B_Float buffer_[YM7128B_Oversampler_Length];
} YM7128B_OversamplerFloat;

extern YM7128B_Float const YM7128B_OversamplerFloat_Kernel[YM7128B_Oversampler_Length];

// ----------------------------------------------------------------------------

YM7128B_INLINE
void YM7128B_OversamplerFloat_Clear(
    YM7128B_OversamplerFloat* self,
    YM7128B_Float input
)
{
    for (YM7128B_Oversampler_Index index = 0; index < YM7128B_Oversampler_Length; ++index) {
        self->buffer_[index] = input;
    }
}

// ----------------------------------------------------------------------------

YM7128B_INLINE
void YM7128B_OversamplerFloat_Reset(YM7128B_OversamplerFloat* self)
{
    YM7128B_OversamplerFloat_Clear(self, 0);
}

// ----------------------------------------------------------------------------

YM7128B_Float YM7128B_OversamplerFloat_Process(
    YM7128B_OversamplerFloat* self,
    YM7128B_Float input
);

// ============================================================================

typedef struct YM7128B_ChipFixed
{
    YM7128B_Register regs_[YM7128B_Reg_Count];
    YM7128B_Fixed gains_[YM7128B_Reg_T0];
    YM7128B_Tap taps_[YM7128B_Tap_Count];
    YM7128B_Fixed t0_d_;
    YM7128B_Tap tail_;
    YM7128B_Fixed buffer_[YM7128B_Buffer_Length];
    YM7128B_OversamplerFixed oversampler_[YM7128B_OutputChannel_Count];
} YM7128B_ChipFixed;

typedef struct YM7128B_ChipFixed_Process_Data
{
    YM7128B_Fixed inputs[YM7128B_InputChannel_Count];
    YM7128B_Fixed outputs[YM7128B_OutputChannel_Count][YM7128B_Oversampling];
} YM7128B_ChipFixed_Process_Data;

// ----------------------------------------------------------------------------

void YM7128B_ChipFixed_Ctor(YM7128B_ChipFixed* self);

void YM7128B_ChipFixed_Dtor(YM7128B_ChipFixed* self);

void YM7128B_ChipFixed_Reset(YM7128B_ChipFixed* self);

void YM7128B_ChipFixed_Start(YM7128B_ChipFixed* self);

void YM7128B_ChipFixed_Stop(YM7128B_ChipFixed* self);

void YM7128B_ChipFixed_Process(
    YM7128B_ChipFixed* self,
    YM7128B_ChipFixed_Process_Data* data
);

YM7128B_Register YM7128B_ChipFixed_Read(
    YM7128B_ChipFixed const* self,
    YM7128B_Address address
);

void YM7128B_ChipFixed_Write(
    YM7128B_ChipFixed* self,
    YM7128B_Address address,
    YM7128B_Register data
);

// ============================================================================

typedef struct YM7128B_ChipFloat
{
    YM7128B_Register regs_[YM7128B_Reg_Count];
    YM7128B_Float gains_[YM7128B_Reg_T0];
    YM7128B_Tap taps_[YM7128B_Tap_Count];
    YM7128B_Float t0_d_;
    YM7128B_Tap tail_;
    YM7128B_Float buffer_[YM7128B_Buffer_Length];
    YM7128B_OversamplerFloat oversampler_[YM7128B_OutputChannel_Count];
} YM7128B_ChipFloat;

typedef struct YM7128B_ChipFloat_Process_Data
{
    YM7128B_Float inputs[YM7128B_InputChannel_Count];
    YM7128B_Float outputs[YM7128B_OutputChannel_Count][YM7128B_Oversampling];
} YM7128B_ChipFloat_Process_Data;

// ----------------------------------------------------------------------------

void YM7128B_ChipFloat_Ctor(YM7128B_ChipFloat* self);

void YM7128B_ChipFloat_Dtor(YM7128B_ChipFloat* self);

void YM7128B_ChipFloat_Reset(YM7128B_ChipFloat* self);

void YM7128B_ChipFloat_Start(YM7128B_ChipFloat* self);

void YM7128B_ChipFloat_Stop(YM7128B_ChipFloat* self);

void YM7128B_ChipFloat_Process(
    YM7128B_ChipFloat* self,
    YM7128B_ChipFloat_Process_Data* data
);

YM7128B_Register YM7128B_ChipFloat_Read(
    YM7128B_ChipFloat const* self,
    YM7128B_Address address
);

void YM7128B_ChipFloat_Write(
    YM7128B_ChipFloat* self,
    YM7128B_Address address,
    YM7128B_Register data
);

// ============================================================================

typedef struct YM7128B_ChipIdeal
{
    YM7128B_Register regs_[YM7128B_Reg_Count];
    YM7128B_Float gains_[YM7128B_Reg_T0];
    YM7128B_TapIdeal taps_[YM7128B_Tap_Count];
    YM7128B_Float t0_d_;
    YM7128B_TapIdeal tail_;
    YM7128B_Float* buffer_;
    YM7128B_TapIdeal length_;
    YM7128B_TapIdeal sample_rate_;
} YM7128B_ChipIdeal;

typedef struct YM7128B_ChipIdeal_Process_Data
{
    YM7128B_Float inputs[YM7128B_InputChannel_Count];
    YM7128B_Float outputs[YM7128B_OutputChannel_Count];
} YM7128B_ChipIdeal_Process_Data;

// ----------------------------------------------------------------------------

void YM7128B_ChipIdeal_Ctor(YM7128B_ChipIdeal* self);

void YM7128B_ChipIdeal_Dtor(YM7128B_ChipIdeal* self);

void YM7128B_ChipIdeal_Reset(YM7128B_ChipIdeal* self);

void YM7128B_ChipIdeal_Start(YM7128B_ChipIdeal* self);

void YM7128B_ChipIdeal_Stop(YM7128B_ChipIdeal* self);

void YM7128B_ChipIdeal_Process(
    YM7128B_ChipIdeal* self,
    YM7128B_ChipIdeal_Process_Data* data
);

YM7128B_Register YM7128B_ChipIdeal_Read(
    YM7128B_ChipIdeal const* self,
    YM7128B_Address address
);

void YM7128B_ChipIdeal_Write(
    YM7128B_ChipIdeal* self,
    YM7128B_Address address,
    YM7128B_Register data
);

void YM7128B_ChipIdeal_Setup(
    YM7128B_ChipIdeal* self,
    YM7128B_TapIdeal sample_rate
);

// ============================================================================

typedef struct YM7128B_ChipShort
{
    YM7128B_Register regs_[YM7128B_Reg_Count];
    YM7128B_Fixed gains_[YM7128B_Reg_T0];
    YM7128B_TapIdeal taps_[YM7128B_Tap_Count];
    YM7128B_Fixed t0_d_;
    YM7128B_TapIdeal tail_;
    YM7128B_Fixed* buffer_;
    YM7128B_TapIdeal length_;
    YM7128B_TapIdeal sample_rate_;
} YM7128B_ChipShort;

typedef struct YM7128B_ChipShort_Process_Data
{
    YM7128B_Fixed inputs[YM7128B_InputChannel_Count];
    YM7128B_Fixed outputs[YM7128B_OutputChannel_Count];
} YM7128B_ChipShort_Process_Data;

// ----------------------------------------------------------------------------

void YM7128B_ChipShort_Ctor(YM7128B_ChipShort* self);

void YM7128B_ChipShort_Dtor(YM7128B_ChipShort* self);

void YM7128B_ChipShort_Reset(YM7128B_ChipShort* self);

void YM7128B_ChipShort_Start(YM7128B_ChipShort* self);

void YM7128B_ChipShort_Stop(YM7128B_ChipShort* self);

void YM7128B_ChipShort_Process(
    YM7128B_ChipShort* self,
    YM7128B_ChipShort_Process_Data* data
);

YM7128B_Register YM7128B_ChipShort_Read(
    YM7128B_ChipShort const* self,
    YM7128B_Address address
);

void YM7128B_ChipShort_Write(
    YM7128B_ChipShort* self,
    YM7128B_Address address,
    YM7128B_Register data
);

void YM7128B_ChipShort_Setup(
    YM7128B_ChipShort* self,
    YM7128B_TapIdeal sample_rate
);

// ============================================================================

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // !_YM7128B_EMU_H_
