project(
    'w3m',
    ['c', 'cpp'],
    version: '0.5.3',
    default_options: [
        'cpp_std=c++23',
    ],
)

gc_dep = dependency('bdw-gc')
ssl_dep = dependency('openssl')
ncurses_dep = dependency('ncurses')
nkf_dep = dependency('nkf')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

subdir('src')
subdir('tests')
