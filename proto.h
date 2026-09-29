/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_PROTO_H
#define W3M_PROTO_H

#include "fm.h"

#include "ctrlcode.h"
#include "buffer.h"
#include "file.h"
#include "form.h"
#include "parsetag.h"
#include "parsetagx.h"

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
#ifdef USE_NNTP
extern void chkNMID(void);
#else
#define chkNMID nulcmd
#endif
extern void rFrame(void);
extern void extbrz(void);
extern void linkbrz(void);
extern void curlno(void);
extern void execCmd(void);
#ifdef USE_IMAGE
extern void dispI(void);
extern void stopI(void);
#else
#define dispI nulcmd
#define stopI nulcmd
#endif
#ifdef USE_ALARM
extern void setAlarm(void);
extern AlarmEvent *setAlarmEvent(AlarmEvent * event, int sec, short status,
				 int cmd, void *data);
#else
#define setAlarm nulcmd
#endif
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
#ifdef USE_MENU
extern void linkMn(void);
extern LinkList *link_menu(Buffer *buf);
extern void accessKey(void);
extern Anchor *accesskey_menu(Buffer *buf);
extern void listMn(void);
extern void movlistMn(void);
extern Anchor *list_menu(Buffer *buf);
#else
#define linkMn nulcmd
#define accessKey nulcmd
#define listMn nulcmd
#define movlistMn nulcmd
#endif
extern void undoPos(void);
extern void redoPos(void);
extern void cursorTop(void);
extern void cursorMiddle(void);
extern void cursorBottom(void);

extern int currentLn(Buffer *buf);
extern char *filename_extension(char *patch, int is_url);
#ifdef USE_EXTERNAL_URI_LOADER
extern void initURIMethods(void);
extern Str searchURIMethods(ParsedURL *pu);
extern void chkExternalURIBuffer(Buffer *buf);
#endif
extern ParsedURL *schemeToProxy(int scheme);

extern wc_ces url_to_charset(char *url, ParsedURL *base,
			     wc_ces doc_charset);
extern char *url_decode2(char *url, Buffer *buf);

extern void examineFile(const char *path, URLFile *uf);
extern char *acceptableEncoding(void);
extern int dir_exist(const char *path);
extern int is_html_type(const char *type);
extern Buffer *loadGeneralFile(char *path, ParsedURL *current, char *referer,
			       int flag, FormList *request);
extern int is_boundary(const unsigned char *, const unsigned char *);
extern void push_render_image(Str str, int width, int limit,
			      struct html_feed_environ *h_env);
extern void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
		      int indent, int force, int width);
extern void do_blankline(struct html_feed_environ *h_env,
			 struct readbuffer *obuf, int indent, int width);
extern void purgeline(struct html_feed_environ *h_env);
extern void save_fonteffect(struct html_feed_environ *h_env,
			    struct readbuffer *obuf);
extern void restore_fonteffect(struct html_feed_environ *h_env,
			       struct readbuffer *obuf);
#ifdef USE_IMAGE
extern void deleteImage(Buffer *buf);
extern void getAllImage(Buffer *buf);
extern void loadImage(Buffer *buf, int flag);
extern ImageCache *getImage(Image * image, ParsedURL *current, int flag);
extern int getImageSize(ImageCache * cache);
#endif
extern Str process_img(struct parsed_tag *tag, int width);
extern Str process_anchor(struct parsed_tag *tag, const char *tagbuf);
extern Str process_input(struct parsed_tag *tag);
extern Str process_button(struct parsed_tag *tag);
extern Str process_n_button(void);
extern Str process_select(struct parsed_tag *tag);
extern Str process_n_select(void);
extern void feed_select(char *str);
extern void process_option(void);
extern Str process_textarea(struct parsed_tag *tag, int width);
extern Str process_n_textarea(void);
extern void feed_textarea(char *str);
extern Str process_form(struct parsed_tag *tag);
extern Str process_n_form(void);
extern int getMetaRefreshParam(const char *q, Str *refresh_uri);
extern int HTMLtagproc1(struct parsed_tag *tag,
			struct html_feed_environ *h_env);
extern void HTMLlineproc2(Buffer *buf, TextLineList *tl);
extern void HTMLlineproc0(char *istr, struct html_feed_environ *h_env,
			  int internal);
#define HTMLlineproc1(x,y) HTMLlineproc0(x,y,TRUE)
extern Buffer *loadHTMLBuffer(URLFile *f, Buffer *newBuf);
extern char *convert_size(size_t size, int usefloat);
extern char *convert_size2(size_t size1, size_t size2, int usefloat);
extern void showProgress(size_t * linelen, size_t * trbyte);
extern void init_henv(struct html_feed_environ *, struct readbuffer *,
		      struct environment *, int, TextLineList *, int, int);
