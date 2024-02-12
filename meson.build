project(
    'w3m',
    ['c', 'cpp'],
    version: '0.5.3',
    default_options: [
        'cpp_std=c++23',
    ],
)

# gc_dep= dependency('gc')
if host_machine.system() == 'darwin'
    gc_inc = include_directories('/opt/homebrew/Cellar/bdw-gc/8.2.6/include/')
    cxx = meson.get_compiler('cpp')
    gc_lib = cxx.find_library(
        'gc',
        dirs: '/opt/homebrew/Cellar/bdw-gc/8.2.6/lib',
    )
    gccpp_lib = cxx.find_library(
        'gccpp',
        dirs: '/opt/homebrew/Cellar/bdw-gc/8.2.6/lib',
    )
    gc_dep = declare_dependency(
        include_directories: gc_inc,
        dependencies: [gc_lib, gccpp_lib],
    )
else
    gc_dep = dependency('bdwgc')
endif

ssl_dep = dependency('openssl')
ncurses_dep = dependency('ncurses')
nkf_dep = dependency('nkf')
lua_dep = dependency('lua')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

subdir('src')
subdir('tests')
