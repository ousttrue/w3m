#pragma aonce
#include <Str.h>

extern int FollowRedirection;
extern char DecodeCTE;
extern int label_topline;
extern int UseExternalDirBuffer;
extern char* DefaultType;
extern int displayLinkNumber;
extern char SimplePreserveSpace;
extern int squeezeBlankLine;

#define CGI_EXTENSION ".cgi"
// #define CGI_EXTENSION ".cmd"
extern char* DirBufferCommand;

#define set_prevchar(x, y, n) Strcopy_charp_n((x), (y), (n))
#define set_space_to_prevchar(x) Strcopy_charp_n((x), " ", 1)

struct _Buffer;
struct _ParsedURL;
struct form_list;
struct HtmlTagParsed;

struct _Buffer* loadGeneralFile(char* path, struct _ParsedURL* current, const char* referer, int flag, struct form_list* request, bool do_download);
int is_boundary(unsigned char*, unsigned char*);

struct _Buffer* loadHTMLString(Str page);

struct URLFile;
void loadHTMLstream(struct URLFile* f, struct _Buffer* newBuf, FILE* src, int internal);
