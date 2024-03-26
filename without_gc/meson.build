without_gc_lib = static_library(
    'without_gc_lib',
    [
        'stringtoken.cpp',
        'utf8.cpp',
        'myctype.cpp',
        'quote.cpp',
        'ioutil.cpp',
        'url_quote.cpp',
        'url_scheme.cpp',
        'url.cpp',
        'url_decode.cpp',
        'lineprop.cpp',
        'entity.cpp',
        'anchor.cpp',
        'line.cpp',
        'event.cpp',
        'tmpfile.cpp',
        'mimetypes.cpp',
        'compression.cpp',
        'cookie_domain.cpp',
        'cookie.cpp',
        'matchattr.cpp',
    ],
    cpp_args: [
        '-DAUXBIN_DIR="' + AUXBIN_DIR + '"',
    ],
    dependencies: [
        deps,
        zlib_dep,
        # ftxui
        screen_dep,
        dom_dep,
        component_dep,
    ],
)

deps = []
if host_machine.system() == 'windows'
    deps += cpp.find_library('ws2_32', required: true)
endif

without_gc_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: without_gc_lib,
    dependencies: deps,
)
