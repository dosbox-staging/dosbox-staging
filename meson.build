project('dosbox-staging',
        meson_version : '>= 0.55.0')

cmake = import('cmake')

munt_options = cmake.subproject_options()

munt_options.add_cmake_defines({
	'BUILD_SHARED_LIBS' : 0,
	'CMAKE_BUILD_TYPE' : 'Release',
	'libmt32emu_C_INTERFACE' : 1,
	'libmt32emu_REQUIRE_ANSI' : 0,
	'libmt32emu_SHARED' : 0,
	'libmt32emu_WITH_INTERNAL_RESAMPLER' : 0,
	'munt_WITH_MT32EMU_QT' : 0,
	'munt_WITH_MT32EMU_SMF2WAV' : 0})

munt_subproject = cmake.subproject(
	'munt',
	options : munt_options)
