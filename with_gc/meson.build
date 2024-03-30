defs = [
    '-DCGIBIN_DIR="' + CGIBIN_DIR + '"',
    '-DHELP_DIR="' + HELP_DIR + '"',
    '-DETC_DIR="' + ETC_DIR + '"',
    '-DCONF_DIR="' + CONF_DIR + '"',
    '-DRC_DIR="' + RC_DIR + '"',
    '-DLOCALEDIR="' + localedir + '"',
    '-DWIN32_LEAN_AND_MEAN=1',
    # '-w',
]

with_gc_lib = static_library(
    'with_gc',
    [
        'html/readbuffer.cpp',
        'html/readtoken.cpp',
        'html/html_feed_env.cpp',
        'html/table.cpp',
        'html/html_parser.cpp',
        'Str.cpp',
        'hash.cpp',
        'buffer.cpp',
        'app.cpp',
        'alloc.cpp',
        'tabbuffer.cpp',
        'proc.cpp',
        'push_symbol.cpp',
        'option.cpp',
        'rc.cpp',
    ],
    dependencies: deps + [without_gc_dep],
    c_args: defs,
    cpp_args: defs,
)
with_gc_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: with_gc_lib,
    dependencies: deps,
)
