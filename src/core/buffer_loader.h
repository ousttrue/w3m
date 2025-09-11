#pragma once
#include <Str.h>
#include <wc.h>
#include "line.h"
#include "url.h"
#include "istream.h"
#include "platform.h"

extern wc_ces DocumentCharset;
extern int autoImage;
extern char MetaRefresh;
extern char DecodeCTE;
extern int label_topline;
extern int UseExternalDirBuffer;
extern const char* DefaultType;
extern int displayLinkNumber;
extern char SimplePreserveSpace;
extern int squeezeBlankLine;
extern char* DirBufferCommand;
extern bool PermitSaveToPipe;

bool canCopyFile(const char* path1, const char* path2);
int setModtime(const char* path, time_t modtime);
int _doFileCopy(const char* tmpf, const char* defstr, int download);
inline static int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
int doFileMove(const char* tmpf, const char* defstr);

#define set_prevchar(x, y, n) Strcopy_charp_n((x), (y), (n))
#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

struct _Buffer;
struct form_list;
struct HtmlTagParsed;
struct Content;
struct html_feed_environ;
union input_stream;

int is_boundary(unsigned char*, unsigned char*);
int getMetaRefreshParam(const char* q, Str* refresh_uri);
void HTMLlineproc0(const char* istr, struct html_feed_environ* h_env, bool internal);
struct _Buffer* makeBuffer(struct Content* c);
struct _Buffer* loadHTMLString(Str page);
struct _Buffer* loadHTMLBuffer(struct Url url, union input_stream *stream, struct _Buffer* newBuf);
struct _Buffer* loadBuffer(struct Url url, union input_stream *streamf, struct _Buffer* newBuf);
