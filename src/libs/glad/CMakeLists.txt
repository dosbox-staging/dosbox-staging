if (APPLE)
  add_compile_options("-Wno-cast-function-type-mismatch")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_library(glad STATIC debug/src/gl.c)
  target_include_directories(glad PUBLIC debug/include)

else() # Release
  add_library(glad STATIC release/src/gl.c)
  target_include_directories(glad PUBLIC release/include)

endif()
