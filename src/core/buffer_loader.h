#pragma once
#include "geometry.h"
#include "url.h"
#include "istream.h"
#include "platform.h"
#include <Str.h>
#include <wc.h>

extern int autoImage;
extern char MetaRefresh;
extern char DecodeCTE;
extern int UseExternalDirBuffer;
extern const char* DefaultType;
extern int displayLinkNumber;
extern char SimplePreserveSpace;
extern int squeezeBlankLine;
extern const char* DirBufferCommand;
extern bool PermitSaveToPipe;

struct Content;
struct Document loadContent(struct Content* content, int cols, bool use_graphic);
