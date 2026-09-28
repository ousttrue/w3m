set(TARGET_NAME wc)
add_library(
  ${TARGET_NAME}
  STATIC
    big5.c
    ces.c
    char_conv.c
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
    wtf.c)
target_compile_definitions(${TARGET_NAME} PRIVATE _GNU_SOURCE USE_UNICODE)


