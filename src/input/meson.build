istream_lib = static_library(
    'istream',
    [
        'myctype.cpp',
        'quote.cpp',
        'ioutil.cpp',
        'url_quote.cpp',
        'url_scheme.cpp',
        'url.cpp',
        'url_decode.cpp',
    ],
)

ws2_dep = cpp.find_library('ws2_32', required: true)

istream_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: istream_lib,
    dependencies: [ws2_dep],
)
