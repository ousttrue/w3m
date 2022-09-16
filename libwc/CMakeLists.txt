set(TARGET_NAME wc)
add_library(
  ${TARGET_NAME}
  big5.cpp
  ces.cpp
  char_conv.cpp
  charset.c
  combining.c
  conv.c
  detect.c
  gb18030.c
  gbk.c
  hkscs.c
  hz.c
  iso2022.c
  jis.c
  johab.c
  priv.c
  putc.c
  search.c
  sjis.c
  status.c
  ucs.c
  uhc.c
  utf7.c
  utf8.c
  viet.c
  wtf.c
  Str.cpp
  myctype.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(${TARGET_NAME} PUBLIC HAVE_CONFIG_H USE_UNICODE)
