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
extern void ldDL(void);
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
extern char *filename_extension(char *patch, int is_url);
struct ParsedURL;
extern ParsedURL *schemeToProxy(int scheme);

extern char *url_decode0(const char *url);
#define url_decode2(url, buf) url_decode0(url)
struct URLFile;
extern void examineFile(char *path, URLFile *uf);
extern char *acceptableEncoding(void);
extern int dir_exist(char *path);
extern char **get_symbol(void);
struct Str;

extern void push_symbol(Str *str, char symbol, int width, int n);
struct FormList;

extern int is_boundary(unsigned char *, unsigned char *);
extern int is_blank_line(char *line, int indent);

extern char *convert_size(long long size, int usefloat);
extern char *convert_size2(long long size1, long long size2, int usefloat);

extern Buffer *loadBuffer(URLFile *uf, Buffer *newBuf);
extern void saveBuffer(Buffer *buf, FILE *f, int cont);
extern void saveBufferBody(Buffer *buf, FILE *f, int cont);
extern Buffer *getshell(char *cmd);
extern Buffer *getpipe(char *cmd);
union input_stream;
extern Buffer *openPagerBuffer(input_stream *stream, Buffer *buf);
extern Buffer *openGeneralPagerBuffer(input_stream *stream);
struct Line;
extern Line *getNextPage(Buffer *buf, int plen);
extern int save2tmp(URLFile uf, char *tmpf);
extern Buffer *doExternal(URLFile uf, const char *type, Buffer *defaultbuf);
extern int _doFileCopy(char *tmpf, char *defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
extern int doFileMove(char *tmpf, char *defstr);
extern int doFileSave(URLFile uf, char *defstr);
extern int checkCopyFile(char *path1, char *path2);
extern int checkSaveFile(input_stream *stream, char *path);
extern int checkOverWrite(char *path);
extern char *inputAnswer(char *prompt);
extern int matchattr(char *p, char *attr, int len, Str **value);
extern void readHeader(URLFile *uf, Buffer *newBuf, int thru, ParsedURL *pu);
extern char *checkHeader(Buffer *buf, char *field);
struct TabBuffer;
extern TabBuffer *newTab(void);
extern void calcTabPos(void);
extern TabBuffer *deleteTab(TabBuffer *tab);

extern Buffer *newBuffer(int width);
extern Buffer *nullBuffer(void);
extern void clearBuffer(Buffer *buf);
extern void discardBuffer(Buffer *buf);
extern Buffer *namedBuffer(Buffer *first, char *name);
extern Buffer *deleteBuffer(Buffer *first, Buffer *delbuf);
extern Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
extern Buffer *nthBuffer(Buffer *firstbuf, int n);
extern void gotoRealLine(Buffer *buf, int n);
extern void gotoLine(Buffer *buf, int n);
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
extern void restorePosition(Buffer *buf, Buffer *orig);
extern int columnSkip(Buffer *buf, int offset);
extern int columnPos(Line *line, int column);
extern int columnLen(Line *line, int column);
extern Line *lineSkip(Buffer *buf, Line *line, int offset, int last);
extern Line *currentLineSkip(Buffer *buf, Line *line, int offset, int last);

extern char *lastFileName(char *path);
extern char *mybasename(char *s);
extern char *mydirname(char *s);
#define conv_search_string(str, f_ces) str
extern int forwardSearch(Buffer *buf, char *str);
extern int backwardSearch(Buffer *buf, char *str);
extern void pcmap(void);
extern void escmap(void);
extern void escbmap(void);
extern void escdmap(char c);
extern void multimap(void);
struct Hist;
extern Str *unescape_spaces(Str *s);
extern Buffer *historyBuffer(Hist *hist);
extern double log_like(int x);

struct MapList;
extern MapList *searchMapList(Buffer *buf, char *name);
extern Buffer *follow_map_panel(Buffer *buf, char *name);
struct Anchor;
extern Anchor *retrieveCurrentMap(Buffer *buf);
extern Buffer *page_info_panel(Buffer *buf);
extern struct frame_body *newFrame(struct parsed_tag *tag, Buffer *buf);
extern struct frameset *newFrameSet(struct parsed_tag *tag);
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

extern int openSocket(char *hostname, char *remoteport_name,
                      unsigned short remoteport_num);
extern ParsedURL *baseURL(Buffer *buf);

union input_stream;
extern void init_stream(URLFile *uf, int scheme, input_stream *stream);
struct HRequest;
Str *HTTPrequestMethod(HRequest *hr);
Str *HTTPrequestURI(ParsedURL *pu, HRequest *hr);
struct URLOption;
struct TextList;
extern URLFile openURL(char *url, ParsedURL *pu, ParsedURL *current,
                       URLOption *option, FormList *request,
                       TextList *extra_header, URLFile *ouf, HRequest *hr,
                       unsigned char *status);
extern void initMailcap(void);
extern char *acceptableMimeTypes(void);
extern char *guessContentType(char *filename);
extern int check_no_proxy(char *domain);

struct Anchor;
extern Anchor *registerHref(Buffer *buf, char *url, char *target, char *referer,
                            char *title, unsigned char key, int line, int pos);
extern Anchor *registerName(Buffer *buf, char *url, int line, int pos);
extern Anchor *registerImg(Buffer *buf, char *url, char *title, int line,
                           int pos);
extern Anchor *registerForm(Buffer *buf, FormList *flist,
                            struct parsed_tag *tag, int line, int pos);

extern Anchor *retrieveCurrentAnchor(Buffer *buf);
extern Anchor *retrieveCurrentImg(Buffer *buf);
extern Anchor *retrieveCurrentForm(Buffer *buf);
extern Anchor *searchURLLabel(Buffer *buf, char *url);
extern void reAnchorWord(Buffer *buf, Line *l, int spos, int epos);
extern char *reAnchor(Buffer *buf, char *re);
struct AnchorList;
extern void addMultirowsForm(Buffer *buf, AnchorList *al);

extern char *getAnchorText(Buffer *buf, AnchorList *al, Anchor *a);
extern Buffer *link_list_panel(Buffer *buf);

extern int set_param_option(char *option);
extern char *get_param_option(char *name);
extern void init_rc(void);
extern void init_tmp(void);
extern Buffer *load_option_panel(void);
extern void sync_with_option(void);
extern char *rcFile(char *base);
extern char *etcFile(char *base);
extern char *confFile(char *base);
extern char *auxbinFile(char *base);
extern char *libFile(char *base);
extern char *helpFile(char *base);
extern Str *localCookie(void);
extern Str *loadLocalDir(char *dirname);
extern FILE *localcgi_post(char *, char *, FormList *, char *);
#define localcgi_get(u, q, r) localcgi_post((u), (q), NULL, (r))
extern FILE *openSecretFile(char *fname);
extern void loadPasswd(void);
extern void loadPreForm(void);
extern int find_auth_user_passwd(ParsedURL *pu, char *realm, Str **uname,
                                 Str **pwd, int is_proxy);
extern void add_auth_user_passwd(ParsedURL *pu, char *realm, Str *uname,
                                 Str *pwd, int is_proxy);
extern void invalidate_auth_user_passwd(ParsedURL *pu, char *realm, Str *uname,
                                        Str *pwd, int is_proxy);
extern char *last_modified(Buffer *buf);

extern void mySystem(char *command, int background);
extern Str *myExtCommand(char *cmd, char *arg, int redirect);
extern Str *myEditor(char *cmd, char *file, int line);
extern int is_localhost(const char *host);
extern char *file_to_url(char *file);
extern char *url_unquote_conv0(char *url);
#define url_unquote_conv(url, charset) url_unquote_conv0(url)
extern char *expandName(char *name);

extern Buffer *cookie_list_panel(void);

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

extern char *searchKeyData(void);

extern void setKeymap(char *p, int lineno, int verbose);
extern void initKeymap(int force);
extern int getFuncList(char *id);
extern int getKey(char *s);
extern char *getKeyData(int key);
extern char *getWord(char **str);
extern char *getQWord(char **str);

#define mainMn nulcmd
#define selMn selBuf
#define tabMn nulcmd

extern void dictword(void);
extern void dictwordat(void);
extern char *guess_save_name(Buffer *buf, char *file);

extern void wrapToggle(void);
extern void saveBufferInfo(void);

extern void dispVer(void);

#ifdef USE_INCLUDED_SRAND48
void srand48(long);
long lrand48(void);
#endif

extern Str *base64_encode(const char *src, size_t len);
