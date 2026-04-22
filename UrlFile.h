#include <openssl/crypto.h>
#pragma onece
#include "url_scheme.h"
#include "Str.h"
#include "line.h"
#include "stream_encoding.h"
#include <time.h>

#define NO_REFERER ((char*)-1)

enum UrlOptionFlags {
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};
struct URLOption {
    const char* referer;
    enum UrlOptionFlags flag;
};

enum ContentCompression {
    CMP_NOCOMPRESS = 0,
    CMP_COMPRESS = 1,
    CMP_GZIP = 2,
    CMP_BZIP2 = 3,
    CMP_DEFLATE = 4,
    CMP_BROTLI = 5,
};

struct URLFile {
    enum UrlScheme scheme;
    bool is_cgi;
    enum StreamEncoding encoding;
    union input_stream* stream;
    const char* ext;
    enum ContentCompression compression;
    int content_encoding;
    const char* guess_type;
    const char* ssl_certificate;
    const char* url;
    time_t modtime;
};

typedef union input_stream* InputStream;
struct URLFile init_stream(enum UrlScheme scheme, InputStream stream);
struct URLFile examineFile(const char* path);
struct Url;
struct Form;
struct _textlist;
struct HttpRequest;
struct CmdArgs;
struct URLFile openURL(struct CmdArgs* args, const char* url, struct Url* pu, struct Url* current,
    struct URLOption* option, struct Form* request,
    struct _textlist* extra_header, struct URLFile* ouf,
    struct HttpRequest* hr, unsigned char* status);

void UFclose(struct URLFile* f);
void UFhalfclose(struct URLFile* f);
Str convertLine(struct URLFile* uf, Str line, enum LineMode mode, wc_ces* charset, wc_ces doc_charset);
void check_compression(struct URLFile* uf, const char* path);
const char* uncompressed_file_type(const char* path, const char** ext);
const char* compress_application_type(enum ContentCompression compression);
const char* acceptableEncoding(void);
void parseCompression(struct URLFile* uf, const char* p);
void uncompress_stream(struct URLFile* uf, const char** src);
Str ssl_get_certificate(struct CmdArgs* args, SSL* ssl, const char* hostname);
void free_ssl_ctx(void);
void ssl_accept_this_site(const char* hostname);
