add_library(libcpu STATIC
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
  paging.cpp
)

target_link_libraries(libcpu PRIVATE
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  simde
)

if (C_DEBUG)
	target_link_libraries(libcpu PRIVATE libdebug)
endif()
