#pragma onece
#include "url.h"
#include "Str.h"
#include "constants.h"
#include "compression.h"
#include "http_client.h"
#include <time.h>
#include <openssl/crypto.h>

#define NO_REFERER ((char*)-1)

struct URLFile {
    struct Url url;
    bool is_cgi;
    struct InputStream* stream;
    enum ContentCompression compression;
    const char* guess_type;
    const char* ssl_certificate;
    time_t modtime;
};

struct InputStream;
struct URLFile init_stream(struct Url url, struct InputStream* stream);
struct URLFile examineFile(const char* path);
struct Url;
struct Form;
struct _textlist;
struct HttpRequest;
struct CmdArgs;

void UFclose(struct URLFile* f);

void free_ssl_ctx(void);

union input_handle;
void ssl_close(union input_handle* _handle);
int ssl_read(union input_handle* _handle, uint8_t* buf, int len);
// decompress and return tmpfile name
struct Uncompressed {
    FILE* pipe;
    const char* tmpf;
};
struct Uncompressed uncompressed_pipe(struct URLFile* uf, struct CompressionDecoder* d);
