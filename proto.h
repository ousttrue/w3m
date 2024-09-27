/* $Id: proto.h,v 1.104 2010/07/25 09:55:05 htrb Exp $ */
/*
 *   This file was automatically generated by version 1.7 of cextract.
 *   Manual editing not recommended.
 *
 *   Created: Wed Feb 10 12:47:03 1999
 */
#include "Str.h"

extern void nulcmd(void);
extern void pushEvent(int cmd, void *data);
extern MySignalHandler intTrap(SIGNAL_ARG);
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
extern void chkWORD(void);
#define chkNMID nulcmd
#define rFrame nulcmd;
extern void extbrz(void);
extern void linkbrz(void);
extern void curlno(void);
extern void execCmd(void);
#define dispI nulcmd
#define stopI nulcmd
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
extern int currentLn(struct Buffer *buf);
extern void tmpClearBuffer(struct Buffer *buf);
extern char *filename_extension(char *patch, int is_url);
extern struct Url *schemeToProxy(int scheme);
#define url_encode(url, base, cs) url_quote(url)
extern char *url_decode0(const char *url);
#define url_decode2(url, buf) url_decode0(url)
struct URLFile;
extern void examineFile(char *path, struct URLFile *uf);
extern char *acceptableEncoding();
extern int dir_exist(char *path);
extern int is_html_type(char *type);
extern char **get_symbol(void);
extern Str convertLine0(struct URLFile *uf, Str line, int mode);
#define convertLine(uf, line, mode, charset, dcharset)                         \
  convertLine0(uf, line, mode)
extern void push_symbol(Str str, char symbol, int width, int n);
extern struct Buffer *loadGeneralFile(char *path, struct Url *current, char *referer,
                               int flag, struct FormList *request);
extern int is_boundary(unsigned char *, unsigned char *);
extern int is_blank_line(char *line, int indent);
extern void push_render_image(Str str, int width, int limit,
                              struct html_feed_environ *h_env);
extern void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
                      int indent, int force, int width);
extern void do_blankline(struct html_feed_environ *h_env,
                         struct readbuffer *obuf, int indent, int indent_incr,
                         int width);
extern void purgeline(struct html_feed_environ *h_env);
extern void save_fonteffect(struct html_feed_environ *h_env,
                            struct readbuffer *obuf);
extern void restore_fonteffect(struct html_feed_environ *h_env,
                               struct readbuffer *obuf);
extern Str process_img(struct parsed_tag *tag, int width);
extern Str process_anchor(struct parsed_tag *tag, char *tagbuf);
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
extern int getMetaRefreshParam(char *q, Str *refresh_uri);

extern char *convert_size(int64_t size, int usefloat);
extern char *convert_size2(int64_t size1, int64_t size2, int usefloat);

extern void saveBuffer(struct Buffer *buf, FILE *f, int cont);
extern void saveBufferBody(struct Buffer *buf, FILE *f, int cont);
extern struct Buffer *getshell(char *cmd);
extern int save2tmp(struct URLFile uf, char *tmpf);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
extern int doFileMove(char *tmpf, char *defstr);
extern int checkCopyFile(char *path1, char *path2);
extern int checkSaveFile(InputStream stream, char *path);
extern int checkOverWrite(char *path);
extern int matchattr(char *p, char *attr, int len, Str *value);
extern void readHeader(struct URLFile *uf, struct Buffer *newBuf, int thru, struct Url *pu);
extern char *checkHeader(struct Buffer *buf, char *field);

extern struct Buffer *newBuffer(int width);
extern struct Buffer *nullBuffer(void);
extern void clearBuffer(struct Buffer *buf);
extern void discardBuffer(struct Buffer *buf);
extern struct Buffer *namedBuffer(struct Buffer *first, char *name);
extern struct Buffer *deleteBuffer(struct Buffer *first, struct Buffer *delbuf);
extern struct Buffer *replaceBuffer(struct Buffer *first, struct Buffer *delbuf, struct Buffer *newbuf);
extern struct Buffer *nthBuffer(struct Buffer *firstbuf, int n);
extern void gotoRealLine(struct Buffer *buf, int n);
extern void gotoLine(struct Buffer *buf, int n);
extern struct Buffer *selectBuffer(struct Buffer *firstbuf, struct Buffer *currentbuf,
                            char *selectchar);
