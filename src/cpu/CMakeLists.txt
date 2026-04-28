target_sources(libdosboxcommon PRIVATE 
  callback.cpp
  core_dynrec.cpp
  core_full.cpp
  core_normal.cpp
  core_prefetch.cpp
  core_simple.cpp
  cpu.cpp
  flags.cpp
  mmx.cpp
  modrm.cpp
  paging.cpp)

if (C_DYNAMIC_X86)
  target_sources(libdosboxcommon PRIVATE core_dyn_x86.cpp)
endif()

target_link_libraries(libdosboxcommon PRIVATE simde)
