#pragma once
#include "constants.h"
#include <stdio.h>
#include <stdint.h>

enum InputStreamType {
    IST_BUFFER = 0,
    IST_FILE_DESC = 1,
    IST_FILE_PIPE = 2,
    IST_SOCK = 3,
    IST_ENCODED = 4,
};

struct InputStream;
struct ssl_st;

struct InputStream* ist_from_path(const char* path);
typedef int(*FpCloseFunc)(FILE*);
struct InputStream* ist_from_fp(FILE* f, FpCloseFunc func);
struct InputStream* ist_from_buffer(const char* s, int len);
struct InputStream* ist_from_socket(int sock, struct ssl_st* ssl);
struct InputStream* ist_decode(struct InputStream* is, enum StreamEncoding encoding);

/// return false if set unclose
bool ist_destroy(struct InputStream* ist);

enum InputStreamType ist_type(struct InputStream* ist);
void ist_close(struct InputStream* ist);
void ist_set_unclose(struct InputStream* ist, bool unclose);
int ist_getc(struct InputStream* ist);
int ist_peek(struct InputStream* ist);
int ist_read(struct InputStream* ist, uint8_t* dst, int bufsize);
int ist_fd(struct InputStream* ist);
bool ist_eos(struct InputStream* ist);
struct growbuf;
void ist_gets_to_growbuf(struct InputStream* stream, struct growbuf* gb, bool check_crnl);
