# Adapted from the CMake Cookbook by Radovan Bast and Roberto Di Remigio.
# Ref: https://github.com/dev-cafe/cmake-cookbook/tree/master/chapter-06/recipe-07

find_package(Git QUIET)
message("****** GET_VERSION 1")

set(BUILD_GIT_HASH "unknown")

if(Git_FOUND)
  message("****** GET_VERSION 3")
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short=5 HEAD
    OUTPUT_VARIABLE BUILD_GIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
	)

  message("****** GET_VERSION 4")
	execute_process(
		COMMAND ${GIT_EXECUTABLE} status --porcelain
		OUTPUT_VARIABLE GIT_STATUS_OUTPUT
		OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
	)

	if("${GIT_STATUS_OUTPUT}" STREQUAL "")
    set(BUILD_GIT_DIRTY "")
	else()
    set(BUILD_GIT_DIRTY "-dirty")
	endif()
endif()

message("****** GET_VERSION: BUILD_GIT_HASH  ${BUILD_GIT_HASH}")
message("****** GET_VERSION: BUILD_GIT_DIRTY ${BUILD_GIT_DIRTY}")

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/src/version.h.in
  ${TARGET_DIR}/version.h
)

message("****** GET_VERSION 2")
