#pragma once
#include <Str.h>
#include <wc.h>
#include "line.h"
#include "url.h"
#include "istream.h"

extern char UseContentCharset;
extern wc_ces DocumentCharset;
extern int autoImage;
extern char MetaRefresh;
extern char DecodeCTE;
extern int label_topline;
extern int UseExternalDirBuffer;
extern char* DefaultType;
extern int displayLinkNumber;
extern char SimplePreserveSpace;
extern int squeezeBlankLine;

#define CGI_EXTENSION ".cgi"
// #define CGI_EXTENSION ".cmd"
extern char* DirBufferCommand;

#define set_prevchar(x, y, n) Strcopy_charp_n((x), (y), (n))
#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

struct _Buffer;
struct _ParsedURL;
struct form_list;
struct HtmlTagParsed;

struct Content {
    struct _ParsedURL pu;
    struct URLFile f;
    Str page;
    wc_ces charset;
    const char* real_type;
    TextList* document_header;
};

struct Content loadGeneralFile(const char* path, struct _ParsedURL* current, struct form_list* post,
    const char* referer, bool no_cache);
struct _Buffer* makeBuffer(struct Content* c, bool do_download);

int is_boundary(unsigned char*, unsigned char*);

struct _Buffer* loadHTMLString(Str page);

struct URLFile;
void loadHTMLstream(struct URLFile* f, struct _Buffer* newBuf, FILE* src, int internal);

struct _Buffer;
void loadHTML(Str html, wc_ces doc_charset, int cols, bool use_graphic, bool internal, struct _Buffer* buf);
void addnewline(struct _Buffer* buf, char* line, Lineprop* prop, Linecolor* color, int pos, int width, int nlines);
int getMetaRefreshParam(char* q, Str* refresh_uri);
