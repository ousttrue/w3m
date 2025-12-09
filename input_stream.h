#pragma once
#include "Url.h"
#include "HttpRequest.h"
#include <gcstr/alloc.h>
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define StrUFgets(f) StrISgets((f)->stream)
#define StrmyUFgets(f) StrmyISgets((f)->stream)
#define UFgetc(f) ISgetc((f)->stream)
#define UFundogetc(f) ISundogetc((f)->stream)
#define UFclose(f)                   \
    if (ISclose((f)->stream) == 0) { \
        (f)->stream = NULL;          \
    }
#define UFfileno(f) ISfileno((f)->stream)

struct stream_buffer {
    unsigned char* buf;
    int size, cur, next;
};

typedef struct stream_buffer* StreamBuffer;

struct io_file_handle {
    FILE* f;
    void (*close)(void*);
};

struct ssl_handle {
    SSL* ssl;
    int sock;
};

union input_stream;

struct ens_handle {
    union input_stream* is;
    struct growbuf gb;
    int pos;
    char encoding;
};

struct base_stream {
    struct stream_buffer stream;
    void* handle;
    char type;
    char iseos;
    int (*read)(void*, void*, int);
    void (*close)(void*);
    bool unclose;
};

struct file_stream {
    struct stream_buffer stream;
    struct io_file_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

struct str_stream {
    struct stream_buffer stream;
    Str handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

struct ssl_stream {
    struct stream_buffer stream;
    struct ssl_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

struct encoded_stream {
    struct stream_buffer stream;
    struct ens_handle* handle;
    char type;
    char iseos;
    int (*read)();
    void (*close)();
    bool unclose;
};

union input_stream {
    struct base_stream base;
    struct file_stream file;
    struct str_stream str;
    struct ssl_stream ssl;
    struct encoded_stream ens;
};

typedef struct base_stream* BaseStream;
typedef struct file_stream* FileStream;
typedef struct str_stream* StrStream;
typedef struct ssl_stream* SSLStream;
typedef struct encoded_stream* EncodedStrStream;

extern union input_stream* newInputStream(int des);
extern union input_stream* newFileStream(FILE* f, void (*closep)());
extern union input_stream* newStrStream(Str s);
extern union input_stream* newSSLStream(SSL* ssl, int sock);
extern union input_stream* newEncodedStream(union input_stream* is, char encoding);
extern int ISclose(union input_stream* stream);
extern int ISgetc(union input_stream* stream);
extern int ISundogetc(union input_stream* stream);
extern Str StrISgets2(union input_stream* stream, char crnl);
inline static Str StrISgets(union input_stream* stream) { return StrISgets2(stream, false); }
inline static Str StrmyISgets(union input_stream* stream) { return StrISgets2(stream, true); }
void ISgets_to_growbuf(union input_stream* stream, struct growbuf* gb, char crnl);
int ISread_n(union input_stream* stream, char* dst, int bufsize);
extern int ISfileno(union input_stream* stream);
extern int ISeos(union input_stream* stream);
extern void ssl_accept_this_site(char* hostname);
extern Str ssl_get_certificate(SSL* ssl, char* hostname);

#define IST_BASIC 0
#define IST_FILE 1
#define IST_STR 2
#define IST_SSL 3
#define IST_ENCODED 4

#define IStype(stream) ((stream)->base.type)

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

union input_stream;
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
int check_no_proxy(char* domain);
