add_library(nuked STATIC opl3.c)

target_include_directories(nuked SYSTEM PUBLIC ..)

disable_warnings(nuked)
