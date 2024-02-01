#pragma once
#include "config.h"
#include "url_schema.h"
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>

#include <time.h>

struct URLOption {
  char *referer;
  int flag;
};

union input_stream;
struct URLFile {
  UrlSchema schema;
  char is_cgi;
  char encoding;
  union input_stream *stream;
  char *ext;
  int compression;
  int content_encoding;
  const char *guess_type;
  char *ssl_certificate;
  char *url;
  time_t modtime;
};

#define CMP_NOCOMPRESS 0
#define CMP_COMPRESS 1
#define CMP_GZIP 2
#define CMP_BZIP2 3
#define CMP_DEFLATE 4
#define CMP_BROTLI 5

#define ENC_7BIT 0
#define ENC_BASE64 1
#define ENC_QUOTE 2
#define ENC_UUENCODE 3

/* Tag attribute */

#define ATTR_UNKNOWN 0
#define ATTR_ACCEPT 1
#define ATTR_ACCEPT_CHARSET 2
#define ATTR_ACTION 3
#define ATTR_ALIGN 4
#define ATTR_ALT 5
#define ATTR_ARCHIVE 6
#define ATTR_BACKGROUND 7
#define ATTR_BORDER 8
#define ATTR_CELLPADDING 9
#define ATTR_CELLSPACING 10
#define ATTR_CHARSET 11
#define ATTR_CHECKED 12
#define ATTR_COLS 13
#define ATTR_COLSPAN 14
#define ATTR_CONTENT 15
#define ATTR_ENCTYPE 16
#define ATTR_HEIGHT 17
#define ATTR_HREF 18
#define ATTR_HTTP_EQUIV 19
#define ATTR_ID 20
#define ATTR_LINK 21
#define ATTR_MAXLENGTH 22
#define ATTR_METHOD 23
#define ATTR_MULTIPLE 24
#define ATTR_NAME 25
#define ATTR_NOWRAP 26
#define ATTR_PROMPT 27
#define ATTR_ROWS 28
#define ATTR_ROWSPAN 29
#define ATTR_SIZE 30
#define ATTR_SRC 31
#define ATTR_TARGET 32
#define ATTR_TYPE 33
#define ATTR_USEMAP 34
#define ATTR_VALIGN 35
#define ATTR_VALUE 36
#define ATTR_VSPACE 37
#define ATTR_WIDTH 38
#define ATTR_COMPACT 39
#define ATTR_START 40
#define ATTR_SELECTED 41
#define ATTR_LABEL 42
#define ATTR_READONLY 43
#define ATTR_SHAPE 44
#define ATTR_COORDS 45
#define ATTR_ISMAP 46
#define ATTR_REL 47
#define ATTR_REV 48
#define ATTR_TITLE 49
#define ATTR_ACCESSKEY 50
#define ATTR_PUBLIC 51

/* Internal attribute */
#define ATTR_XOFFSET 60
#define ATTR_YOFFSET 61
#define ATTR_TOP_MARGIN 62
#define ATTR_BOTTOM_MARGIN 63
#define ATTR_TID 64
#define ATTR_FID 65
#define ATTR_FOR_TABLE 66
#define ATTR_FRAMENAME 67
#define ATTR_HBORDER 68
#define ATTR_HSEQ 69
#define ATTR_NO_EFFECT 70
#define ATTR_REFERER 71
#define ATTR_SELECTNUMBER 72
#define ATTR_TEXTAREANUMBER 73
#define ATTR_PRE_INT 74

#define MAX_TAGATTR 75

/* HTML Tag Information Table */

struct TagInfo {
  char *name;
  unsigned char *accept_attribute;
  unsigned char max_attribute;
  unsigned char flag;
};

#define TFLG_END 1
#define TFLG_INT 2

/* HTML Tag Attribute Information Table */

struct TagAttrInfo {
  char *name;
  unsigned char vtype;
  unsigned char flag;
};

#define AFLG_INT 1

#define VTYPE_NONE 0
#define VTYPE_STR 1
#define VTYPE_NUMBER 2
#define VTYPE_LENGTH 3
#define VTYPE_ALIGN 4
#define VTYPE_VALIGN 5
#define VTYPE_ACTION 6
#define VTYPE_ENCTYPE 7
#define VTYPE_METHOD 8
#define VTYPE_MLENGTH 9
#define VTYPE_TYPE 10

#define SHAPE_UNKNOWN 0
#define SHAPE_DEFAULT 1
#define SHAPE_RECT 2
#define SHAPE_CIRCLE 3
#define SHAPE_POLY 4

extern TagInfo TagMAP[];
extern TagAttrInfo AttrMAP[];

#define MAX_ENV_LEVEL 20
#define MAX_INDENT_LEVEL 10