extern void reshapeBuffer(struct Buffer *buf);
extern void copyBuffer(struct Buffer *a, struct Buffer *b);
extern struct Buffer *prevBuffer(struct Buffer *first, struct Buffer *buf);
extern int writeBufferCache(struct Buffer *buf);
extern int readBufferCache(struct Buffer *buf);
extern void displayBuffer(struct Buffer *buf, int mode);
extern void addChar(char c, Lineprop mode);
extern struct Buffer *message_list_panel(void);
extern void cursorUp0(struct Buffer *buf, int n);
extern void cursorUp(struct Buffer *buf, int n);
extern void cursorDown0(struct Buffer *buf, int n);
extern void cursorDown(struct Buffer *buf, int n);
extern void cursorUpDown(struct Buffer *buf, int n);
extern void cursorRight(struct Buffer *buf, int n);
extern void cursorLeft(struct Buffer *buf, int n);
extern void cursorHome(struct Buffer *buf);
extern void arrangeCursor(struct Buffer *buf);
extern void arrangeLine(struct Buffer *buf);
extern void cursorXY(struct Buffer *buf, int x, int y);
extern void restorePosition(struct Buffer *buf, struct Buffer *orig);
extern int columnSkip(struct Buffer *buf, int offset);
extern int columnPos(Line *line, int column);
extern int columnLen(Line *line, int column);
extern Line *lineSkip(struct Buffer *buf, Line *line, int offset, int last);
extern Line *currentLineSkip(struct Buffer *buf, Line *line, int offset, int last);
extern int gethtmlcmd(char **s);
#define checkType(a, b, c) _checkType(a, b)
extern Str checkType(Str s, Lineprop **oprop, Linecolor **ocolor);
extern int calcPosition(char *l, Lineprop *pr, int len, int pos, int bpos,
                        int mode);
extern char *lastFileName(char *path);
extern char *mybasename(char *s);
extern char *mydirname(char *s);
extern int next_status(char c, int *status);
extern int read_token(Str buf, char **instr, int *status, int pre, int append);
extern Str correct_irrtag(int status);
#define conv_search_string(str, f_ces) str
extern int forwardSearch(struct Buffer *buf, char *str);
extern int backwardSearch(struct Buffer *buf, char *str);
extern void pcmap(void);
extern void escmap(void);
extern void escbmap(void);
extern void escdmap(char c);
extern void multimap(void);
extern char *
inputLineHistSearch(const char *prompt, const char *def_str, int flag, Hist *hist,
                    int (*incfunc)(int ch, Str buf, Lineprop *prop));
extern Str unescape_spaces(Str s);
extern struct Buffer *historyBuffer(Hist *hist);
extern void loadHistory(Hist *hist);
extern void saveHistory(Hist *hist, size_t size);
extern void ldHist(void);
extern double log_like(int x);
extern struct table *newTable(void);
extern void pushdata(struct table *t, int row, int col, char *data);
extern int visible_length(char *str);
extern void align(TextLine *lbuf, int width, int mode);
extern void print_item(struct table *t, int row, int col, int width, Str buf);
extern void print_sep(struct table *t, int row, int type, int maxcol, Str buf);
extern void do_refill(struct table *tbl, int row, int col, int maxlimit);
extern void initRenderTable(void);
extern void renderTable(struct table *t, int max_width,
                        struct html_feed_environ *h_env);
extern struct table *begin_table(int border, int spacing, int padding,
                                 int vspace);
extern void end_table(struct table *tbl);
extern void check_rowcol(struct table *tbl, struct table_mode *mode);
extern int minimum_length(char *line);
extern int feed_table(struct table *tbl, char *line, struct table_mode *mode,
                      int width, int internal);
extern void feed_table1(struct table *tbl, Str tok, struct table_mode *mode,
                        int width);
extern void pushTable(struct table *, struct table *);
extern struct FormList *newFormList(char *action, char *method, char *charset,
                                     char *enctype, char *target, char *name,
                                     struct FormList *_next);
extern struct FormItemList *formList_addInput(struct FormList *fl,
                                                struct parsed_tag *tag);
