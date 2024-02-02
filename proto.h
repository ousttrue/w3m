/* $Id: proto.h,v 1.104 2010/07/25 09:55:05 htrb Exp $ */
/*
 *   This file was automatically generated by version 1.7 of cextract.
 *   Manual editing not recommended.
 *
 *   Created: Wed Feb 10 12:47:03 1999
 */
#include <stdio.h>
#include "line.h"

extern int main(int argc, char **argv);
extern void nulcmd(void);
extern void pushEvent(int cmd, void *data);
extern void pgFore(void);
extern void pgBack(void);
extern void hpgFore(void);
extern void hpgBack(void);
extern void lup1(void);
extern void ldown1(void);
extern void ctrCsrV(void);
extern void ctrCsrH(void);
extern void rdrwSc(void);
extern void srchfor(void);
extern void isrchfor(void);
extern void srchbak(void);
extern void isrchbak(void);
extern void srchnxt(void);
extern void srchprv(void);
extern void shiftl(void);
extern void shiftr(void);
extern void col1R(void);
extern void col1L(void);
extern void setEnv(void);
extern void pipeBuf(void);
extern void pipesh(void);
extern void readsh(void);
extern void execsh(void);
extern void ldfile(void);
extern void ldhelp(void);
extern void movL(void);
extern void movL1(void);
extern void movD(void);
extern void movD1(void);
extern void movU(void);
extern void movU1(void);
extern void movR(void);
extern void movR1(void);
extern void movLW(void);
extern void movRW(void);
extern void qquitfm(void);
extern void quitfm(void);
extern void selBuf(void);
extern void susp(void);
extern void goLine(void);
extern void goLineF(void);
extern void goLineL(void);
extern void linbeg(void);
extern void linend(void);
extern void editBf(void);
extern void editScr(void);
extern void followA(void);
extern void bufferA(void);
extern void followI(void);
extern void submitForm(void);
extern void followForm(void);
extern void topA(void);
extern void lastA(void);
extern void nthA(void);
extern void onA(void);

extern void nextA(void);
extern void prevA(void);
extern void nextVA(void);
extern void prevVA(void);
extern void nextL(void);
extern void nextLU(void);
extern void nextR(void);
extern void nextRD(void);
extern void nextD(void);
extern void nextU(void);
extern void nextBf(void);
extern void prevBf(void);
extern void backBf(void);
extern void deletePrevBuf(void);
extern void goURL(void);
extern void goHome(void);
extern void gorURL(void);
extern void ldBmark(void);
extern void adBmark(void);
extern void ldOpt(void);
extern void setOpt(void);
extern void pginfo(void);
extern void msgs(void);
extern void svA(void);
extern void svI(void);
extern void svBuf(void);
extern void svSrc(void);
extern void peekURL(void);
extern void peekIMG(void);
extern void curURL(void);
extern void vwSrc(void);
extern void reload(void);
extern void reshape(void);
extern void chkURL(void);
struct Buffer;
extern void chkURLBuffer(Buffer *buf);
extern void chkWORD(void);
#define chkNMID nulcmd
extern void rFrame(void);
extern void extbrz(void);
extern void linkbrz(void);
extern void curlno(void);
extern void execCmd(void);
#define dispI nulcmd
#define stopI nulcmd
extern void setAlarm(void);
struct AlarmEvent;
extern AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status,
                                 int cmd, void *data);
extern void reinit(void);
extern void defKey(void);
extern void newT(void);
extern void closeT(void);
extern void nextT(void);
extern void prevT(void);
extern void tabA(void);
extern void tabURL(void);
extern void tabrURL(void);
extern void tabR(void);
extern void tabL(void);
extern void linkLst(void);
#define linkMn nulcmd
#define accessKey nulcmd
#define listMn nulcmd
#define movlistMn nulcmd
extern void undoPos(void);
extern void redoPos(void);
extern void cursorTop(void);
extern void cursorMiddle(void);
extern void cursorBottom(void);

struct Buffer;
extern int currentLn(Buffer *buf);
extern void tmpClearBuffer(Buffer *buf);

extern char **get_symbol(void);
struct Str;

extern void push_symbol(Str *str, char symbol, int width, int n);
struct FormList;

extern int is_boundary(unsigned char *, unsigned char *);
extern int is_blank_line(char *line, int indent);

extern char *convert_size(long long size, int usefloat);
extern char *convert_size2(long long size1, long long size2, int usefloat);

struct UrlStream;
extern Buffer *loadBuffer(UrlStream *uf, Buffer *newBuf);
extern void saveBuffer(Buffer *buf, FILE *f, int cont);
extern void saveBufferBody(Buffer *buf, FILE *f, int cont);
union input_stream;
extern Buffer *openPagerBuffer(input_stream *stream, Buffer *buf);
extern Buffer *openGeneralPagerBuffer(input_stream *stream);
struct Line;
extern Line *getNextPage(Buffer *buf, int plen);

