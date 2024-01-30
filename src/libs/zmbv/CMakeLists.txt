add_library(zmbv STATIC zmbv.cpp)

find_package(ZLIB REQUIRED)

target_include_directories(zmbv PUBLIC ..)
target_link_libraries(zmbv PRIVATE ZLIB::ZLIB)
