#pragma once
#include "html/anchor.h"
#include "transport/url.h"
#include <wc_types.h>
#include <stdint.h>

struct Line;
union InputStream;
struct LinkList;
struct FormList;
struct FormItemList;
struct MapList;
struct AlarmEvent;
struct TextList;

enum LinkBufferTypes
{
    LB_NOLINK = -1,
    LB_FRAME = 0, /* rFrame() */
    LB_N_FRAME = 1,
    LB_INFO = 2, /* pginfo() */
    LB_N_INFO = 3,
    LB_SOURCE = 4, /* vwSrc() */
    LB_N_SOURCE = LB_SOURCE
};
const int MAX_LB = 5;

enum BufferProps
{
    BP_NORMAL = 0x0,
    BP_PIPE = 0x1,
    BP_FRAME = 0x2,
    BP_INTERNAL = 0x8,
    BP_NO_URL = 0x10,
    BP_REDIRECTED = 0x20,
    BP_CLOSE = 0x40,
};

struct BufferPos
{
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    BufferPos *next;
    BufferPos *prev;
};

using BufferPtr = struct Buffer *;
struct Buffer : gc_cleanup
{
    friend struct Tab;

    std::string filename;
    std::string buffername;
    Line *firstLine;
    Line *topLine;
    Line *currentLine;
    Line *lastLine;

private:
    BufferPtr nextBuffer;

public:
    BufferPtr linkBuffer[MAX_LB];
    short width;
    short height;

    // mimetype: text/plain
    std::string type;
    std::string real_type;

public:
    int allLine;
    short bufferprop;
    int currentColumn;
    short cursorX;
    short cursorY;
    int pos;
    int visualpos;
    short rootX;
    short rootY;
    short COLS;
    short LINES;
    InputStream *pagerSource;
    AnchorList href;
    AnchorList name;
    AnchorList img;
    AnchorList formitem;
    int prevhseq = -1;

    LinkList *linklist;
    FormList *formlist;
    MapList *maplist;
    std::vector<BufferPoint> hmarklist;
    std::vector<BufferPoint> imarklist;
    ParsedURL currentURL = {};
    ParsedURL baseURL = {};
    char *baseTarget;
    int real_scheme;
    char *sourcefile;
    struct frameset *frameset;
    struct frameset_queue *frameQ;
    int *clone;
    size_t trbyte;
    char check_url;
    wc_ces document_charset;
    uint8_t auto_detect;
    TextList *document_header;
    FormItemList *form_submit;
    char *savecache;
    char *edit;
    struct mailcap *mailcap;
    char *mailcap_source;
    char *header_source;
    char search_header;
    char *ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    Anchor *submit;
    BufferPos *undo;
    AlarmEvent *event;

    Buffer();
    ~Buffer();
    void TmpClear();
    int WriteBufferCache();
    int ReadBufferCache();
    BufferPtr Copy();
    void CopyFrom(BufferPtr src);
    void ClearLink();
    ParsedURL *BaseURL();
    void putHmarker(int line, int pos, int seq);
    void shiftAnchorPosition(AnchorList &al, const BufferPoint &bp, int shift);
};

BufferPtr newBuffer(int width);

void deleteImage(BufferPtr buf);
void getAllImage(BufferPtr buf);
void loadImage(BufferPtr buf, int flag);

BufferPtr nullBuffer(void);
void clearBuffer(BufferPtr buf);
void gotoRealLine(BufferPtr buf, int n);
void gotoLine(BufferPtr buf, int n);
void reshapeBuffer(BufferPtr buf);

void set_buffer_environ(BufferPtr buf);
void cmd_loadBuffer(BufferPtr buf, int prop, LinkBufferTypes linkid);

// anchor
const Anchor *retrieveCurrentAnchor(BufferPtr buf);
const Anchor *retrieveCurrentImg(BufferPtr buf);
const Anchor *retrieveCurrentForm(BufferPtr buf);
const Anchor *searchURLLabel(BufferPtr buf, char *url);
void reAnchorWord(BufferPtr buf, Line *l, int spos, int epos);
char *reAnchor(BufferPtr buf, char *re);
char *reAnchorNews(BufferPtr buf, char *re);
char *reAnchorNewsheader(BufferPtr buf);
void addMultirowsForm(BufferPtr buf, AnchorList &al);
void addMultirowsImg(BufferPtr buf, AnchorList &al);
char *getAnchorText(BufferPtr buf, AnchorList &al, Anchor *a);

void chkExternalURIBuffer(BufferPtr buf);
TextList *make_domain_list(char *domain_list);