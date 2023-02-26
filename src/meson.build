prefix = get_option('prefix')
openssl_dep = dependency('openssl')
gc_dep = dependency('bdw-gc')
ncurses_dep = dependency('ncurses')
asio_dep = dependency('asio')
plog_dep = dependency('plog')
executable(
    'w3m',
    [
        'main.cpp',
        'Args.cpp',
        'App.cpp',
        'core.cpp',
        'defun.cpp',
        'file.cpp',
        'buffer.cpp',
        'display.cpp',
        'etc.cpp',
        'search.cpp',
        'linein.cpp',
        'table.cpp',
        'local.cpp',
        'form.cpp',
        'map.cpp',
        'frame.cpp',
        'rc.cpp',
        'menu.cpp',
        'mailcap.cpp',
        'image.cpp',
        'symbol.cpp',
        'entity.cpp',
        'terms.cpp',
        'url.cpp',
        'ftp.cpp',
        'mimehead.cpp',
        #
        'func.cpp',
        'cookie.cpp',
        'history.cpp',
        'backend.cpp',
        #
        'keybind.cpp',
        'anchor.cpp',
        'parsetagx.cpp',
        #
        'tagtable.cpp',
        'istream.cpp',
        'version.c',
        #
        'indep.cpp',
        'regex.cpp',
        'textlist.cpp',
        'parsetag.cpp',
        'hash.cpp',
        #
        'tty.cpp',
    ],
    install: true,
    cpp_args: [
        '-DAUXBIN_DIR="' + prefix + '/libexec/w3m"',
        '-DCGIBIN_DIR="' + prefix + '/libexec/w3m/cgi-bin"',
        '-DHELP_DIR="' + prefix + '/share/w3m"',
        '-DETC_DIR="' + prefix + '/etc"',
        '-DCONF_DIR="' + prefix + '/etc/w3m"',
        '-DRC_DIR="~/.w3m"',
        '-DLOCALEDIR="' + prefix + '/share/locale"',
    ],
    dependencies: [
        libwc_dep,
        openssl_dep,
        gc_dep,
        ncurses_dep,
        asio_dep,
        plog_dep,
    ],
)
