add_library(loguru STATIC loguru.cpp)

set(STACK_INCLUDES cxxabi.h dlfcn.h execinfo.h)

check_include_files("${STACK_INCLUDES}" HAS_STACK_INCLUDES LANGUAGE CXX)

if (HAS_STACK_INCLUDES)
	target_compile_definitions(loguru PRIVATE LOGURU_STACKTRACES=1)
	target_link_options(dosbox PRIVATE "-rdynamic")
endif()

if (NOT MSVC)
	set_source_files_properties(loguru.cpp
                                    PROPERTIES COMPILE_OPTIONS
                                    "-Wno-duplicated-branches")
endif()
