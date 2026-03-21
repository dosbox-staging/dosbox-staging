add_library(libriffcpp STATIC
  membuffer.cpp
  riffcpp.cpp
)

target_include_directories(libriffcpp PUBLIC .)
