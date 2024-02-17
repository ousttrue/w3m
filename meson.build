project(
    'w3m',
    ['c', 'cpp'],
    version: '0.5.3',
    default_options: [
        'cpp_std=c++23',
    ],
)

# gc_dep= dependency('gc')
cxx = meson.get_compiler('cpp')
if host_machine.system() == 'darwin'
    gc_inc = include_directories('/opt/homebrew/Cellar/bdw-gc/8.2.6/include/')
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
elif host_machine.system() == 'windows'
    gc_dep = dependency('gc')
else
    gccpp_lib = cxx.find_library(
        'gccpp',
    )
    gc_dep = declare_dependency(
        dependencies: [dependency('bdw-gc'), gccpp_lib],
    )
endif

if host_machine.system() == 'windows'
    ssl_dep = dependency('openssl')
else
    ssl_dep = dependency('openssl')
endif

nkf_dep = dependency('nkf')
lua_dep = dependency('lua')
libuv_dep = dependency('libuv')

screen_dep = dependency('ftxui-screen')
# dom_dep = dependency('ftxui-dom')
# component_dep = dependency('ftxui-component')

subdir('src/input')

deps = [
    gc_dep,
    ssl_dep,
    istream_dep,
    nkf_dep,
    lua_dep,
    libuv_dep,
    screen_dep,
]

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

if host_machine.system() != 'windows'
    # ncurses_dep = dependency('ncurses')
    # deps += ncurses_dep
    deps += (
        m_dep # , wc_dep
    )
endif

subdir('src')
subdir('tests')
