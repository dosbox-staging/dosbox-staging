add_library(ym7128bemu STATIC YM7128B_emu.c)

target_include_directories(ym7128bemu PUBLIC .)