extern void completeHTMLstream(struct html_feed_environ *,
			       struct readbuffer *);
extern void loadHTMLstream(URLFile *f, Buffer *newBuf, FILE * src,
			   int internal);
extern Buffer *loadHTMLString(Str page);
#ifdef USE_GOPHER
#ifdef USE_M17N
extern Str loadGopherDir(URLFile *uf, ParsedURL *pu, wc_ces * charset);
extern Str loadGopherSearch(ParsedURL *pu, wc_ces * charset);
#else
extern Str loadGopherDir0(URLFile *uf, ParsedURL *pu);
extern Str loadGopherSearch0(ParsedURL *pu);
#define loadGopherDir(uf,pu,charset) loadGopherDir0(uf,pu)
#define loadGopherSearch(pu,charset) loadGopherSearch0(pu)
#endif
#endif				/* USE_GOPHER */
extern Buffer *loadBuffer(URLFile *uf, Buffer *newBuf);
#ifdef USE_IMAGE
extern Buffer *loadImageBuffer(URLFile *uf, Buffer *newBuf);
#endif
extern void saveBuffer(Buffer *buf, FILE * f, int cont);
extern void saveBufferBody(Buffer *buf, FILE * f, int cont);
extern Buffer *getshell(char *cmd);
extern Buffer *getpipe(char *cmd);
extern Buffer *openPagerBuffer(InputStream stream, Buffer *buf);
extern Buffer *openGeneralPagerBuffer(InputStream stream);
extern Line *getNextPage(Buffer *buf, int plen);
extern int save2tmp(URLFile uf, const char *tmpf);
extern Buffer *doExternal(URLFile uf, const char *type, Buffer *defaultbuf);
extern int _doFileCopy(const char *tmpf, const char *defstr, int download);
static inline int  doFileCopy(const char *tmpf, const char *defstr){
    return _doFileCopy(tmpf, defstr, false);
}
extern int doFileMove(char *tmpf, char *defstr);
extern int doFileSave(URLFile uf, const char *defstr);
extern int checkCopyFile(const char *path1, const char *path2);
extern int checkSaveFile(InputStream stream, const char *path);
extern int checkOverWrite(const char *path);
extern int confirm(Str prompt);
extern char confirm_multi(const char *prompt);
extern int matchattr(const char *p, const char *attr, int len, Str *value);
extern void readHeader(URLFile *uf, Buffer *newBuf, int thru, ParsedURL *pu);
extern char *checkHeader(Buffer *buf, const char *field);
extern void displayBuffer(Buffer *buf, int mode);
extern void addChar(char c, Lineprop mode);
#ifdef USE_M17N
extern void addMChar(char *c, Lineprop mode, size_t len);
#endif
extern void record_err_message(const char *s);
extern Buffer *message_list_panel(void);
extern void message(const char *s, int return_x, int return_y);
extern void disp_err_message(const char *s, int redraw_current);
extern void disp_message_nsec(const char *s, int redraw_current, int sec,
			      int purge, int mouse);
extern void disp_message(const char *s, int redraw_current);
#ifdef USE_MOUSE
extern void disp_message_nomouse(char *s, int redraw_current);
#else
#define disp_message_nomouse disp_message
#endif
extern void set_delayed_message(const char *s);
extern void cursorUp0(Buffer *buf, int n);
extern void cursorUp(Buffer *buf, int n);
extern void cursorDown0(Buffer *buf, int n);
extern void cursorDown(Buffer *buf, int n);
extern void cursorUpDown(Buffer *buf, int n);
extern void cursorRight(Buffer *buf, int n);
extern void cursorLeft(Buffer *buf, int n);
extern void arrangeCursor(Buffer *buf);
extern void arrangeLine(Buffer *buf);
extern void cursorXY(Buffer *buf, int x, int y);
extern void restorePosition(Buffer *buf, Buffer *orig);
#ifndef USE_ANSI_COLOR
#define checkType(a,b,c) _checkType(a,b)
#endif
extern void pcmap(void);
extern void escmap(void);
extern void escbmap(void);
extern void multimap(void);
extern Str unescape_spaces(Str s);

/* XXX: Should be form.h, can't be due to circular deps */
extern struct form_list *newFormList(char *action, char *method, char *charset,
				     char *enctype, char *target, char *name,
				     struct form_list *_next);
extern struct form_item_list *formList_addInput(struct form_list *fl,
						struct parsed_tag *tag);
extern char *form2str(FormItemList *fi);
extern int formtype(char *typestr);
extern void formRecheckRadio(Anchor *a, Buffer *buf, FormItemList *form);
extern void formResetBuffer(Buffer *buf, AnchorList *formitem);
extern void formUpdateBuffer(Anchor *a, Buffer *buf, FormItemList *form);
extern void preFormUpdateBuffer(Buffer *buf);
extern Str textfieldrep(Str s, int width);
extern void input_textarea(FormItemList *fi);
extern void do_internal(char *action, char *data);
extern void form_write_data(FILE * f, char *boundary, char *name, char *value);
extern void form_write_from_file(FILE * f, char *boundary, char *name,
				 char *filename, char *file);
