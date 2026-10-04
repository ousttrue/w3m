#pragma once
#include "buffer.h"
#include "file.h"
#include "form.h"
#include "parsetag.h"
#include "parsetagx.h"

extern void nulcmd(void);
extern void pushEvent(int cmd, void* data);
extern void pgFore(void);
extern void pgBack(void);
extern void hpgFore(void);
extern void hpgBack(void);
extern void lup1(void);
extern void ldown1(void);
extern void ctrCsrV(void);
extern void ctrCsrH(void);
extern void lineTop(void);
extern void lineBottom(void);
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
extern void cd(void);
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
extern void followI(void);
extern void submitForm(void);
extern void topA(void);
extern void lastA(void);
extern void nthA(void);
extern void onA(void);

extern void nextA(void);
extern void prevA(void);
extern void nextVA(void);
extern void prevVA(void);
extern void nextI(void);
extern void prevI(void);
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
extern void goMain(void);
extern void gorURL(void);
extern void ldBmark(void);
extern void adBmark(void);
extern void strSession(void);
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
extern void foldPre(void);
extern void reload(void);
extern void reshape(void);
extern void chkURL(void);
extern void chkWORD(void);
extern void chkNMID(void);
extern void rFrame(void);
extern void extbrz(void);
extern void linkbrz(void);
extern void curlno(void);
extern void execCmd(void);
extern void dispI(void);
extern void stopI(void);
extern void setAlarm(void);
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
extern void linkMn(void);
extern LinkList* link_menu(Buffer* buf);
extern void accessKey(void);
extern Anchor* accesskey_menu(Buffer* buf);
extern void listMn(void);
extern void movlistMn(void);
extern Anchor* list_menu(Buffer* buf);
extern void undoPos(void);
extern void redoPos(void);
extern void cursorTop(void);
extern void cursorMiddle(void);
extern void cursorBottom(void);

extern int currentLn(Buffer* buf);
extern ParsedURL* schemeToProxy(int scheme);
struct UrlStream;
extern void examineFile(const char* path, struct UrlStream* uf);
extern char* acceptableEncoding(void);
extern int dir_exist(const char* path);
extern int is_html_type(const char* type);
extern Buffer* loadGeneralFile(const char* path, ParsedURL* current, const char* referer,
    int flag, FormList* request);
extern int is_boundary(const unsigned char*, const unsigned char*);
extern void push_render_image(pStr str, int width, int limit,
    struct html_feed_environ* h_env);
extern void flushline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    int indent, int force, int width);
extern void do_blankline(struct html_feed_environ* h_env,
    struct readbuffer* obuf, int indent, int width);
extern void purgeline(struct html_feed_environ* h_env);
extern void save_fonteffect(struct html_feed_environ* h_env,
    struct readbuffer* obuf);
extern void restore_fonteffect(struct html_feed_environ* h_env,
    struct readbuffer* obuf);

extern pStr process_img(struct parsed_tag* tag, int width);
extern pStr process_anchor(struct parsed_tag* tag, const char* tagbuf);
extern pStr process_input(struct parsed_tag* tag);
extern pStr process_button(struct parsed_tag* tag);
extern pStr process_n_button(void);
extern pStr process_select(struct parsed_tag* tag);
extern pStr process_n_select(void);
extern void feed_select(const char* str);
extern void process_option(void);
extern pStr process_textarea(struct parsed_tag* tag, int width);
extern pStr process_n_textarea(void);
extern void feed_textarea(const char* str);
extern pStr process_form(struct parsed_tag* tag);
extern pStr process_n_form(void);
extern int getMetaRefreshParam(const char* q, pStr* refresh_uri);
extern int HTMLtagproc1(struct parsed_tag* tag,
    struct html_feed_environ* h_env);
extern void HTMLlineproc2(Buffer* buf, struct TextLineList* tl);
extern void HTMLlineproc0(const char* istr, struct html_feed_environ* h_env,
    int internal);
#define HTMLlineproc1(x, y) HTMLlineproc0(x, y, true)
extern Buffer* loadHTMLBuffer(struct UrlStream* f, Buffer* newBuf);
extern char* convert_size(size_t size, int usefloat);
extern char* convert_size2(size_t size1, size_t size2, int usefloat);
extern void showProgress(size_t* linelen, size_t* trbyte);
extern void init_henv(struct html_feed_environ*, struct readbuffer*,
    struct environment*, int, struct TextLineList*, int, int);
extern void completeHTMLstream(struct html_feed_environ*,
    struct readbuffer*);
extern void loadHTMLstream(struct UrlStream* f, Buffer* newBuf, FILE* src,
    int internal);
