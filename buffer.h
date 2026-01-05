#pragma once
#include "url.h"
#include "content.h"
#include "document.h"
#include <stddef.h>
#include <libwc/wc_types.h>

enum LinkBufferID {
    LB_NOLINK = -1,
    LB_FRAME = 0, /* rFrame() */
    LB_N_FRAME = 1,
    LB_INFO = 2, /* pginfo() */
    LB_N_INFO = 3,
    LB_SOURCE = 4, /* vwSrc() */
    LB_N_SOURCE = LB_SOURCE,
    MAX_LB = 5,
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

struct Buffer* buf_new(struct Content *content);
struct Url* baseURL(struct Buffer* buf);
void delBuffer(struct Buffer* buf);
void cmd_loadBuffer(struct Buffer* buf, int prop, enum LinkBufferID linkid);
bool readBufferCache(struct Buffer* buf);
void reshapeBuffer(struct Buffer* buf);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
void saveBufferBody(struct Buffer* buf, FILE* f, int cont);
struct Buffer* getshell(char* cmd);
void clearBuffer(struct Buffer* buf);
void discardBuffer(struct Buffer* buf);
struct Buffer* namedBuffer(struct Buffer* first, char* name);
struct Buffer* deleteBuffer(struct Buffer* first, struct Buffer* delbuf);
struct Buffer* replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf);
struct Buffer* nthBuffer(struct Buffer* firstbuf, int n);
struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf, char* selectchar);
void copyBuffer(struct Buffer* a, struct Buffer* b);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
bool checkBackBuffer(struct Buffer* buf);
Str page_info_panel(struct Buffer* buf);
