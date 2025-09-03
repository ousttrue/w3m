#pragma once
#include <Str.h>
#include <wc.h>
#include "line.h"

extern char UseContentCharset;
extern wc_ces DocumentCharset;
extern int autoImage;
extern char MetaRefresh;

extern long long current_content_length;

struct _Buffer;
void loadHTML(Str html, wc_ces doc_charset, int cols, bool use_graphic, bool internal, struct _Buffer* buf);
void addnewline(struct _Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines);
int getMetaRefreshParam(char* q, Str* refresh_uri);
