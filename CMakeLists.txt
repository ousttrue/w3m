cmake_minimum_required(VERSION 3.20.0)
project(w3m VERSION 0.5.6)

# for compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# for build/.gitignore
if(NOT PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/.gitignore CONTENT "*")
endif()

add_compile_options(
  -D_DEFAULT_SOURCE
  -std=c23
  -Wall
  -Wcast-qual
  -Werror=vla
  -Wimplicit-function-declaration
  -Wimplicit-int
  -Wnull-dereference
  -Wold-style-definition
  -Wredundant-decls
  -Wstrict-prototypes
  -Wstring-compare
  -Wtype-limits
  -Wuninitialized
  -Wunused
  -g
  -O2)

add_library(
  indep
  STATIC
    Str.c
    indep.c
    regex.c
    textlist.c
    parsetag.c
    myctype.c
    hash.c)

subdirs(libwc)

# target
set(TARGET_NAME ${PROJECT_NAME})
add_executable(
  ${TARGET_NAME}
  main.c
  keybind.c
  file.c
  line.c
  buffer.c
  display.c
  etc.c
  str_const.c
  str_gc.c
  auth.c
  html_parser.c
  search.c
  linein.c
  table.c
  local.c
  form.c
  map.c
  frame.c
  rc.c
  menu.c
  mailcap.c
  image.c
  symbol.c
  entity.c
  terms.c
  url.c
  ftp.c
  mimehead.c
  regex.c
  news.c
  func.c
  cookie.c
  history.c
  backend.c
  util.c #
  anchor.c
  parsetagx.c
  tagtable.c
  istream.c)
target_link_libraries(${TARGET_NAME} PRIVATE indep wc gc ssl crypto tinfo m)

target_compile_definitions(${TARGET_NAME} PRIVATE _GNU_SOURCE)

# mktable
add_executable(mktable mktable.c entity.c Str.c hash.c myctype.c)
target_compile_definitions(mktable PRIVATE DUMMY)
target_link_libraries(mktable PRIVATE indep wc gc)

# w3mbookmark
add_executable(w3mbookmark w3mbookmark.c entity.c)
target_compile_definitions(w3mbookmark PRIVATE DUMMY)
target_link_libraries(w3mbookmark PRIVATE indep wc gc)

# w3mhelperpanel
add_executable(w3mhelperpanel w3mhelperpanel.c entity.c)
target_compile_definitions(w3mhelperpanel PRIVATE DUMMY)
target_link_libraries(w3mhelperpanel PRIVATE indep wc gc)


