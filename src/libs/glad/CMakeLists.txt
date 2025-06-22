add_library(libglad STATIC
  src/gl.c
  src/gles2.c
)

target_include_directories(libglad PUBLIC include)
