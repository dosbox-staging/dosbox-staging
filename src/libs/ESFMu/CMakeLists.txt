add_library(libesfmu STATIC
  esfm.c
  esfm_registers.c
)

target_include_directories(libesfmu PUBLIC ..)
