add_library(riffcpp STATIC
  membuffer.cpp
  riffcpp.cpp
)

target_include_directories(riffcpp SYSTEM PUBLIC .)
