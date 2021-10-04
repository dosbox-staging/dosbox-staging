libloguru = static_library('loguru',
                           'loguru.cpp',
                           dependencies : [ threads_dep, dl_dep ])

libloguru_dep = declare_dependency(link_with : libloguru,
                                   include_directories : '.')
