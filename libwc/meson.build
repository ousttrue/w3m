wc_inc = include_directories('.')
wc_lib = static_library(
    'wc',
    [
        'big5.c',
        'ces.c',
        'char_conv.c',
        'charset.c',
        'combining.c',
        'conv.c',
        'detect.c',
        'gb18030.c',
        'gbk.c',
        'hkscs.c',
        'hz.c',
        'iso2022.c',
        'jis.c',
        'johab.c',
        'priv.c',
        'putc.c',
        'search.c',
        'sjis.c',
        'status.c',
        'ucs.c',
        'uhc.c',
        'utf7.c',
        'utf8.c',
        'viet.c',
        'wtf.c',
    ],
    include_directories: include_directories('..'),
    c_args: [
        '-DHAVE_CONFIG_H',
    ],
)
wc_dep = declare_dependency(
    link_with: wc_lib,
    include_directories: wc_inc,
)
