#pragma aonce
#include <Str.h>
#include "textlist.h"

struct _Buffer;
struct _ParsedURL;
struct form_list;
struct html_feed_environ;
struct readbuffer;
struct parsed_tag;
struct environment;

char* acceptableEncoding(void);
int dir_exist(char* path);
int is_html_type(char* type);
char* inputAnswer(char* prompt);
struct _Buffer* loadGeneralFile(char* path, struct _ParsedURL* current, const char* referer, int flag, struct form_list* request);
int is_boundary(unsigned char*, unsigned char*);
void push_render_image(Str str, int width, int limit, struct html_feed_environ* h_env);
void flushline(struct html_feed_environ* h_env, struct readbuffer* obuf,
    int indent, int force, int width);
void do_blankline(struct html_feed_environ* h_env, struct readbuffer* obuf, int indent, int indent_incr, int width);
void purgeline(struct html_feed_environ* h_env);
void save_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf);
void restore_fonteffect(struct html_feed_environ* h_env, struct readbuffer* obuf);
Str process_img(struct parsed_tag* tag, int width);
Str process_anchor(struct parsed_tag* tag, char* tagbuf);
Str process_input(struct parsed_tag* tag);
Str process_button(struct parsed_tag* tag);
Str process_n_button(void);
Str process_select(struct parsed_tag* tag);
Str process_n_select(void);
void feed_select(char* str);
void process_option(void);
Str process_textarea(struct parsed_tag* tag, int width);
Str process_n_textarea(void);
void feed_textarea(char* str);
Str process_form(struct parsed_tag* tag);
Str process_n_form(void);
int getMetaRefreshParam(char* q, Str* refresh_uri);
int HTMLtagproc1(struct parsed_tag* tag, struct html_feed_environ* h_env);
void HTMLlineproc2(struct _Buffer* buf, TextLineList* tl);
void HTMLlineproc0(char* istr, struct html_feed_environ* h_env, int internal);
#define HTMLlineproc1(x, y) HTMLlineproc0(x, y, TRUE)
char* convert_size(long long size, int usefloat);
char* convert_size2(long long size1, long long size2, int usefloat);
void showProgress(long long* linelen, long long* trbyte);
void init_henv(struct html_feed_environ*, struct readbuffer*,
    struct environment*, int, TextLineList*, int, int);
void completeHTMLstream(struct html_feed_environ*,
    struct readbuffer*);
struct _Buffer* loadHTMLString(Str page);
void saveBuffer(struct _Buffer* buf, FILE* f, int cont);
void saveBufferBody(struct _Buffer* buf, FILE* f, int cont);
struct _Buffer* getshell(char* cmd);
struct _Buffer* getpipe(char* cmd);
int _doFileCopy(char* tmpf, char* defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
int doFileMove(char* tmpf, char* defstr);
int checkCopyFile(char* path1, char* path2);
int checkOverWrite(char* path);
int matchattr(char* p, char* attr, int len, Str* value);
char* checkHeader(struct _Buffer* buf, char* field);
char* guess_save_name(struct _Buffer* buf, char* file);
Str getLinkNumberStr(int correction);
