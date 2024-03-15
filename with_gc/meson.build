PACKAGE = meson.project_name()
prefix = get_option('prefix')
exec_prefix = prefix
datarootdir = prefix + '/share'
datadir = datarootdir
localedir = datadir + '/locale'
libexecdir = exec_prefix + '/libexec'
sysconfdir = prefix + '/etc'

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
    '-DWIN32_LEAN_AND_MEAN=1',
    # '-w',
]

with_gc_lib = static_library(
    'with_gc',
    [
        'html/readbuffer.cpp',
        'html/html_feed_env.cpp',
        'html/table.cpp',
        'html/form.cpp',
        'html/form_item.cpp',
        'html/anchor.cpp',
        'html/html_tag.cpp',
        'html/tagtable.cpp',
        'html/html_parser.cpp',
        'html/html_quote.cpp',
        'Str.cpp',
        'regex.cpp',
        'generallist.cpp',
        'textline.cpp',
        'keyvalue.cpp',
        'hash.cpp',
        'buffer.cpp',
        'etc.cpp',
        'search.cpp',
        'linein.cpp',
        'local_cgi.cpp',
        'rc.cpp',
        'mailcap.cpp',
        'symbol.cpp',
        'mimehead.cpp',
        'cookie.cpp',
        'history.cpp',
        'istream.cpp',
        'version.cpp',
        'downloadlist.cpp',
        'content.cpp',
        'ssl_util.cpp',
        'mimetypes.cpp',
        'url_stream.cpp',
        'compression.cpp',
        'mytime.cpp',
        'app.cpp',
        'http_auth.cpp',
        'http_request.cpp',
        'http_response.cpp',
        'http_session.cpp',
        'line.cpp',
        'matchattr.cpp',
        'auth_pass.cpp',
        'auth_digest.cpp',
        'loaddirectory.cpp',
        'alloc.cpp',
        'tabbuffer.cpp',
        'proc.cpp',
        'line_layout.cpp',
        'line_data.cpp',
        'file_util.cpp',
        'dict.cpp',
        'matrix.cpp',
        'settings.cpp',
    ],
    dependencies: deps + [without_gc_dep],
    c_args: defs,
    cpp_args: defs,
)
with_gc_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: with_gc_lib,
    dependencies: deps,
)
