#pragma once
#include "Str.h"
#include <openssl/types.h>

void free_ssl_ctx(void);
void ssl_accept_this_site(const char* hostname);
Str ssl_get_certificate(SSL* ssl, const char* hostname);
SSL* openSSLHandle(int sock, const char* hostname, const char** p_cert);
void SSL_write_from_file(SSL* ssl, const char* file);

struct ssl_handle {
    SSL* ssl;
    int sock;
};
void ssl_close(struct ssl_handle* handle);
int ssl_read(struct ssl_handle* handle, char* buf, int len);

