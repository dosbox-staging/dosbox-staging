add_library(talchorus STATIC Lfo.cpp)

target_include_directories(talchorus PUBLIC ..)
target_link_libraries(talchorus PRIVATE project_headers)
