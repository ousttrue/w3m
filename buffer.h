#pragma once
#include "Line.h"
#include "LinkList.h"
#include "Url.h"
#include "textlist.h"
#include "anchor.h"
#include <wc/wc.h>

extern int REV_LB[];

/* Buffer Property */
#define BP_NORMAL 0x0
#define BP_PIPE 0x1
#define BP_FRAME 0x2
#define BP_INTERNAL 0x8
#define BP_NO_URL 0x10
#define BP_REDIRECTED 0x20
#define BP_CLOSE 0x40

/* Link Buffer */
#define LB_NOLINK -1
#define LB_FRAME 0 /* rFrame() */
#define LB_N_FRAME 1
#define LB_INFO 2 /* pginfo() */
#define LB_N_INFO 3
#define LB_SOURCE 4 /* vwSrc() */
#define LB_N_SOURCE LB_SOURCE
#define MAX_LB 5

struct Buffer {
    const char* filename;
    const char* buffername;
    struct Line* firstLine;
    struct Line* topLine;
    struct Line* currentLine;
    struct Line* lastLine;
    struct Buffer* nextBuffer;
    struct Buffer* linkBuffer[MAX_LB];
    short width;
    short height;
    const char* type;
    const char* real_type;
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
    union input_stream* pagerSource;
    struct _AnchorList* href;
    struct _AnchorList* name;
    struct _AnchorList* img;
    struct _AnchorList* formitem;
    struct LinkList* linklist;
    struct form_list* formlist;
    struct MapList* maplist;
    struct _HmarkerList* hmarklist;
    struct _HmarkerList* imarklist;
    struct Url currentURL;
    struct Url* baseURL;
    const char* baseTarget;
    int real_scheme;
    const char* sourcefile;
    struct frameset* frameset;
    struct frameset_queue* frameQ;
    int* clone;
    size_t trbyte;
    char check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    TextList* document_header;
    struct form_item_list* form_submit;
    const char* savecache;
    const char* edit;
    struct mailcap* mailcap;
    const char* mailcap_source;
    const char* header_source;
    char search_header;
    const char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    struct _Anchor* submit;
    struct _BufferPos* undo;
    struct AlarmEvent* event;
};

typedef struct _BufferPos {
    long top_linenumber;
    long cur_linenumber;
    int currentColumn;
    int pos;
    int bpos;
    struct _BufferPos* next;
    struct _BufferPos* prev;
} BufferPos;

typedef struct _TabBuffer {
    struct _TabBuffer* nextTab;
    struct _TabBuffer* prevTab;
    struct Buffer* currentBuffer;
    struct Buffer* firstBuffer;
    short x1;
    short x2;
    short y;
} TabBuffer;

/*
 * global Buffer *Currentbuf;
 * global Buffer *Firstbuf;
 */
extern TabBuffer* CurrentTab;
extern TabBuffer* FirstTab;
extern TabBuffer* LastTab;
extern int open_tab_blank;
extern int open_tab_dl_list;
extern int close_tab_back;
extern int nTab;
extern int TabCols;
#define NO_TABBUFFER ((TabBuffer*)1)
#define Currentbuf (CurrentTab->currentBuffer)
#define Firstbuf (CurrentTab->firstBuffer)

void chkURLBuffer(struct Buffer* buf);
void chkNMIDBuffer(struct Buffer* buf);
struct LinkList* link_menu(struct Buffer* buf);
struct _Anchor;
struct _Anchor* accesskey_menu(struct Buffer* buf);
struct _Anchor* list_menu(struct Buffer* buf);
int currentLn(struct Buffer* buf);
void tmpClearBuffer(struct Buffer* buf);
void deleteImage(struct Buffer* buf);
void getAllImage(struct Buffer* buf);
void HTMLlineproc2(struct Buffer* buf, TextLineList* tl);
struct URLFile;
struct Buffer* loadHTMLBuffer(struct URLFile* f, struct Buffer* newBuf);
void loadHTMLstream(struct URLFile* f, struct Buffer* newBuf, FILE* src,
    int internal);
struct Buffer* loadHTMLString(Str page);
struct Buffer* loadBuffer(struct URLFile* uf, struct Buffer* newBuf);
struct Buffer* loadImageBuffer(struct URLFile* uf, struct Buffer* newBuf);
void saveBuffer(struct Buffer* buf, FILE* f, int cont);
void saveBufferBody(struct Buffer* buf, FILE* f, int cont);
struct Buffer* getshell(char* cmd);
struct Buffer* getpipe(char* cmd);
struct Buffer* openPagerBuffer(union input_stream* stream, struct Buffer* buf);
struct Buffer* openGeneralPagerBuffer(union input_stream* stream);
struct Line* getNextPage(struct Buffer* buf, int plen);
struct Buffer* doExternal(struct URLFile uf, char* type, struct Buffer* defaultbuf);
void readHeader(struct URLFile* uf, struct Buffer* newBuf, int thru, struct Url* pu);
char* checkHeader(struct Buffer* buf, char* field);
TabBuffer* newTab(void);
TabBuffer* deleteTab(TabBuffer* tab);
struct Buffer* newBuffer(int width);
struct Buffer* nullBuffer(void);
void clearBuffer(struct Buffer* buf);
void discardBuffer(struct Buffer* buf);
struct Buffer* namedBuffer(struct Buffer* first, char* name);
struct Buffer* deleteBuffer(struct Buffer* first, struct Buffer* delbuf);
struct Buffer* replaceBuffer(struct Buffer* first, struct Buffer* delbuf, struct Buffer* newbuf);
struct Buffer* nthBuffer(struct Buffer* firstbuf, int n);
void gotoRealLine(struct Buffer* buf, int n);
void gotoLine(struct Buffer* buf, int n);
struct Buffer* selectBuffer(struct Buffer* firstbuf, struct Buffer* currentbuf,
    char* selectchar);