extern Buffer* loadHTMLString(pStr page);
extern pStr loadGopherDir(struct UrlStream* uf, ParsedURL* pu, wc_ces* charset);
extern pStr loadGopherSearch(ParsedURL* pu, wc_ces* charset);
extern Buffer* loadBuffer(struct UrlStream* uf, Buffer* newBuf);
extern Buffer* loadImageBuffer(struct UrlStream* uf, Buffer* newBuf);
extern void saveBuffer(Buffer* buf, FILE* f, int cont);
extern void saveBufferBody(Buffer* buf, FILE* f, int cont);
extern Buffer* getshell(const char* cmd);
extern Buffer* getpipe(const char* cmd);
extern Buffer* openPagerBuffer(struct input_stream* stream, Buffer* buf);
extern Buffer* openGeneralPagerBuffer(struct input_stream* stream);
extern Line* getNextPage(Buffer* buf, int plen);
extern int save2tmp(struct UrlStream uf, const char* tmpf);
extern Buffer* doExternal(struct UrlStream uf, const char* type, Buffer* defaultbuf);
extern int _doFileCopy(const char* tmpf, const char* defstr, int download);
static inline int doFileCopy(const char* tmpf, const char* defstr)
{
    return _doFileCopy(tmpf, defstr, false);
}
extern int doFileMove(char* tmpf, char* defstr);
extern int doFileSave(struct UrlStream uf, const char* defstr);
extern int checkCopyFile(const char* path1, const char* path2);
extern int checkSaveFile(struct input_stream* stream, const char* path);
extern int checkOverWrite(const char* path);
extern int confirm(pStr prompt);
extern char confirm_multi(const char* prompt);
extern int matchattr(const char* p, const char* attr, int len, pStr* value);
extern void readHeader(struct UrlStream* uf, Buffer* newBuf, int thru, ParsedURL* pu);
extern char* checkHeader(Buffer* buf, const char* field);
extern void displayBuffer(Buffer* buf, int mode);
extern void addChar(char c, Lineprop mode);
extern void addMChar(char* c, Lineprop mode, size_t len);
extern void record_err_message(const char* s);
extern Buffer* message_list_panel(void);
extern void message(const char* s, int return_x, int return_y);
extern void disp_err_message(const char* s, int redraw_current);
extern void disp_message_nsec(const char* s, int redraw_current, int sec,
    int purge, int mouse);
extern void disp_message(const char* s, int redraw_current);
#define disp_message_nomouse disp_message
extern void set_delayed_message(const char* s);
extern void cursorUp0(Buffer* buf, int n);
extern void cursorUp(Buffer* buf, int n);
extern void cursorDown0(Buffer* buf, int n);
extern void cursorDown(Buffer* buf, int n);
extern void cursorUpDown(Buffer* buf, int n);
extern void cursorRight(Buffer* buf, int n);
extern void cursorLeft(Buffer* buf, int n);
extern void arrangeCursor(Buffer* buf);
extern void arrangeLine(Buffer* buf);
extern void cursorXY(Buffer* buf, int x, int y);
extern void restorePosition(Buffer* buf, Buffer* orig);
extern void pcmap(void);
extern void escmap(void);
extern void escbmap(void);
extern void multimap(void);
extern pStr unescape_spaces(pStr s);


extern Buffer* page_info_panel(Buffer* buf);

extern void initMimeTypes(void);
extern void free_ssl_ctx(void);
extern ParsedURL* baseURL(Buffer* buf);
extern pStr parsedURL2Str(const ParsedURL* pu);
extern pStr parsedURL2RefererStr(ParsedURL* pu);
extern int mailcapMatch(struct mailcap* mcap, const char* type);
extern struct mailcap* searchMailcap(struct mailcap* table, const char* type);
extern void initMailcap(void);
extern char* acceptableMimeTypes(void);
extern struct mailcap* searchExtViewer(const char* type);
extern struct TextList* make_domain_list(char* domain_list);


extern void addMultirowsForm(Buffer* buf, AnchorList* al);
extern Anchor* closest_next_anchor(AnchorList* a, Anchor* an, int x, int y);
extern Anchor* closest_prev_anchor(AnchorList* a, Anchor* an, int x, int y);
void addMultirowsImg(Buffer* buf, AnchorList* al);
extern HmarkerList* putHmarker(HmarkerList* ml, int line, int pos, int seq);
extern void shiftAnchorPosition(AnchorList* a, HmarkerList* hl, int line,
    int pos, int shift);
extern char* getAnchorText(Buffer* buf, AnchorList* al, Anchor* a);
extern Buffer* link_list_panel(Buffer* buf);

extern pStr decodeB(char** ww);
struct growbuf;
extern void decodeB_to_growbuf(struct growbuf* gb, char** ww);
extern pStr decodeQ(char** ww);
extern void decodeQP_to_growbuf(struct growbuf* gb, char** ww);
extern void decodeU_to_growbuf(struct growbuf* gb, char** ww);
extern pStr decodeWord(char** ow, wc_ces* charset);
extern pStr decodeMIME(pStr orgstr, wc_ces* charset);
extern FILE* openSecretFile(char* fname);
extern void loadPasswd(void);
extern void loadPreForm(void);

extern void docCSet(void);
extern void defCSet(void);
extern void change_charset(struct parsed_tagarg* arg);

extern void _mark(void);
extern void nextMk(void);
extern void prevMk(void);
extern void reMark(void);

#define mouse nulcmd
#define sgrmouse nulcmd
#define msToggle nulcmd
#define movMs nulcmd
#define menuMs nulcmd
#define tabMs nulcmd
#define closeTMs nulcmd

extern void initImage(void);
extern void termImage(void);
extern void addImage(ImageCache* cache, int x, int y, int sx, int sy, int w,
    int h);
extern void drawImage(void);
extern void clearImage(void);

extern const char* searchKeyData(void);

extern void initKeymap(int force);
extern int getKey(const char* s);
extern char* getKeyData(int key);
struct regex;
extern char* getRegexWord(const char** str, struct regex** regex_ret);

extern void dictword(void);
extern void dictwordat(void);

extern void wrapToggle(void);

extern pStr getLinkNumberStr(int correction);

extern void dispVer(void);

extern void userMessage(void);
extern pStr unquote_mailcap(const char* qstr, const char* type, const char* name, const char* attr, int* mc_stat);