extern char *form2str(struct FormItemList *fi);
extern int formtype(char *typestr);
extern void formRecheckRadio(Anchor *a, struct Buffer *buf, struct FormItemList *form);
extern void formResetBuffer(struct Buffer *buf, AnchorList *formitem);
extern void formUpdateBuffer(Anchor *a, struct Buffer *buf, struct FormItemList *form);
extern void preFormUpdateBuffer(struct Buffer *buf);
extern Str textfieldrep(Str s, int width);
extern void input_textarea(struct FormItemList *fi);
extern void do_internal(char *action, char *data);
extern void form_write_data(FILE *f, char *boundary, char *name, char *value);
extern void form_write_from_file(FILE *f, char *boundary, char *name,
                                 char *filename, char *file);
extern void follow_map(struct parsed_tagarg *arg);
extern struct Buffer *follow_map_panel(struct Buffer *buf, char *name);
extern Anchor *retrieveCurrentMap(struct Buffer *buf);
extern struct Buffer *page_info_panel(struct Buffer *buf);

extern char *ttyname_tty(void);
extern MySignalHandler reset_exit(SIGNAL_ARG);
extern MySignalHandler error_dump(SIGNAL_ARG);
extern void set_int(void);
extern void getTCstr(void);

extern void initMimeTypes();
extern void free_ssl_ctx();
extern struct Url *baseURL(struct Buffer *buf);
extern int openSocket(char *hostname, char *remoteport_name,
                      unsigned short remoteport_num);
extern void parseURL(char *url, struct Url *p_url, struct Url *current);
extern void copyParsedURL(struct Url *p, const struct Url *q);
extern void parseURL2(char *url, struct Url *pu, struct Url *current);
extern Str parsedURL2Str(struct Url *pu);
extern Str parsedURL2RefererStr(struct Url *pu);
extern int getURLScheme(char **url);
extern void init_stream(struct URLFile *uf, int scheme, InputStream stream);
struct HttpRequest;
extern struct URLFile openURL(char *url, struct Url *pu, struct Url *current,
                       URLOption *option, struct FormList *request,
                       TextList *extra_header, struct URLFile *ouf, struct HttpRequest *hr,
                       unsigned char *status);
extern int mailcapMatch(struct mailcap *mcap, char *type);
extern struct mailcap *searchMailcap(struct mailcap *table, char *type);
extern void initMailcap();
extern char *acceptableMimeTypes();
extern struct mailcap *searchExtViewer(char *type);
extern Str unquote_mailcap(char *qstr, char *type, char *name, char *attr,
                           int *mc_stat);
extern char *guessContentType(char *filename);
extern TextList *make_domain_list(char *domain_list);
extern int check_no_proxy(char *domain);
extern InputStream openFTPStream(struct Url *pu, struct URLFile *uf);
extern Str loadFTPDir0(struct Url *pu);
#define loadFTPDir(pu, charset) loadFTPDir0(pu)
extern void closeFTP(void);
extern void disconnectFTP(void);
extern AnchorList *putAnchor(AnchorList *al, char *url, char *target,
                             Anchor **anchor_return, char *referer, char *title,
                             unsigned char key, int line, int pos);
extern Anchor *registerHref(struct Buffer *buf, char *url, char *target, char *referer,
                            char *title, unsigned char key, int line, int pos);
extern Anchor *registerName(struct Buffer *buf, char *url, int line, int pos);
extern Anchor *registerImg(struct Buffer *buf, char *url, char *title, int line,
                           int pos);
extern Anchor *registerForm(struct Buffer *buf, struct FormList *flist,
                            struct parsed_tag *tag, int line, int pos);
extern int onAnchor(Anchor *a, int line, int pos);
extern Anchor *retrieveAnchor(AnchorList *al, int line, int pos);
extern Anchor *retrieveCurrentAnchor(struct Buffer *buf);
extern Anchor *retrieveCurrentImg(struct Buffer *buf);
extern Anchor *retrieveCurrentForm(struct Buffer *buf);
extern Anchor *searchAnchor(AnchorList *al, char *str);
extern Anchor *searchURLLabel(struct Buffer *buf, char *url);
extern void reAnchorWord(struct Buffer *buf, Line *l, int spos, int epos);
extern char *reAnchor(struct Buffer *buf, char *re);
extern void addMultirowsForm(struct Buffer *buf, AnchorList *al);
extern Anchor *closest_next_anchor(AnchorList *a, Anchor *an, int x, int y);
extern Anchor *closest_prev_anchor(AnchorList *a, Anchor *an, int x, int y);
extern HmarkerList *putHmarker(HmarkerList *ml, int line, int pos, int seq);
extern void shiftAnchorPosition(AnchorList *a, HmarkerList *hl, int line,
                                int pos, int shift);
