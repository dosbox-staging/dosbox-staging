libiir1_sources = files([
  'iir/Biquad.cpp',
  'iir/Butterworth.cpp',
  'iir/Cascade.cpp',
  'iir/ChebyshevI.cpp',
  'iir/ChebyshevII.cpp',
  'iir/Custom.cpp',
  'iir/PoleFilter.cpp',
  'iir/RBJ.cpp',
])

libiir1 = static_library('iir1', libiir1_sources)

libiir1_dep  = declare_dependency(link_with : libiir1)

