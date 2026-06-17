# Suppress all compiler warnings for vendored third-party library targets.
#
# Third-party headers are already marked as SYSTEM includes, which tells the
# compiler to suppress warnings when *project code* includes them. But that
# doesn't help when compiling the library's own source files — the compiler
# finds headers locally (not through -isystem) and applies our strict warning
# flags (-Wall -Wextra -Weffc++ etc.) to code we don't maintain.
#
# Adding -w (GCC/Clang) or /w (MSVC) to each vendored target suppresses all
# warnings during their own compilation, complementing the SYSTEM approach.

function(disable_warnings target)
  target_compile_options(${target} PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>
  )
endfunction()

add_subdirectory(base64)
add_subdirectory(clap)
add_subdirectory(decoders)
add_subdirectory(enet)
add_subdirectory(ESFMu)
add_subdirectory(glad)
add_subdirectory(http)
add_subdirectory(json)
add_subdirectory(loguru)
add_subdirectory(manymouse)
add_subdirectory(mverb)
add_subdirectory(nuked)

if (OPT_DEBUGGER)
add_subdirectory(imgui)
endif()

add_subdirectory(PL_MPEG)
add_subdirectory(residfp)
add_subdirectory(riffcpp)
add_subdirectory(simde)
add_subdirectory(simpleini)
add_subdirectory(tal-chorus)
add_subdirectory(whereami)
add_subdirectory(YM7128B_emu)
