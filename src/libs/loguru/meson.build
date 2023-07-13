# Enable Loguru stack traces if supported
stacktrace_headers = ['cxxabi.h', 'dlfcn.h', 'execinfo.h']

all_stacktrace_headers_found = true

foreach header : stacktrace_headers
    if not cxx.has_header(header)
        all_stacktrace_headers_found = false
    endif
endforeach

if all_stacktrace_headers_found
    add_project_arguments('-DLOGURU_STACKTRACES=1', language: 'cpp')
    add_project_link_arguments('-rdynamic', language: 'cpp')
endif

# Prevent loguru from parsing command-line arguments with
# the hosts's locale-applied, because this can foul up
# ncurses. (DOSBox also doesn't have foreign-language arguments).
add_project_arguments('-DLOGURU_USE_LOCALE=0', language: 'cpp')

libloguru = static_library(
    'loguru',
    'loguru.cpp',
    dependencies: [threads_dep, dl_dep],
)

libloguru_dep = declare_dependency(
    link_with: libloguru,
    include_directories: '.',
)
