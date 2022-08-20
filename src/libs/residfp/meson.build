libresidfp_sources = files(
    'resample/SincResampler.cpp',
    'Dac.cpp',
    'EnvelopeGenerator.cpp',
    'ExternalFilter.cpp',
    'Filter.cpp',
    'Filter6581.cpp',
    'Filter8580.cpp',
    'FilterModelConfig.cpp',
    'FilterModelConfig6581.cpp',
    'FilterModelConfig8580.cpp',
    'Integrator6581.cpp',
    'Integrator8580.cpp',
    'OpAmp.cpp',
    'SID.cpp',
    'Spline.cpp',
    'WaveformCalculator.cpp',
    'WaveformGenerator.cpp',
)

libresidfp = static_library('residfp', libresidfp_sources)

libresidfp_dep = declare_dependency(link_with: libresidfp)
