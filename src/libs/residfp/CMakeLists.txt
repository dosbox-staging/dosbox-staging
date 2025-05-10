add_library(libresidfp STATIC
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
  Integrator6581.cpp
  Integrator8580.cpp
  OpAmp.cpp
  SID.cpp
  Spline.cpp
  WaveformCalculator.cpp
  WaveformGenerator.cpp
)

target_include_directories(libresidfp PUBLIC ..)
