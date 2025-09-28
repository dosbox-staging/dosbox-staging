configure_file(winres.rc.in winres.rc @ONLY)

target_sources(dosbox PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}/winres.rc
)
