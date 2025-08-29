#pragma once
#include <openssl/crypto.h>

extern int ssl_verify_server;
extern char* ssl_cert_file;
extern char* ssl_key_file;
extern char* ssl_ca_path;
extern char* ssl_ca_file;
extern int ssl_ca_default;
extern int ssl_path_modified;
extern char* ssl_forbid_method;
extern char* ssl_min_version;
extern char* ssl_cipher;

SSL* openSSLHandle(int sock, char* hostname, char** p_cert);
void SSL_write_from_file(SSL* ssl, char* file);
