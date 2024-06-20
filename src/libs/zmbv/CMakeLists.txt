add_library(zmbv STATIC zmbv.cpp)

pkg_check_modules(ZLIB_NG REQUIRED IMPORTED_TARGET zlib-ng)

target_include_directories(zmbv PUBLIC ..)
target_link_libraries(zmbv PRIVATE PkgConfig::ZLIB_NG)
