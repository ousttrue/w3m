# https://www.servernote.net/article.cgi?id=make-libnkf-for-c-cpp
project(
    'nkf',
    ['c'],
    version: '2.1.5',
)

nkf_inc = include_directories('.')
nkf_lib = static_library(
    'nkf',
    'libnkf.c',
    include_directories: nkf_inc,
)

nkf_dep = declare_dependency(
    include_directories: nkf_inc,
    link_with: nkf_lib,
)
