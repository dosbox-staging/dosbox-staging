target_sources(dosboxcommon PRIVATE
  callback.cpp
  core_dynrec.cpp
  core_full.cpp
  core_normal.cpp
  core_prefetch.cpp
  core_simple.cpp
  cpu.cpp
  cpu_snapshot.cpp
  flags.cpp
  mmx.cpp
  modrm.cpp
  paging.cpp)

# x86-only dynamic recompiler core. The file's entire body is gated by
# #if (C_DYNAMIC_X86); on ARM and other non-x86 targets it would compile
# to an empty translation unit and trigger libtool/ranlib "no symbols"
# warnings on macOS.
if (C_DYNAMIC_X86)
  target_sources(dosboxcommon PRIVATE core_dyn_x86.cpp)
endif()

target_link_libraries(dosboxcommon PRIVATE simde)
