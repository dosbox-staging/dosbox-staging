add_library(cqm STATIC cqm.c)

target_include_directories(cqm SYSTEM PUBLIC ..)

disable_warnings(cqm)
