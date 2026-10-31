target_sources(dosboxcommon PRIVATE fpu.cpp)

if (NOT C_FPU_X86)
  target_sources(dosboxcommon PRIVATE fpu_portable.cpp float80.cpp)
  target_link_libraries(dosboxcommon PUBLIC softfloat)
endif()
