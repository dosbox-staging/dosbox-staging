if(APPLE)
  add_compile_options("-Wno-cast-function-type-mismatch")
endif()

add_library(libglad STATIC
  src/gl.c
)

target_include_directories(libglad PUBLIC include)
