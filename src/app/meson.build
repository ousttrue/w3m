defs = [
    '-DCGIBIN_DIR="' + CGIBIN_DIR + '"',
    '-DAUXBIN_DIR="' + AUXBIN_DIR + '"',
    '-DHELP_DIR="' + HELP_DIR + '"',
    '-DETC_DIR="' + ETC_DIR + '"',
    '-DCONF_DIR="' + CONF_DIR + '"',
    '-DRC_DIR="' + RC_DIR + '"',
    '-DLOCALEDIR="' + localedir + '"',
    '-DWIN32_LEAN_AND_MEAN=1',
    # '-w',
]

without_gc_lib = static_library(
    'without_gc_lib',
    [
        'option_param.cpp',
        'option.cpp',
        'line_layout.cpp',
        'event.cpp',
        'mimehead.cpp',
        'compression.cpp',
        'matchattr.cpp',
        'downloadlist.cpp',
        'auth_pass.cpp',
        'http_request.cpp',
        'mytime.cpp',
        'http_response.cpp',
        'istream.cpp',
        'opensocket.cpp',
        'http_auth.cpp',
        'local_cgi.cpp',
        'ssl_util.cpp',
        'url_stream.cpp',
        'http_session.cpp',
        'dict.cpp',
        'invoke.cpp',
        'etc.cpp',
        'file_util.cpp',
        'history.cpp',
        'loaddirectory.cpp',
        'mailcap.cpp',
        'search.cpp',
        'linein.cpp',
        'buffer.cpp',
        'app.cpp',
        'tabbuffer.cpp',
        'proc.cpp',
        'rc.cpp',
        'platform.cpp',
        'load_buffer.cpp',
    ],
    cpp_args: defs,
    dependencies: [
        deps,
        zlib_dep,
        # ftxui
        screen_dep,
        dom_dep,
        component_dep,
        #
        html_dep,
    ],
)

if host_machine.system() == 'windows'
    deps += cpp.find_library('ws2_32', required: true)
endif

without_gc_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: without_gc_lib,
    dependencies: deps + [html_dep],
)