extern MapList *searchMapList(Buffer *buf, char *name);
extern void follow_map(struct parsed_tagarg *arg);
#if defined(USE_MENU) || defined(USE_IMAGE)
extern MapArea *follow_map_menu(Buffer *buf, char *name, Anchor *a_img, int x,
				int y);
#endif
#ifndef USE_MENU
extern Buffer *follow_map_panel(Buffer *buf, char *name);
#endif
#ifdef USE_IMAGE
extern int getMapXY(Buffer *buf, Anchor *a, int *x, int *y);
extern MapArea *retrieveCurrentMapArea(Buffer *buf);
#endif
extern Anchor *retrieveCurrentMap(Buffer *buf);
extern MapArea *newMapArea(char *url, char *target, char *alt, char *shape,
			   char *coords);
extern Buffer *page_info_panel(Buffer *buf);
extern int initscr(void);
extern void move(int line, int column);
#ifdef USE_M17N
extern void addmch(const char *p, size_t len);
#endif
extern void addch(char c);
extern void standout(void);
extern void standend(void);
extern void bold(void);
extern void boldend(void);
extern void underline(void);
extern void underlineend(void);
extern void graphstart(void);
extern void graphend(void);
extern int graph_ok(void);
#ifdef USE_COLOR
extern void setfcolor(int color);
#ifdef USE_BG_COLOR
extern void setbcolor(int color);
#endif				/* USE_BG_COLOR */
#endif				/* USE_COLOR */
extern void refresh(void);
#ifdef USE_RAW_SCROLL
extern void scroll(int);
extern void rscroll(int);
#endif
extern void clrtoeolx(void);
extern void clrtobotx(void);
extern void addstr(const char *s);
extern void addnstr(const char *s, int n);
extern void addnstr_sup(const char *s, int n);
extern void crmode(void);
extern void term_noecho(void);
extern void term_raw(void);
extern void term_cooked(void);
extern void term_cbreak(void);
extern void term_title(const char *s);
extern void toggle_stand(void);
extern void bell(void);
extern int sleep_till_anykey(int sec, int purge);
#ifdef USE_IMAGE
extern void touch_cursor(void);
#endif
extern void initMimeTypes(void);
extern void free_ssl_ctx(void);
extern ParsedURL *baseURL(Buffer *buf);
extern int openSocket(char *hostname, char *remoteport_name,
		      unsigned short remoteport_num);
extern Str parsedURL2Str(const ParsedURL *pu);
extern Str parsedURL2RefererStr(ParsedURL *pu);
extern void init_stream(URLFile *uf, int scheme, InputStream stream);
extern int mailcapMatch(struct mailcap *mcap, const char *type);
extern struct mailcap *searchMailcap(struct mailcap *table, const char *type);
extern void initMailcap(void);
extern char *acceptableMimeTypes(void);
extern struct mailcap *searchExtViewer(const char *type);
extern Str unquote_mailcap(char *qstr, const char *type, char *name, char *attr,
			   int *mc_stat);
extern char *guessContentType(const char *filename);
extern TextList *make_domain_list(char *domain_list);
extern InputStream openFTPStream(ParsedURL *pu, URLFile *uf);
#ifdef USE_M17N
extern Str loadFTPDir(ParsedURL *pu, wc_ces * charset);
#else
extern Str loadFTPDir0(ParsedURL *pu);
#define loadFTPDir(pu,charset)	loadFTPDir0(pu)
#endif
extern void closeFTP(void);
extern void disconnectFTP(void);
#ifdef USE_NNTP
extern InputStream openNewsStream(ParsedURL *pu);
#ifdef USE_M17N
extern Str loadNewsgroup(ParsedURL *pu, wc_ces * charset);
#else
extern Str loadNewsgroup0(ParsedURL *pu);
#define loadNewsgroup(pu,charset) loadNewsgroup0(pu)
#endif
extern void closeNews(void);
extern void disconnectNews(void);
#endif
extern AnchorList *putAnchor(AnchorList *al, char *url, char *target,
			     Anchor **anchor_return, char *referer,
			     char *title, unsigned char key, int line,
			     int pos);
extern Anchor *registerHref(Buffer *buf, char *url, char *target,
			    char *referer, char *title, unsigned char key,
			    int line, int pos);
extern Anchor *registerName(Buffer *buf, char *url, int line, int pos);
extern Anchor *registerImg(Buffer *buf, char *url, char *title, int line,
			   int pos);
