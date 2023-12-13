zlib_or_ng_dep = system_zlib_ng_dep.found() ? system_zlib_ng_dep : zlib_dep

libzmbv = static_library(
    'zmbv',
    'zmbv.cpp',
    include_directories: incdir,
    dependencies: [libmisc_dep, zlib_or_ng_dep]
)

libzmbv_dep = declare_dependency(link_with: libzmbv)
