istream_lib = static_library(
    'istream',
    [
        'myctype.cpp',
        'quote.cpp',
        'url_schema.cpp',
    ],
)
istream_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: istream_lib,
)
