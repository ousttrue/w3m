subdir('input')

# SHELL=/bin/bash
PACKAGE = meson.project_name()
# VERSION = 0.5.3
# DOMAIN = $(PACKAGE)
prefix = get_option('prefix')
exec_prefix = prefix
datarootdir = prefix + '/share'
# bindir = ${exec_prefix}/bin
datadir = datarootdir
localedir = datadir + '/locale'
# libdir = ${exec_prefix}/lib
# includedir = ${prefix}/include
# infodir = ${datarootdir}/info
libexecdir = exec_prefix + '/libexec'
# localstatedir = ${prefix}/var
# mandir = ${datarootdir}/man
# oldincludedir = /usr/include
# sbindir = ${exec_prefix}/sbin
# sharedstatedir = ${prefix}/com
# srcdir = .
sysconfdir = prefix + '/etc'
# top_srcdir = .
# top_builddir = .
# VPATH = $(top_srcdir):.
#
CGIBIN_DIR = libexecdir + '/' + PACKAGE + '/cgi-bin'
AUXBIN_DIR = libexecdir + '/' + PACKAGE
HELP_DIR = datarootdir + '/w3m'
RC_DIR = '~/.w3m'
ETC_DIR = sysconfdir
CONF_DIR = sysconfdir + '/' + PACKAGE

defs = [
    '-DAUXBIN_DIR="' + AUXBIN_DIR + '"',
    '-DCGIBIN_DIR="' + CGIBIN_DIR + '"',
    '-DHELP_DIR="' + HELP_DIR + '"',
    '-DETC_DIR="' + ETC_DIR + '"',
    '-DCONF_DIR="' + CONF_DIR + '"',
    '-DRC_DIR="' + RC_DIR + '"',
    '-DLOCALEDIR="' + localedir + '"',
    # '-w',
]

# use generated config.h by configure

# conf_data = configuration_data()
# conf_data.set('DISPLAY_CHARSET', 'WC_CES_UTF_8')
# conf_data.set('SYSTEM_CHARSET', 'WC_CES_UTF_8')
# conf_data.set('DOCUMENT_CHARSET', 'WC_CES_UTF_8')
# conf_data.set('W3M_LANG', 'EN')
# conf_data.set('DEF_CAFILE', '')
# conf_data.set('DEF_EDITOR', '/usr/bin/vi')
# conf_data.set('DEF_MAILER', '/usr/bin/mail')
# conf_data.set('DEF_EXT_BROWSER', '/usr/bin/firefox')
# conf_data.set('DEF_MIGEMO_COMMAND', '')
# conf_data.set('RETSIGTYPE', 'void')
# conf_data.set('SIGNAL_RETURN', 'return')
#
# configure_file(
#     input: 'config.h.in',
#     output: 'config.h',
#     configuration: conf_data,
# )

executable(
    'w3m',
    [
        'w3m.cpp',
        'main.cpp',
        'tmpfile.cpp',
        'readbuffer.cpp',
        'keybind.cpp',
        'signal_util.cpp',
        'Str.cpp',
        'regex.cpp',
        'textlist.cpp',
        'keyvalue.cpp',
        'hash.cpp',
        'buffer.cpp',
        'display.cpp',
        'etc.cpp',
        'search.cpp',
        'linein.cpp',
        'table.cpp',
        'local_cgi.cpp',
        'form.cpp',
        'rc.cpp',
        'mailcap.cpp',
        'symbol.cpp',
        'entity.cpp',
        'terms.cpp',
        'mimehead.cpp',
        'func.cpp',
        'cookie.cpp',
        'history.cpp',
        'util.cpp',
        'anchor.cpp',
        'htmltag.cpp',
        'tagtable.cpp',
        'istream.cpp',
        'version.cpp',
        'utf8.cpp',
        'downloadlist.cpp',
        'screen.cpp',
        'message.cpp',
        'ssl_util.cpp',
        'mimetypes.cpp',
        'url_stream.cpp',
        'compression.cpp',
        'mytime.cpp',
        'funcname.cpp',
        'app.cpp',
        'bufferpos.cpp',
        'http_auth.cpp',
        'http_request.cpp',
        'http_response.cpp',
        'http_session.cpp',
        'line.cpp',
        'lineprop.cpp',
        'matchattr.cpp',
        'auth_pass.cpp',
        'auth_digest.cpp',
        'loaddirectory.cpp',
        'alloc.cpp',
        'tabbuffer.cpp',
        'proc.cpp',
        'line_layout.cpp',
    ],
    install: true,
    dependencies: [
        gc_dep,
        ssl_dep,
        ncurses_dep,
        m_dep, # , wc_dep
        istream_dep,
        nkf_dep,
    ],
    c_args: defs,
    cpp_args: defs,
    # link_args: ['-lgccpp'],
)
