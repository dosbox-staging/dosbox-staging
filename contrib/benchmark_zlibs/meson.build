project(
    'benchmark_zlibs',
    'cpp',
    license: 'GPL-2.0-or-later',
    meson_version: '>= 1.3.0',
    default_options: [
        'cpp_std=c++20',
        'buildtype=release',
        'b_ndebug=if-release',
        'b_staticpic=false',
        'b_pie=false',
        'warning_level=3',
        'zlib-ng:c_std=c11',
    ],
)

# zlib
zlib_dep = dependency('zlib')

# zlib-ng
cmake_bin = find_program('cmake', required: false)
cmake_module = import('cmake', required: false)
cmake_options = cmake_module.subproject_options()

zlib_ng_options = get_option('use_zlib_ng')
zlib_ng_is_native = zlib_ng_options.contains('native')

zlib_ng_defines = {
    'ZLIB_COMPAT': true,
    'WITH_OPTIM': true,
    'ZLIB_BUILD_STATIC': true,
    'PIC': get_option('b_staticpic'),
    'BUILD_SHARED_LIBS': false,
    'WITH_GTEST': false,
    'ZLIB_ENABLE_TESTS': false,
    'WITH_NATIVE_INSTRUCTIONS': zlib_ng_is_native,
    'WITH_SANITIZER': get_option('b_sanitize'),
}

foreach instruction_set : [
    'avx2',
    'avx512',
    'avx512vnni',
    'sse2',
    'ssse3',
    'sse42',
    'pclmulqdq',
    'vpclmulqdq',
    'acle',
    'neon',
    'armv6',
    'altivec',
    'power8',
    'rvv',
    'crc32_vx',
    'dfltcc_deflate',
    'dfltcc_inflate',
]
    cmake_define_key = 'WITH_' + instruction_set.to_upper()
    cmake_define_value = (
        zlib_ng_is_native
        or zlib_ng_options.contains(instruction_set)
    )
    zlib_ng_defines += {cmake_define_key: cmake_define_value}
endforeach

cmake_options.add_cmake_defines(zlib_ng_defines)

zlib_ng_subproject = cmake_module.subproject(
    'zlib-ng',
    options: cmake_options,
)
zlib_ng_dep = zlib_ng_subproject.get_variable('zlib_dep')

executable(
    'benchmark_zlib',
    'src/main.cpp',
    dependencies: zlib_dep,
)

executable(
    'benchmark_zlib_ng',
    'src/main.cpp',
    dependencies: zlib_ng_dep,
)
