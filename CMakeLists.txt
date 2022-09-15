# project
cmake_minimum_required(VERSION 3.20.0)
project(w3m VERSION 0.5.3)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/bin)

set(TARGET_NAME wc)
add_library(
  ${TARGET_NAME}
  libwc/big5.c
  libwc/ces.c
  libwc/char_conv.c
  libwc/charset.c
  libwc/combining.c
  libwc/conv.c
  libwc/detect.c
  libwc/gb18030.c
  libwc/gbk.c
  libwc/hkscs.c
  libwc/hz.c
  libwc/iso2022.c
  libwc/jis.c
  libwc/johab.c
  libwc/priv.c
  libwc/putc.c
  libwc/search.c
  libwc/sjis.c
  libwc/status.c
  libwc/ucs.c
  libwc/uhc.c
  libwc/utf7.c
  libwc/utf8.c
  libwc/viet.c
  libwc/wtf.c)
target_include_directories(${TARGET_NAME} PUBLIC libwc
                                                 ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(
  ${TARGET_NAME}
  PUBLIC HAVE_CONFIG_H
         AUXBIN_DIR="${PREFIX}/libexec/w3m"
         CGIBIN_DIR="${PREFIX}/libexec/w3m/cgi-bin"
         HELP_DIR="${PREFIX}/share/w3m"
         ETC_DIR="${PREFIX}/etc"
         CONF_DIR="${PREFIX}/etc/w3m"
         RC_DIR="~/.w3m"
         LOCALEDIR="${PREFIX}/share/locale"
         USE_UNICODE)

set(TARGET_NAME indep)
add_library(
  ${TARGET_NAME}
  Str.c
  indep.cpp
  regex.c
  textlist.c
  parsetag.c
  myctype.c
  hash.cpp)
target_link_libraries(${TARGET_NAME} PUBLIC wc)

# w3m
set(PERFIX $ENV{HOME}/local)
set(TARGET_NAME w3m)
add_executable(
  ${TARGET_NAME}
  main.cpp
  Args.cpp
  App.cpp
  core.cpp
  defun.cpp
  file.cpp
  buffer.cpp
  display.cpp
  etc.cpp
  search.c
  linein.c
  table.c
  local.c
  form.cpp
  map.c
  frame.cpp
  rc.c
  menu.c
  mailcap.c
  image.cpp
  symbol.c
  entity.cpp
  terms.c
  url.c
  ftp.cpp
  mimehead.c
  news.c
  #
  func.cpp
  cookie.cpp
  history.cpp
  backend.cpp
  #
  keybind.cpp
  anchor.cpp
  parsetagx.c
  #
  tagtable.c
  istream.cpp
  version.c)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}
                                                  libwc)
target_link_libraries(
  ${TARGET_NAME}
  PRIVATE m
          nsl
          dl
          indep
          gc
          wc
          ssl
          crypto
          gpm
          tinfo)
target_compile_definitions(${TARGET_NAME} PRIVATE _main=main)
