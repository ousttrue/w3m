#pragma once
#include <openssl/ssl.h>
#include <string>

#define USE_SSL 1
#define USE_SSL_VERIFY 1

extern const char *ssl_forbid_method;
extern bool ssl_path_modified;
extern const char *ssl_cipher;
extern const char *ssl_cert_file;
extern const char *ssl_key_file;
extern const char *ssl_ca_path;
extern const char *ssl_ca_file;
extern bool ssl_ca_default;
extern bool ssl_verify_server;
extern char *ssl_min_version;

struct SslConnection {
  SSL *handle = nullptr;
  std::string cert;
};

SslConnection openSSLHandle(int sock, const char *hostname);
void free_ssl_ctx();
void SSL_write_from_file(SSL *ssl, const char *file);