extern Buffer *doExternal(UrlStream uf, const char *type, Buffer *defaultbuf);

struct ParsedURL;
extern void readHeader(UrlStream *uf, Buffer *newBuf, int thru, ParsedURL *pu);

struct TabBuffer;
extern TabBuffer *newTab(void);
extern void calcTabPos(void);
extern TabBuffer *deleteTab(TabBuffer *tab);

extern Buffer *nullBuffer(void);
extern void clearBuffer(Buffer *buf);
extern void discardBuffer(Buffer *buf);
extern Buffer *namedBuffer(Buffer *first, char *name);
extern Buffer *deleteBuffer(Buffer *first, Buffer *delbuf);
extern Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
extern Buffer *nthBuffer(Buffer *firstbuf, int n);
extern void gotoRealLine(Buffer *buf, int n);
extern Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf,
                            char *selectchar);
extern void reshapeBuffer(Buffer *buf);
extern void copyBuffer(Buffer *a, Buffer *b);
extern Buffer *prevBuffer(Buffer *first, Buffer *buf);
extern int writeBufferCache(Buffer *buf);
extern int readBufferCache(Buffer *buf);

extern void cursorUp0(Buffer *buf, int n);
extern void cursorUp(Buffer *buf, int n);
extern void cursorDown0(Buffer *buf, int n);
extern void cursorDown(Buffer *buf, int n);
extern void cursorUpDown(Buffer *buf, int n);
extern void cursorRight(Buffer *buf, int n);
extern void cursorLeft(Buffer *buf, int n);
extern void cursorHome(Buffer *buf);
extern void arrangeCursor(Buffer *buf);
extern void arrangeLine(Buffer *buf);
extern void cursorXY(Buffer *buf, int x, int y);
extern int columnSkip(Buffer *buf, int offset);
extern int columnPos(Line *line, int column);
extern int columnLen(Line *line, int column);
extern Line *lineSkip(Buffer *buf, Line *line, int offset, int last);
extern Line *currentLineSkip(Buffer *buf, Line *line, int offset, int last);

#define conv_search_string(str, f_ces) str
extern void pcmap(void);
extern void escmap(void);
extern void escbmap(void);
extern void escdmap(char c);
extern void multimap(void);
extern Str *unescape_spaces(Str *s);
extern double log_like(int x);

struct MapList;
extern MapList *searchMapList(Buffer *buf, char *name);
extern Buffer *follow_map_panel(Buffer *buf, char *name);

struct Anchor;
extern Anchor *retrieveCurrentMap(Buffer *buf);

extern Buffer *page_info_panel(Buffer *buf);
struct HtmlTag;
extern struct frame_body *newFrame(HtmlTag *tag, Buffer *buf);
extern struct frameset *newFrameSet(HtmlTag *tag);
extern void deleteFrame(struct frame_body *b);
extern void deleteFrameSet(struct frameset *f);
extern void deleteFrameSetElement(union frameset_element e);
extern struct frameset *copyFrameSet(struct frameset *of);
extern void pushFrameTree(struct frameset_queue **fqpp, struct frameset *fs,
                          Buffer *buf);
extern struct frameset *popFrameTree(struct frameset_queue **fqpp);
extern void resetFrameElement(union frameset_element *f_element, Buffer *buf,
                              char *referer, FormList *request);
extern Buffer *renderFrame(Buffer *Cbuf, int force_reload);
extern union frameset_element *search_frame(struct frameset *fset, char *name);

extern ParsedURL *baseURL(Buffer *buf);

struct URLOption;
struct TextList;

extern void initMailcap(void);
extern char *acceptableMimeTypes(void);

extern Buffer *load_option_panel(void);
extern void sync_with_option(void);

extern void loadPasswd(void);
extern void loadPreForm(void);
extern void add_auth_user_passwd(ParsedURL *pu, char *realm, Str *uname,
                                 Str *pwd, int is_proxy);

#define docCSet nulcmd
#define defCSet nulcmd

#define _mark nulcmd
#define nextMk nulcmd
#define prevMk nulcmd
#define reMark nulcmd

#define mouse nulcmd
#define sgrmouse nulcmd
#define msToggle nulcmd
#define movMs nulcmd
#define menuMs nulcmd
#define tabMs nulcmd
#define closeTMs nulcmd


#define mainMn nulcmd
#define selMn selBuf
#define tabMn nulcmd

extern void dictword(void);
extern void dictwordat(void);

extern void wrapToggle(void);
extern void saveBufferInfo(void);

extern void dispVer(void);

#ifdef USE_INCLUDED_SRAND48
void srand48(long);
long lrand48(void);
#endif
