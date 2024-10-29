#pragma once
#include "input/istream.h"
#include "input/stream_buffer.h"
#include <openssl/types.h>

struct ssl_handle {
  SSL *ssl;
  int sock;
};

struct ssl_stream {
  struct stream_buffer stream;
  struct ssl_handle *handle;
  enum IST_TYPE type;
  bool unclose;
  bool iseos;
  ReadFunc read;
  CloseFunc close;
  const char *ssl_certificate;
};

extern bool ssl_verify_server;
extern const char *ssl_cert_file;
extern const char *ssl_key_file;
extern const char *ssl_ca_path;
extern const char *ssl_ca_file;
extern bool ssl_ca_default;
extern bool ssl_path_modified;
extern const char *ssl_forbid_method;
extern const char *ssl_min_version;
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
extern const char *ssl_cipher;
#else
extern const char *ssl_cipher;
#endif

SSL *openSSLHandle(int sock, char *hostname, char **p_cert);
void SSL_write_from_file(SSL *ssl, const char *file);
void ssl_close(void *_handle);
int ssl_read(void *_handle, unsigned char *buf, int len);
void free_ssl_ctx();
