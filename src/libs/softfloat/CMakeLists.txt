set(softfloat_sources
  s_countLeadingZeros8.c
  s_approxRecip_1Ks.c
  s_approxRecipSqrt_1Ks.c
  extF80_add.c
  extF80_div.c
  extF80_eq.c
  extF80_eq_signaling.c
  extF80_isSignalingNaN.c
  extF80_le.c
  extF80_lt.c
  extF80_mul.c
  extF80_rem.c
  extF80_roundToInt.c
  extF80_sqrt.c
  extF80_sub.c
  extF80_to_f128.c
  extF80_to_f32.c
  extF80_to_f64.c
  extF80_to_i32.c
  extF80_to_i64.c
  extF80_to_ui64.c
  f128_add.c
  f128_div.c
  f128_eq.c
  f128_lt.c
  f128_mul.c
  f128_rem.c
  f128_roundToInt.c
  f128_sqrt.c
  f128_sub.c
  f128_to_extF80.c
  f128_to_i64.c
  f32_to_extF80.c
  f64_to_extF80.c
  i32_to_extF80.c
  i64_to_extF80.c
  i64_to_f128.c
  s_add128.c
  s_addMagsExtF80.c
  s_addMagsF128.c
  s_approxRecip32_1.c
  s_approxRecipSqrt32_1.c
  s_commonNaNToExtF80UI.c
  s_commonNaNToF128UI.c
  s_commonNaNToF32UI.c
  s_commonNaNToF64UI.c
  s_eq128.c
  s_extF80UIToCommonNaN.c
  s_f128UIToCommonNaN.c
  s_f32UIToCommonNaN.c
  s_f64UIToCommonNaN.c
  s_le128.c
  s_lt128.c
  s_normRoundPackToExtF80.c
  s_normRoundPackToF128.c
  s_normSubnormalExtF80Sig.c
  s_normSubnormalF128Sig.c
  s_normSubnormalF32Sig.c
  s_normSubnormalF64Sig.c
  s_propagateNaNExtF80UI.c
  s_propagateNaNF128UI.c
  s_roundPackToExtF80.c
  s_roundPackToF128.c
  s_roundPackToF32.c
  s_roundPackToF64.c
  s_roundToI32.c
  s_roundToI64.c
  s_roundToUI64.c
  s_shiftRightJam128.c
  s_shiftRightJam128Extra.c
  s_shiftRightJam32.c
  s_shiftRightJam64.c
  s_shiftRightJam64Extra.c
  s_shortShiftLeft128.c
  s_shortShiftRight128.c
  s_shortShiftRightJam128Extra.c
  s_shortShiftRightJam64.c
  s_shortShiftRightJam64Extra.c
  s_sub128.c
  s_subMagsExtF80.c
  s_subMagsF128.c
  softfloat_raiseFlags.c
  softfloat_state.c
  ui64_to_extF80.c
)

# These files provide fallback implementations. opts-GCC.h supplies inline
# versions when the corresponding compiler intrinsics are enabled.
if (NOT SOFTFLOAT_USE_BUILTIN_CLZ)
  list(APPEND softfloat_sources
    s_countLeadingZeros32.c
    s_countLeadingZeros64.c
  )
endif()
if (NOT SOFTFLOAT_USE_INTRINSIC_INT128)
  list(APPEND softfloat_sources
    s_mul128By32.c
    s_mul128To256M.c
    s_mul64ByShifted32To128.c
    s_mul64To128.c
  )
endif()

add_library(softfloat STATIC ${softfloat_sources})
target_include_directories(softfloat SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(softfloat PUBLIC SOFTFLOAT_FAST_INT64)
set_target_properties(softfloat PROPERTIES C_STANDARD 99)
disable_warnings(softfloat)
