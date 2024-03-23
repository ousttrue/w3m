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
    ],
    dependencies: [
        screen_dep,
        dom_dep,
        component_dep,
    ],
)

ws2_dep = cpp.find_library('ws2_32', required: true)

without_gc_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: without_gc_lib,
    dependencies: [
        ws2_dep,
    ],
)
