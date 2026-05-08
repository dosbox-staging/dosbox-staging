add_library(zmbv STATIC zmbv.cpp)
target_include_directories(zmbv PUBLIC ..)
target_link_libraries(zmbv PRIVATE project_headers ${ZMBV_ZLIB_TARGET})
