add_library(residfp STATIC
  resample/SincResampler.cpp
  Dac.cpp
  EnvelopeGenerator.cpp
  ExternalFilter.cpp
  Filter.cpp
  Filter6581.cpp
  Filter8580.cpp
  FilterModelConfig.cpp
  FilterModelConfig6581.cpp
  FilterModelConfig8580.cpp
  OpAmp.cpp
  SID.cpp
  Spline.cpp
  WaveformCalculator.cpp
  WaveformGenerator.cpp
)

# Integrator6581.cpp and Integrator8580.cpp are intentionally omitted.
#
# With RESID_INLINING=1 (hardcoded in siddefs-fp.h) the file bodies are fully
# inlined into the headers, so they would compile to empty translation units.
# This results in linker warnings on at least macOS.

target_include_directories(residfp SYSTEM PUBLIC ..)
target_link_libraries(residfp PRIVATE project_headers)

disable_warnings(residfp)
