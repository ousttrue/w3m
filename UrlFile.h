#pragma onece
#include "url_scheme.h"
#include "Str.h"
#include "constants.h"
#include "compression.h"
#include <time.h>
#include <openssl/crypto.h>

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

struct URLFile {
    enum UrlScheme scheme;
    bool is_cgi;
    struct InputStream* stream;
    const char* ext;
    enum ContentCompression compression;
    const char* guess_type;
    const char* ssl_certificate;
    const char* url;
    time_t modtime;
};

struct InputStream;
struct URLFile init_stream(enum UrlScheme scheme, struct InputStream* stream);
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

Str ssl_get_certificate(struct CmdArgs* args, SSL* ssl, const char* hostname);
void free_ssl_ctx(void);

union input_handle;
void ssl_close(union input_handle* _handle);
int ssl_read(union input_handle* _handle, uint8_t* buf, int len);
void uncompress_and_reopen(struct URLFile* uf, struct CompressionDecoder* d, const char** src);
