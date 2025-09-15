#pragma once
#include "ContentType.h"
#include "Document.h"

extern int autoImage;
extern char MetaRefresh;
extern char DecodeCTE;
extern int UseExternalDirBuffer;
extern const char* DefaultType;
extern int squeezeBlankLine;
extern const char* DirBufferCommand;
extern bool PermitSaveToPipe;

struct Document loadContent(struct Url url,
    const char* content, wc_ces content_charset, enum ContentType content_type,
    int cols, bool use_graphic);