void reshapeBuffer(struct Buffer* buf);
void copyBuffer(struct Buffer* a, struct Buffer* b);
struct Buffer* prevBuffer(struct Buffer* first, struct Buffer* buf);
int writeBufferCache(struct Buffer* buf);
int readBufferCache(struct Buffer* buf);
void cursorUp0(struct Buffer* buf, int n);
void cursorUp(struct Buffer* buf, int n);
void cursorDown0(struct Buffer* buf, int n);
void cursorDown(struct Buffer* buf, int n);
void cursorUpDown(struct Buffer* buf, int n);
void cursorRight(struct Buffer* buf, int n);
void cursorLeft(struct Buffer* buf, int n);
void cursorHome(struct Buffer* buf);
void arrangeCursor(struct Buffer* buf);
void arrangeLine(struct Buffer* buf);
void cursorXY(struct Buffer* buf, int x, int y);
void restorePosition(struct Buffer* buf, struct Buffer* orig);
int columnSkip(struct Buffer* buf, int offset);
struct Line* lineSkip(struct Buffer* buf, struct Line* line, int offset, int last);
struct Line* currentLineSkip(struct Buffer* buf, struct Line* line, int offset, int last);
int forwardSearch(struct Buffer* buf, char* str);
int backwardSearch(struct Buffer* buf, char* str);
struct Hist;
struct Buffer* historyBuffer(struct Hist* hist);
void formRecheckRadio(Anchor* a, struct Buffer* buf, struct form_item_list* form);
void formResetBuffer(struct Buffer* buf, AnchorList* formitem);
void formUpdateBuffer(Anchor* a, struct Buffer* buf, struct form_item_list* form);
void preFormUpdateBuffer(struct Buffer* buf);
struct MapArea* follow_map_menu(struct Buffer* buf, char* name, Anchor* a_img, int x,
    int y);
struct Buffer* follow_map_panel(struct Buffer* buf, char* name);
int getMapXY(struct Buffer* buf, Anchor* a, int* x, int* y);
struct MapArea* retrieveCurrentMapArea(struct Buffer* buf);
Anchor* retrieveCurrentMap(struct Buffer* buf);
struct Buffer* page_info_panel(struct Buffer* buf);
struct HtmlTag;
struct frame_body* newFrame(struct HtmlTag* tag, struct Buffer* buf);
void pushFrameTree(struct frameset_queue** fqpp, struct frameset* fs,
    struct Buffer* buf);
union frameset_element;
void resetFrameElement(union frameset_element* f_element, struct Buffer* buf,
    char* referer, struct form_list* request);
struct Buffer* renderFrame(struct Buffer* Cbuf, int force_reload);
struct Url* baseURL(struct Buffer* buf);
Anchor* registerHref(struct Buffer* buf, char* url, char* target,
    char* referer, char* title, unsigned char key,
    int line, int pos);
Anchor* registerName(struct Buffer* buf, char* url, int line, int pos);
Anchor* registerImg(struct Buffer* buf, char* url, char* title, int line,
    int pos);
Anchor* registerForm(struct Buffer* buf, struct form_list* flist,
    struct HtmlTag* tag, int line, int pos);
Anchor* retrieveCurrentAnchor(struct Buffer* buf);
Anchor* retrieveCurrentImg(struct Buffer* buf);
Anchor* retrieveCurrentForm(struct Buffer* buf);
Anchor* searchURLLabel(struct Buffer* buf, char* url);
void reAnchorWord(struct Buffer* buf, struct Line* l, int spos, int epos);
char* reAnchor(struct Buffer* buf, char* re);
char* reAnchorNews(struct Buffer* buf, char* re);
char* reAnchorNewsheader(struct Buffer* buf);
void addMultirowsForm(struct Buffer* buf, AnchorList* al);
void addMultirowsImg(struct Buffer* buf, AnchorList* al);
char* getAnchorText(struct Buffer* buf, AnchorList* al, Anchor* a);
struct Buffer* link_list_panel(struct Buffer* buf);
struct Buffer* load_option_panel(void);
char* last_modified(struct Buffer* buf);
char* guess_save_name(struct Buffer* buf, char* file);
void saveBufferInfo(void);
wc_ces urlCharset(struct Buffer* buf, const char* url);
struct KeyValueList;
void follow_map(struct KeyValueList* arg);
