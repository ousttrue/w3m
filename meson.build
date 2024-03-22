project(
    'w3m',
    ['c', 'cpp'],
    version: '0.5.3',
    default_options: [
        'cpp_std=c++20',
    ],
)

add_project_arguments(
    '-DNOMINMAX',
    '-D_USE_MATH_DEFINES',
    language: 'cpp',
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

gc_dep = dependency('gc', default_options: ['default_library=static'])
ssl_dep = subproject(
    'openssl',
    default_options: ['default_library=static'],
).get_variable('openssl_dep')
nkf_dep = dependency('nkf')
lua_dep = dependency('lua')
libuv_dep = dependency('libuv', default_options: ['default_library=static'])
zlib_dep = dependency('zlib', default_options: ['default_library=static'])
screen_dep = dependency('ftxui-screen')
dom_dep = dependency('ftxui-dom')
component_dep = dependency('ftxui-component')
deps = [
    gc_dep,
    ssl_dep,
    nkf_dep,
    lua_dep,
    libuv_dep,
    zlib_dep,
    # ftxui
    screen_dep,
    dom_dep,
    component_dep,
]
if host_machine.system() != 'windows'
    m_dep = cc.find_library('m', required: false)
    deps += [
        m_dep,
    ]
endif

subdir('without_gc')
subdir('with_gc')

executable(
    'w3m',
    'main.cpp',
    install: true,
    dependencies: [
        without_gc_dep,
        with_gc_dep,
    ],
)

subdir('tests')
subdir('tui_sample')
