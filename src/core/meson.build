core_lib = static_library(
    'core',
    [
        'cookie_domain.cpp',
        'cookie.cpp',
        'ioutil.cpp',
        'utf8.cpp',
        'myctype.cpp',
        'cmp.cpp',
        'quote.cpp',
        'url_scheme.cpp',
        'url_quote.cpp',
        'url.cpp',
        'entity.cpp',
        'tmpfile.cpp',
        'url_decode.cpp',
        'dbgstream.cpp',
    ],
)
core_dep = declare_dependency(
    link_with: core_lib,
    include_directories: include_directories('.'),
)