extern Anchor *registerForm(Buffer *buf, FormList *flist,
			    struct parsed_tag *tag, int line, int pos);
extern int onAnchor(Anchor *a, int line, int pos);
extern Anchor *retrieveAnchor(AnchorList *al, int line, int pos);
extern Anchor *retrieveCurrentAnchor(Buffer *buf);
extern Anchor *retrieveCurrentImg(Buffer *buf);
extern Anchor *retrieveCurrentForm(Buffer *buf);
extern Anchor *searchAnchor(AnchorList *al, char *str);
extern Anchor *searchURLLabel(Buffer *buf, char *url);
extern void reAnchorWord(Buffer *buf, Line *l, int spos, int epos);
extern const char *reAnchor(Buffer *buf, char *re);
#ifdef USE_NNTP
extern const char *reAnchorNews(Buffer *buf, char *re);
extern char *reAnchorNewsheader(Buffer *buf);
#endif				/* USE_NNTP */
extern void addMultirowsForm(Buffer *buf, AnchorList *al);
extern Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
extern Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
#ifdef USE_IMAGE
void addMultirowsImg(Buffer *buf, AnchorList *al);
#endif
extern HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
extern void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line,
				int pos, int shift);
extern char *getAnchorText(Buffer *buf, AnchorList *al, Anchor *a);
extern Buffer *link_list_panel(Buffer *buf);

extern Str decodeB(char **ww);
extern void decodeB_to_growbuf(struct growbuf *gb, char **ww);
extern Str decodeQ(char **ww);
extern void decodeQP_to_growbuf(struct growbuf *gb, char **ww);
extern void decodeU_to_growbuf(struct growbuf *gb, char **ww);
#ifdef USE_M17N
extern Str decodeWord(char **ow, wc_ces * charset);
extern Str decodeMIME(Str orgstr, wc_ces * charset);
#else
extern Str decodeWord0(char **ow);
extern Str decodeMIME0(Str orgstr);
#define decodeWord(ow,charset) decodeWord0(ow)
#define decodeMIME(orgstr,charset) decodeMIME0(orgstr)
#endif
extern Str localCookie(void);
extern void set_environ(const char *var, const char *value);
extern FILE *localcgi_post(char *, char *, FormList *, char *);
#define localcgi_get(u, q, r) localcgi_post((u), (q), NULL, (r))
extern FILE *openSecretFile(char *fname);
extern void loadPasswd(void);
extern void loadPreForm(void);

#ifdef USE_M17N
extern void docCSet(void);
extern void defCSet(void);
extern void change_charset(struct parsed_tagarg *arg);
#else
#define docCSet nulcmd
#define defCSet nulcmd
#endif

#ifdef USE_MARK
extern void _mark(void);
extern void nextMk(void);
extern void prevMk(void);
extern void reMark(void);
#else				/* not USE_MARK */
#define _mark  nulcmd
#define nextMk nulcmd
#define prevMk nulcmd
#define reMark nulcmd
#endif				/* not USE_MARK */

#ifdef USE_MOUSE
extern void mouse(void);
extern void sgrmouse(void);
extern void mouse_active(void);
extern void mouse_inactive(void);
extern void msToggle(void);
extern void movMs(void);
#ifdef USE_MENU
extern void menuMs(void);
#else
#define menuMs nulcmd
#endif
extern void tabMs(void);
extern void closeTMs(void);
#else				/* not USE_MOUSE */
#define mouse nulcmd
#define sgrmouse nulcmd
#define msToggle nulcmd
#define movMs nulcmd
#define menuMs nulcmd
#define tabMs nulcmd
#define closeTMs nulcmd
#endif				/* not USE_MOUSE */

#ifdef USE_IMAGE
extern void initImage(void);
extern void termImage(void);
extern void addImage(ImageCache * cache, int x, int y, int sx, int sy, int w,
		     int h);
extern void drawImage(void);
extern void clearImage(void);
#endif

extern char *searchKeyData(void);

extern void setKeymap(char *p, int lineno, int verbose);
extern void initKeymap(int force);
extern int getFuncList(char *id);
extern int getKey(const char *s);
extern char *getKeyData(int key);
extern char *getWord(char **str);
extern char *getQWord(char **str);
struct regex;
extern char *getRegexWord(char **str, struct regex **regex_ret);
#ifdef USE_MOUSE
extern void initMouseAction(void);
#endif

#ifdef USE_DICT
extern void dictword(void);
extern void dictwordat(void);
#else				/* not USE_DICT */
#define dictword nulcmd
#define dictwordat nulcmd
#endif				/* not USE_DICT */
extern char *guess_save_name(Buffer *buf, const char *file);

extern void wrapToggle(void);

extern Str getLinkNumberStr(int correction);

extern void dispVer(void);


extern void userMessage(void);
#endif
