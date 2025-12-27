if (APPLE)
  add_compile_options("-Wno-cast-function-type-mismatch")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_library(libglad STATIC debug/src/gl.c debug/src/gles2.c)
  target_include_directories(libglad PUBLIC debug/include)

else() # Release
  add_library(libglad STATIC release/src/gl.c release/src/gles2.c)
  target_include_directories(libglad PUBLIC release/include)

endif()
