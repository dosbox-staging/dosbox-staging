# Enable Loguru stack traces if supported
stacktrace_headers = ['cxxabi.h', 'dlfcn.h', 'execinfo.h']

has_stacktrace_headers = true

foreach header : stacktrace_headers
    if not cxx.has_header(header)
        has_stacktrace_headers = false
    endif
endforeach

# Some platforms might have the headers but not all of the
# functions needed for Loguru's stacktracing (like FreeBSD).
#
has_stacktrace_functions = cc.has_function(
    'backtrace',
    prefix: '#include <execinfo.h>'
)

if has_stacktrace_headers and has_stacktrace_functions
    add_project_arguments('-DLOGURU_STACKTRACES=1', language: 'cpp')
    add_project_link_arguments('-rdynamic', language: 'cpp')
endif

# Prevent loguru from parsing command-line arguments with
# the host's locale-applied, because this can foul up
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
