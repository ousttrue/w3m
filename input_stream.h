#pragma once
#include <stdio.h>

enum InputStreamType {
    IST_BUFFER = 0,
    IST_FD = 1,
    IST_FILE = 2,
    IST_SSL = 3,
    IST_ENCODED = 4,
};

struct InputStream;
struct ssl_st;

struct InputStream* ist_from_fd(int des);
struct InputStream* ist_from_path(const char* path);
struct InputStream* ist_from_fp(FILE* f, int (*closep)(FILE*));
struct InputStream* ist_from_buffer(const char* s, int len);
struct InputStream* ist_from_ssl(struct ssl_st* ssl, int sock);

enum InputStreamType ist_type(struct InputStream* stream);
bool ist_close(struct InputStream* stream);
void ist_set_unclose(struct InputStream* stream, bool unclose);
int ist_getc(struct InputStream* stream);
int ist_peek(struct InputStream* stream);
int ist_read(struct InputStream* stream, char* dst, int bufsize);
int ist_fd(struct InputStream* stream);
int ist_eos(struct InputStream* stream);
