add_library(libym7128bemu STATIC YM7128B_emu.c)

target_include_directories(libym7128bemu PUBLIC .)
