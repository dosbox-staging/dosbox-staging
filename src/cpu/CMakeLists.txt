target_sources(dosboxcommon PRIVATE 
  callback.cpp
  core_dyn_x86.cpp
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

target_link_libraries(dosboxcommon PRIVATE simde)
