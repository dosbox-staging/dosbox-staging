add_library(esfmu STATIC
  esfm.c
  esfm_registers.c
)

target_include_directories(esfmu SYSTEM PUBLIC ..)
