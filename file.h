#pragma once
#include "Url.h"
#include "istream.h"
#include <wc.h>
#include <gcstr/gcstr.h>

struct _Buffer;
struct form_list;
struct _Buffer* loadGeneralFile(char* path, struct Url* current, char* referer, enum LoadGeneralFlags flag, struct form_list* request);
wc_ces url_to_charset(const char* url, const struct Url* base,
    wc_ces doc_charset);
char* url_encode(const char* url, const struct Url* base,
    wc_ces doc_charset);
struct URLFile;
Str convertLine(struct URLFile* uf, Str line, int mode, wc_ces* charset, wc_ces doc_charset);
struct parsed_tag;
extern Str process_img(struct parsed_tag* tag, int width);
extern Str process_anchor(struct parsed_tag* tag, char* tagbuf);
extern Str process_input(struct parsed_tag* tag);
extern Str process_button(struct parsed_tag* tag);
extern Str process_n_button(void);
extern Str process_select(struct parsed_tag* tag);
extern Str process_n_select(void);
extern void feed_select(char* str);
extern void process_option(void);
extern Str process_textarea(struct parsed_tag* tag, int width);
extern Str process_n_textarea(void);
extern void feed_textarea(char* str);
extern Str process_form(struct parsed_tag* tag);
extern Str process_n_form(void);
extern int getMetaRefreshParam(char* q, Str* refresh_uri);
extern Str loadGopherDir(struct URLFile* uf, struct Url* pu, wc_ces* charset);
extern Str loadGopherSearch(struct URLFile* uf, struct Url* pu, wc_ces* charset);
extern Str getLinkNumberStr(int correction);
extern char* inputAnswer(char* prompt);
extern void examineFile(char* path, struct URLFile* uf);
extern int is_boundary(unsigned char*, unsigned char*);
extern void pushEvent(int cmd, void* data);
extern int checkOverWrite(char* path);
extern int doFileMove(char* tmpf, char* defstr);
extern int doFileSave(struct URLFile uf, char* defstr);
extern int _doFileCopy(char* tmpf, char* defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
extern int save2tmp(struct URLFile uf, char* tmpf);
extern int checkCopyFile(char* path1, char* path2);
extern int gethtmlcmd(char** s);
char* acceptableEncoding(void);
