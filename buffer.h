#pragma once
#include "Line.h"
#include "LinkList.h"
#include "Url.h"
#include "textlist.h"
#include "anchor.h"
#include <wc.h>

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

typedef struct _Buffer {
    char* filename;
    char* buffername;
    struct Line* firstLine;
    struct Line* topLine;
    struct Line* currentLine;
    struct Line* lastLine;
    struct _Buffer* nextBuffer;
    struct _Buffer* linkBuffer[MAX_LB];
    short width;
    short height;
    char* type;
    char* real_type;
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
    char* baseTarget;
    int real_scheme;
    char* sourcefile;
    struct frameset* frameset;
    struct frameset_queue* frameQ;
    int* clone;
    size_t trbyte;
    char check_url;
    wc_ces document_charset;
    wc_uint8 auto_detect;
    TextList* document_header;
    struct form_item_list* form_submit;
    char* savecache;
    char* edit;
    struct mailcap* mailcap;
    char* mailcap_source;
    char* header_source;
    char search_header;
    char* ssl_certificate;
    char image_flag;
    char image_loaded;
    char need_reshape;
    struct _Anchor* submit;
    struct _BufferPos* undo;
    struct AlarmEvent* event;
} Buffer;

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
    Buffer* currentBuffer;
    Buffer* firstBuffer;
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

void chkURLBuffer(Buffer* buf);
void chkNMIDBuffer(Buffer* buf);
struct LinkList* link_menu(Buffer* buf);
struct _Anchor;
struct _Anchor* accesskey_menu(Buffer* buf);
struct _Anchor* list_menu(Buffer* buf);
int currentLn(Buffer* buf);
void tmpClearBuffer(Buffer* buf);
void deleteImage(Buffer* buf);
void getAllImage(Buffer* buf);
void HTMLlineproc2(Buffer* buf, TextLineList* tl);
struct URLFile;
Buffer* loadHTMLBuffer(struct URLFile* f, Buffer* newBuf);
void loadHTMLstream(struct URLFile* f, Buffer* newBuf, FILE* src,
    int internal);
Buffer* loadHTMLString(Str page);
Buffer* loadBuffer(struct URLFile* uf, Buffer* newBuf);
Buffer* loadImageBuffer(struct URLFile* uf, Buffer* newBuf);
void saveBuffer(Buffer* buf, FILE* f, int cont);
void saveBufferBody(Buffer* buf, FILE* f, int cont);
Buffer* getshell(char* cmd);
Buffer* getpipe(char* cmd);
Buffer* openPagerBuffer(union input_stream* stream, Buffer* buf);
Buffer* openGeneralPagerBuffer(union input_stream* stream);
struct Line* getNextPage(Buffer* buf, int plen);
Buffer* doExternal(struct URLFile uf, char* type, Buffer* defaultbuf);
void readHeader(struct URLFile* uf, Buffer* newBuf, int thru, struct Url* pu);
char* checkHeader(Buffer* buf, char* field);
TabBuffer* newTab(void);
TabBuffer* deleteTab(TabBuffer* tab);
Buffer* newBuffer(int width);
Buffer* nullBuffer(void);
void clearBuffer(Buffer* buf);
void discardBuffer(Buffer* buf);
Buffer* namedBuffer(Buffer* first, char* name);
Buffer* deleteBuffer(Buffer* first, Buffer* delbuf);
Buffer* replaceBuffer(Buffer* first, Buffer* delbuf, Buffer* newbuf);
Buffer* nthBuffer(Buffer* firstbuf, int n);
void gotoRealLine(Buffer* buf, int n);
void gotoLine(Buffer* buf, int n);
Buffer* selectBuffer(Buffer* firstbuf, Buffer* currentbuf,
    char* selectchar);
