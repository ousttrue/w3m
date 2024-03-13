project(
    'w3m',
    ['c', 'cpp'],
    version: '0.5.3',
    default_options: [
        'cpp_std=c++23',
    ],
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# gc_dep= dependency('gc')
cxx = meson.get_compiler('cpp')
gc_dep = dependency('gc', default_options: ['default_library=static'])
ssl_dep = dependency('openssl')
nkf_dep = dependency('nkf')
lua_dep = dependency('lua')
libuv_dep = dependency('libuv')
zlib_dep = dependency('zlib', default_options: ['default_library=static'])

screen_dep = dependency('ftxui-screen')
dom_dep = dependency('ftxui-dom')
component_dep = dependency('ftxui-component')

subdir('src/input')

deps = [
    gc_dep,
    ssl_dep,
    istream_dep,
    nkf_dep,
    lua_dep,
    libuv_dep,
    zlib_dep,
    #
    screen_dep,
    dom_dep,
    # component_dep,
]


if host_machine.system() != 'windows'
    m_dep = cc.find_library('m', required: false)
    deps += [
        m_dep,
    ]
endif

subdir('src')
subdir('tests')

executable(
    'co_test',
    'co_test.cpp',
    install: true,
    dependencies: [libuv_dep],
)

subdir('tui_sample')
