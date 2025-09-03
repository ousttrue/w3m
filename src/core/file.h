#pragma aonce
#include <Str.h>
#include "textlist.h"
#include "line.h"

#define set_prevchar(x, y, n) Strcopy_charp_n((x), (y), (n))
#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

struct _Buffer;
struct _ParsedURL;
struct form_list;
struct HtmlTagParsed;

int dir_exist(char* path);
int is_html_type(char* type);
char* inputAnswer(char* prompt);
struct _Buffer* loadGeneralFile(char* path, struct _ParsedURL* current, const char* referer, int flag, struct form_list* request);
int is_boundary(unsigned char*, unsigned char*);

struct _Buffer* loadHTMLString(Str page);
void saveBuffer(struct _Buffer* buf, FILE* f, int cont);
void saveBufferBody(struct _Buffer* buf, FILE* f, int cont);
struct _Buffer* getshell(char* cmd);
int _doFileCopy(char* tmpf, char* defstr, int download);
#define doFileCopy(tmpf, defstr) _doFileCopy(tmpf, defstr, FALSE);
int doFileMove(char* tmpf, char* defstr);
int checkCopyFile(char* path1, char* path2);
int checkOverWrite(char* path);
char* checkHeader(struct _Buffer* buf, char* field);
char* guess_save_name(struct _Buffer* buf, char* file);
