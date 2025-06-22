add_library(libglad STATIC
  src/gl.c
)

target_include_directories(libglad PUBLIC include)
