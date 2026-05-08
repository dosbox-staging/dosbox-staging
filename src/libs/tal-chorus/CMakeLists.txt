add_library(libtalchorus STATIC Lfo.cpp)

target_include_directories(libtalchorus PUBLIC ..)
target_link_libraries(libtalchorus PRIVATE project_headers)
