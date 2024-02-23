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
gc_dep = dependency('gc', default_options: ['default_library=static'])

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

executable(
    'co_test',
    'co_test.cpp',
    install: true,
    dependencies: [libuv_dep],
)
