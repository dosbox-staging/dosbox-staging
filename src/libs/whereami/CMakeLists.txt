add_library(whereami STATIC whereami.c)

target_include_directories(whereami SYSTEM PUBLIC .)