extern char *getAnchorText(struct Buffer *buf, AnchorList *al, Anchor *a);
extern struct Buffer *link_list_panel(struct Buffer *buf);

extern Str decodeB(char **ww);
extern void decodeB_to_growbuf(struct growbuf *gb, char **ww);
extern Str decodeQ(char **ww);
extern Str decodeQP(char **ww);
extern void decodeQP_to_growbuf(struct growbuf *gb, char **ww);
extern Str decodeU(char **ww);
extern void decodeU_to_growbuf(struct growbuf *gb, char **ww);
extern Str decodeWord0(char **ow);
extern Str decodeMIME0(Str orgstr);
#define decodeWord(ow, charset) decodeWord0(ow)
#define decodeMIME(orgstr, charset) decodeMIME0(orgstr)
extern Str encodeB(char *a);
extern int set_param_option(char *option);
extern char *get_param_option(char *name);
extern void init_rc(void);
extern struct Buffer *load_option_panel(void);
extern void panel_set_option(struct parsed_tagarg *);
extern void sync_with_option(void);
extern char *rcFile(char *base);
extern char *etcFile(char *base);
extern char *confFile(char *base);
extern char *auxbinFile(char *base);
extern char *libFile(char *base);
extern char *helpFile(char *base);
extern const void *querySiteconf(const struct Url *query_pu, int field);
extern Str loadLocalDir(char *dirname);
extern FILE *openSecretFile(char *fname);
extern void loadPasswd(void);
extern void loadPreForm(void);
extern int find_auth_user_passwd(struct Url *pu, char *realm, Str *uname,
                                 Str *pwd, int is_proxy);
extern void add_auth_user_passwd(struct Url *pu, char *realm, Str uname, Str pwd,
                                 int is_proxy);
extern void invalidate_auth_user_passwd(struct Url *pu, char *realm, Str uname,
                                        Str pwd, int is_proxy);
extern char *last_modified(struct Buffer *buf);
extern Str romanNumeral(int n);
extern Str romanAlphabet(int n);
extern void myExec(char *command);
extern Str myExtCommand(char *cmd, char *arg, int redirect);
extern Str myEditor(char *cmd, char *file, int line);
extern int is_localhost(const char *host);
extern char *file_to_url(char *file);
extern char *url_unquote_conv0(char *url);
#define url_unquote_conv(url, charset) url_unquote_conv0(url)
extern Str tmpfname(int type, char *ext);
extern time_t mymktime(char *timestr);
extern void (*mySignal(int signal_number, void (*action)(int)))(int);
extern char *FQDN(char *host);
extern Str find_cookie(struct Url *pu);
extern int add_cookie(struct Url *pu, Str name, Str value, time_t expires,
                      Str domain, Str path, int flag, Str comment, int version,
                      Str port, Str commentURL);
extern void save_cookies(void);
extern void load_cookies(void);
extern void initCookie(void);
extern void cooLst(void);
extern struct Buffer *cookie_list_panel(void);
extern void set_cookie_flag(struct parsed_tagarg *arg);
extern int check_cookie_accept_domain(char *domain);
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
struct regex;
extern char *getRegexWord(const char **str, struct regex **regex_ret);

#define mainMn nulcmd
#define selMn selBuf
#define tabMn nulcmd

extern void dictword(void);
extern void dictwordat(void);
extern char *guess_save_name(struct Buffer *buf, char *file);

extern void wrapToggle(void);
extern void saveBufferInfo(void);

extern Str getLinkNumberStr(int correction);

extern void dispVer(void);

#ifdef USE_INCLUDED_SRAND48
void srand48(long);
long lrand48(void);
#endif

extern Str base64_encode(const unsigned char *src, size_t len);
