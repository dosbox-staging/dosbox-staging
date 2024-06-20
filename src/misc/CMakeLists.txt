add_library(libmisc STATIC
		ansi_code_markup.cpp
		cross.cpp
		ethernet.cpp
		ethernet_slirp.cpp
		fs_utils.cpp
		fs_utils_posix.cpp
		fs_utils_win32.cpp
		help_util.cpp
		messages.cpp
		pacer.cpp
		programs.cpp
		rwqueue.cpp
		setup.cpp
		string_utils.cpp
		support.cpp
		unicode.cpp
)

target_link_libraries(libmisc PRIVATE libwhereami $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)
