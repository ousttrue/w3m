#pragma once
#include "url.h"
#include "content.h"
#include "document.h"
#include <stddef.h>
#include <libwc/wc_types.h>

enum LinkBufferID {
    LB_NOLINK = -1,
    LB_INFO = 0, /* pginfo() */
    LB_N_INFO = 1,
    LB_SOURCE = 2, /* vwSrc() */
    LB_N_SOURCE = LB_SOURCE,
    MAX_LB = 3,
};

enum BufferPropertyFlags : uint16_t {
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
    BP_FRAME = 0x2,
    BP_INTERNAL = 0x8,
    BP_NO_URL = 0x10,
    BP_REDIRECTED = 0x20,
    BP_CLOSE = 0x40,
};

enum CheckUrlFlags : uint8_t {
    CHK_URL = 1,
    CHK_NMID = 2,
};

struct Buffer {
    struct Content* content;
    struct Document* doc;
    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    enum BufferPropertyFlags bufferprop;

    int* clone;
    enum CheckUrlFlags check_url;
    const char* savecache;
    const char* edit;
    struct _AlarmEvent* event;
};

struct Buffer* buf_new(struct Content* content);
struct Url* buf_baseUrl(struct Buffer* buf);

void buf_set_link(struct Buffer* buf,
    struct Buffer* link_buf, enum BufferPropertyFlags bp, enum LinkBufferID linkid);

bool readBufferCache(struct Buffer* buf);
void reshapeBuffer(struct Buffer* buf);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
void saveBufferBody(struct Buffer* buf, FILE* f, int cont);
struct Buffer* getshell(char* cmd);
void buf_discard(struct Buffer* buf);
void copyBuffer(struct Buffer* a, struct Buffer* b);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
bool checkBackBuffer(struct Buffer* buf);
Str page_info_panel(struct Buffer* buf);

struct FollowOption {
    bool on_target;
    bool do_download;
};
struct FollowResult {
    struct Anchor* anchor;
    struct Buffer* new_buf;
};
struct FollowResult buf_followForm(struct Buffer* buf, struct FollowOption option, bool submit);
struct FollowResult buf_followA(struct Buffer* buf, struct FollowOption option);
struct FollowResult gotoLabel(struct Buffer* buf, const char* label);
void _followI(bool do_download);
struct Buffer* loadLink(const char* url, struct FormList* request,
    const char* target, const char* referer, struct FollowOption option);
