set(TARGET_NAME wc)
add_library(
  ${TARGET_NAME}
  big5.cpp
  ces.cpp
  char_conv.cpp
  charset.cpp
  combining.cpp
  conv.cpp
  detect.cpp
  gb18030.cpp
  gbk.cpp
  hkscs.cpp
  hz.cpp
  iso2022.cpp
  jis.cpp
  johab.cpp
  priv.cpp
  putc.cpp
  search.cpp
  sjis.cpp
  status.cpp
  ucs.cpp
  uhc.cpp
  utf7.cpp
  utf8.c
  viet.c
  wtf.c
  Str.cpp
  myctype.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(${TARGET_NAME} PUBLIC HAVE_CONFIG_H USE_UNICODE)
