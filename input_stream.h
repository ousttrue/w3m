#pragma once
#include "ist.h"
#include "Url.h"
#include "HttpRequest.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct URLFile {
    unsigned char scheme;
    char is_cgi;
    char encoding;
    union input_stream* stream;
    const char* ext;
    int compression;
    int content_encoding;
    const char* guess_type;
    const char* ssl_certificate;
    const char* url;
    time_t modtime;
};
inline static int UFgetc(struct URLFile* f)
{
    return ISgetc(f->stream);
}

extern union input_stream* newFileStream(FILE* f, void (*closep)());
extern union input_stream* newStrStream(Str s);
extern union input_stream* newSSLStream(SSL* ssl, int sock);
extern union input_stream* newEncodedStream(union input_stream* is, char encoding);
static inline void UFclose(struct URLFile* f)
{
    if (ISclose(f->stream) == 0) {
        (f)->stream = NULL;
    }
}

extern int ISundogetc(union input_stream* stream);
static inline int UFundogetc(struct URLFile* f)
{
    return ISundogetc(f->stream);
}
inline static Str StrUFgets(struct URLFile* f)
{
    return StrISgets(f->stream);
}

inline static Str StrmyUFgets(struct URLFile* f) { return StrmyISgets(f->stream); }
extern int ISfileno(union input_stream* stream);
static inline int UFfileno(struct URLFile* f) { return ISfileno(f->stream); }
extern void ssl_accept_this_site(char* hostname);
extern Str ssl_get_certificate(SSL* ssl, char* hostname);

inline static enum InputStreamType IStype(union input_stream* stream)
{
    return stream->base.type;
}

static inline bool iseos(union input_stream* stream)
{
    return ((stream)->base.iseos);
}

static inline int ssl_socket_of(union input_stream* stream)
{
    return ((stream)->ssl.handle->sock);
}

static inline union input_stream* openIS(const char* path)
{
    return newInputStream(open((path), O_RDONLY));
}

enum LoadGeneralFlags {
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};

struct UrlOption {
    char* referer;
    enum LoadGeneralFlags flag;
};

extern struct URLFile openURL(char* url, struct Url* pu, struct Url* current,
    struct UrlOption* option, struct form_list* request,
    TextList* extra_header, struct URLFile* ouf,
    struct HttpRequest* hr, unsigned char* status);

int checkSaveFile(union input_stream* stream, char* path);
void init_stream(struct URLFile* uf, int scheme, union input_stream* stream);
union input_stream* openFTPStream(struct Url* pu, struct URLFile* uf);
union input_stream* openNewsStream(struct Url* pu);
int openSocket(char* hostname, char* remoteport_name, unsigned short remoteport_num);
void free_ssl_ctx(void);
int check_no_proxy(const char* domain);
