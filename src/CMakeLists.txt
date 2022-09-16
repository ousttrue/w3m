set(TARGET_NAME indep)
add_library(${TARGET_NAME} indep.cpp regex.cpp textlist.cpp parsetag.cpp
                           hash.cpp)
target_link_libraries(${TARGET_NAME} PUBLIC wc)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(
  ${TARGET_NAME}
  PUBLIC AUXBIN_DIR="${PREFIX}/libexec/w3m"
         CGIBIN_DIR="${PREFIX}/libexec/w3m/cgi-bin"
         HELP_DIR="${PREFIX}/share/w3m"
         ETC_DIR="${PREFIX}/etc"
         CONF_DIR="${PREFIX}/etc/w3m"
         RC_DIR="~/.w3m"
         LOCALEDIR="${PREFIX}/share/locale")

#
# w3m
#
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
  search.cpp
  linein.cpp
  table.cpp
  local.cpp
  form.cpp
  map.cpp
  frame.cpp
  rc.cpp
  menu.cpp
  mailcap.cpp
  image.cpp
  symbol.cpp
  entity.cpp
  terms.cpp
  url.cpp
  ftp.cpp
  mimehead.cpp
  #
  func.cpp
  cookie.cpp
  history.cpp
  backend.cpp
  #
  keybind.cpp
  anchor.cpp
  parsetagx.cpp
  #
  tagtable.cpp
  istream.cpp
  version.c)
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(
  ${TARGET_NAME}
  PRIVATE indep
          wc
          m
          nsl
          dl
          gc
          ssl
          crypto
          gpm
          tinfo)
target_compile_definitions(${TARGET_NAME} PRIVATE _main=main)