void reshapeBuffer(Buffer* buf);
void copyBuffer(Buffer* a, Buffer* b);
Buffer* prevBuffer(Buffer* first, Buffer* buf);
int writeBufferCache(Buffer* buf);
int readBufferCache(Buffer* buf);
void cursorUp0(Buffer* buf, int n);
void cursorUp(Buffer* buf, int n);
void cursorDown0(Buffer* buf, int n);
void cursorDown(Buffer* buf, int n);
void cursorUpDown(Buffer* buf, int n);
void cursorRight(Buffer* buf, int n);
void cursorLeft(Buffer* buf, int n);
void cursorHome(Buffer* buf);
void arrangeCursor(Buffer* buf);
void arrangeLine(Buffer* buf);
void cursorXY(Buffer* buf, int x, int y);
void restorePosition(Buffer* buf, Buffer* orig);
int columnSkip(Buffer* buf, int offset);
struct Line* lineSkip(Buffer* buf, struct Line* line, int offset, int last);
struct Line* currentLineSkip(Buffer* buf, struct Line* line, int offset, int last);
int forwardSearch(Buffer* buf, char* str);
int backwardSearch(Buffer* buf, char* str);
struct Hist;
Buffer* historyBuffer(struct Hist* hist);
void formRecheckRadio(Anchor* a, Buffer* buf, struct form_item_list* form);
void formResetBuffer(Buffer* buf, AnchorList* formitem);
void formUpdateBuffer(Anchor* a, Buffer* buf, struct form_item_list* form);
void preFormUpdateBuffer(Buffer* buf);
struct MapArea* follow_map_menu(Buffer* buf, char* name, Anchor* a_img, int x,
    int y);
Buffer* follow_map_panel(Buffer* buf, char* name);
int getMapXY(Buffer* buf, Anchor* a, int* x, int* y);
struct MapArea* retrieveCurrentMapArea(Buffer* buf);
Anchor* retrieveCurrentMap(Buffer* buf);
Buffer* page_info_panel(Buffer* buf);
struct parsed_tag;
struct frame_body* newFrame(struct parsed_tag* tag, Buffer* buf);
void pushFrameTree(struct frameset_queue** fqpp, struct frameset* fs,
    Buffer* buf);
union frameset_element;
void resetFrameElement(union frameset_element* f_element, Buffer* buf,
    char* referer, struct form_list* request);
Buffer* renderFrame(Buffer* Cbuf, int force_reload);
struct Url* baseURL(Buffer* buf);
Anchor* registerHref(Buffer* buf, char* url, char* target,
    char* referer, char* title, unsigned char key,
    int line, int pos);
Anchor* registerName(Buffer* buf, char* url, int line, int pos);
Anchor* registerImg(Buffer* buf, char* url, char* title, int line,
    int pos);
Anchor* registerForm(Buffer* buf, struct form_list* flist,
    struct parsed_tag* tag, int line, int pos);
Anchor* retrieveCurrentAnchor(Buffer* buf);
Anchor* retrieveCurrentImg(Buffer* buf);
Anchor* retrieveCurrentForm(Buffer* buf);
Anchor* searchURLLabel(Buffer* buf, char* url);
void reAnchorWord(Buffer* buf, struct Line* l, int spos, int epos);
char* reAnchor(Buffer* buf, char* re);
char* reAnchorNews(Buffer* buf, char* re);
char* reAnchorNewsheader(Buffer* buf);
void addMultirowsForm(Buffer* buf, AnchorList* al);
void addMultirowsImg(Buffer* buf, AnchorList* al);
char* getAnchorText(Buffer* buf, AnchorList* al, Anchor* a);
Buffer* link_list_panel(Buffer* buf);
Buffer* load_option_panel(void);
char* last_modified(Buffer* buf);
char* guess_save_name(Buffer* buf, char* file);
void saveBufferInfo(void);
wc_ces urlCharset(Buffer* buf, const char* url);
struct parsed_tagarg;
void follow_map(struct parsed_tagarg* arg);
