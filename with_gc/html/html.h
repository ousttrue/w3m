#pragma once

/* HTML Tag Information Table */

struct TagInfo {
  const char *name;
  unsigned char *accept_attribute;
  unsigned char max_attribute;
  unsigned char flag;
};

#define TFLG_END 1
#define TFLG_INT 2

/* HTML Tag Attribute Information Table */

struct TagAttrInfo {
  const char *name;
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
