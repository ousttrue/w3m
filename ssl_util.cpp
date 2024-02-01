#include "ssl_util.h"
#include "message.h"
// #include "terms.h"
// #include "linein.h"
#include "Str.h"
#include "config.h"
#include "myctype.h"
#include "indep.h"
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#ifndef SSLEAY_VERSION_NUMBER
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#endif
#include <openssl/err.h>

const char *ssl_forbid_method = "2, 3, t, 5";
bool ssl_path_modified = false;

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
const char *ssl_cipher "DEFAULT:!LOW:!RC4:!EXP";
#else
const char *ssl_cipher = nullptr;
#endif

const char *ssl_cert_file = nullptr;
const char *ssl_key_file = nullptr;
const char *ssl_ca_path = nullptr;
const char *ssl_ca_file = DEF_CAFILE;
bool ssl_ca_default = true;

static Str *accept_this_site;
void ssl_accept_this_site(const char *hostname) {
  if (hostname)
    accept_this_site = Strnew_charp(hostname);
  else
    accept_this_site = nullptr;
}

SSL_CTX *ssl_ctx = nullptr;
bool ssl_verify_server = true;
#ifdef SSL_CTX_set_min_proto_version
char *ssl_min_version = NULL;
static int str_to_ssl_version(const char *name) {
  if (!strcasecmp(name, "all"))
    return 0;
  if (!strcasecmp(name, "none"))
    return 0;
#ifdef TLS1_3_VERSION
  if (!strcasecmp(name, "TLSv1.3"))
    return TLS1_3_VERSION;
#endif
#ifdef TLS1_2_VERSION
  if (!strcasecmp(name, "TLSv1.2"))
    return TLS1_2_VERSION;
#endif
#ifdef TLS1_1_VERSION
  if (!strcasecmp(name, "TLSv1.1"))
    return TLS1_1_VERSION;
#endif
  if (!strcasecmp(name, "TLSv1.0"))
    return TLS1_VERSION;
  if (!strcasecmp(name, "TLSv1"))
    return TLS1_VERSION;
  if (!strcasecmp(name, "SSLv3.0"))
    return SSL3_VERSION;
  if (!strcasecmp(name, "SSLv3"))
    return SSL3_VERSION;
  return -1;
}
#endif /* SSL_CTX_set_min_proto_version */

#if SSLEAY_VERSION_NUMBER >= 0x00905100
#include <openssl/rand.h>
static void init_PRNG(void) {
  char buffer[256];
  const char *file;
  long l;
  if (RAND_status())
    return;
  if ((file = RAND_file_name(buffer, sizeof(buffer)))) {
#ifdef USE_EGD
    if (RAND_egd(file) > 0)
      return;
#endif
    RAND_load_file(file, -1);
  }
  if (RAND_status())
    goto seeded;
  srand48((long)time(NULL));
  while (!RAND_status()) {
    l = lrand48();
    RAND_seed((unsigned char *)&l, sizeof(long));
  }
seeded:
  if (file)
    RAND_write_file(file);
}
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */

static int ssl_match_cert_ident(const char *ident, int ilen,
                                const char *hostname) {
  /* RFC2818 3.1.  Server Identity
   * Names may contain the wildcard
   * character * which is considered to match any single domain name
   * component or component fragment. E.g., *.a.com matches foo.a.com but
   * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
   */
  int hlen = strlen(hostname);
  int i, c;

  /* Is this an exact match? */
  if ((ilen == hlen) && strncasecmp(ident, hostname, hlen) == 0)
    return TRUE;

  for (i = 0; i < ilen; i++) {
    if (ident[i] == '*' && ident[i + 1] == '.') {
      while ((c = *hostname++) != '\0')
        if (c == '.')
          break;
      i++;
    } else {
      if (ident[i] != *hostname++)
        return FALSE;
    }
  }
  return *hostname == '\0';
}

static Str *ssl_check_cert_ident(X509 *x, const char *hostname) {
  int i;
  Str *ret = NULL;
  int match_ident = false;
  /*
   * All we need to do here is check that the CN matches.
   *
   * From RFC2818 3.1 Server Identity:
   * If a subjectAltName extension of type dNSName is present, that MUST
   * be used as the identity. Otherwise, the (most specific) Common Name
   * field in the Subject field of the certificate MUST be used. Although
   * the use of the Common Name is existing practice, it is deprecated and
   * Certification Authorities are encouraged to use the dNSName instead.
   */
  i = X509_get_ext_by_NID(x, NID_subject_alt_name, -1);
  if (i >= 0) {
    X509_EXTENSION *ex;
    STACK_OF(GENERAL_NAME) * alt;

    ex = X509_get_ext(x, i);
    alt = (struct stack_st_GENERAL_NAME *)X509V3_EXT_d2i(ex);
    if (alt) {
      int n;
      GENERAL_NAME *gn;
      Str *seen_dnsname = NULL;

      n = sk_GENERAL_NAME_num(alt);
      for (i = 0; i < n; i++) {
        gn = sk_GENERAL_NAME_value(alt, i);
        if (gn->type == GEN_DNS) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
          unsigned char *sn = ASN1_STRING_data(gn->d.ia5);
#else
          const unsigned char *sn = ASN1_STRING_get0_data(gn->d.ia5);
#endif
          int sl = ASN1_STRING_length(gn->d.ia5);

          /*
           * sn is a pointer to internal data and not guaranteed to
           * be null terminated. Ensure we have a null terminated
           * string that we can modify.
           */
          auto asn = (char *)GC_MALLOC(sl + 1);
          if (!asn)
            exit(1);
          bcopy(sn, asn, sl);
          asn[sl] = '\0';

          if (!seen_dnsname)
            seen_dnsname = Strnew();
          /* replace \0 to make full string visible to user */
          if (sl != strlen(asn)) {
            int i;
            for (i = 0; i < sl; ++i) {
              if (!asn[i])
                asn[i] = '!';
            }
          }
          Strcat_m_charp(seen_dnsname, asn, " ", NULL);
          if (sl == strlen(asn) /* catch \0 in SAN */
              && ssl_match_cert_ident(asn, sl, hostname))
            break;
        }
      }
      X509V3_EXT_get(ex);
      sk_GENERAL_NAME_free(alt);
      if (i < n) /* Found a match */
        match_ident = TRUE;
      else if (seen_dnsname)
        /* FIXME: gettextize? */
        ret = Sprintf("Bad cert ident from %s: dNSName=%s", hostname,
                      seen_dnsname->ptr);
    }
  }

  if (match_ident == FALSE && ret == NULL) {
    X509_NAME *xn;
    char buf[2048];
    int slen;

    xn = X509_get_subject_name(x);

    slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
    if (slen == -1)
      /* FIXME: gettextize? */
      ret = Strnew_charp("Unable to get common name from peer cert");
    else if (slen != strlen(buf) ||
             !ssl_match_cert_ident(buf, strlen(buf), hostname)) {
      /* replace \0 to make full string visible to user */
      if (slen != strlen(buf)) {
        int i;
        for (i = 0; i < slen; ++i) {
          if (!buf[i])
            buf[i] = '!';
        }
      }
      /* FIXME: gettextize? */
      ret = Sprintf("Bad cert ident %s from %s", buf, hostname);
    }
  }
  return ret;
}
static Str *ssl_get_certificate(SSL *ssl, const char *hostname) {
  BIO *bp;
  X509 *x;
  X509_NAME *xn;
  char *p;
  int len;
  Str *s;
  char buf[2048];
  Str *amsg = NULL;
  Str *emsg;
  const char *ans;

  if (ssl == NULL)
    return NULL;
  x = SSL_get_peer_certificate(ssl);
  if (x == NULL) {
    if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
      ans = "y";
    else {
      // emsg = Strnew_charp("No SSL peer certificate: accept? (y/n)");
      // ans = inputAnswer(emsg->ptr);
      ans = "y";
    }
    if (ans && TOLOWER(*ans) == 'y')
      amsg = Strnew_charp("Accept SSL session without any peer certificate");
    else {
      const char *e = "This SSL session was rejected "
                      "to prevent security violation: no peer certificate";
      disp_err_message(e, false);
      free_ssl_ctx();
      return NULL;
    }
    if (amsg)
      disp_err_message(amsg->ptr, false);
    ssl_accept_this_site(hostname);
    s = amsg ? amsg : Strnew_charp("valid certificate");
    return s;
  }

  /* check the cert chain.
   * The chain length is automatically checked by OpenSSL when we
   * set the verify depth in the ctx.
   */
  if (ssl_verify_server) {
    long verr;
    if ((verr = SSL_get_verify_result(ssl)) != X509_V_OK) {
      const char *em = X509_verify_cert_error_string(verr);
      if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
        ans = "y";
      else {
        // emsg = Sprintf("%s: accept? (y/n)", em);
        // ans = inputAnswer(emsg->ptr);
        ans = "y";
      }
      if (ans && TOLOWER(*ans) == 'y') {
        amsg = Sprintf("Accept unsecure SSL session: "
                       "unverified: %s",
                       em);
      } else {
        char *e = Sprintf("This SSL session was rejected: %s", em)->ptr;
        disp_err_message(e, false);
        free_ssl_ctx();
        return NULL;
      }
    }
  }

  emsg = ssl_check_cert_ident(x, hostname);
  if (emsg != NULL) {
    if (accept_this_site && strcasecmp(accept_this_site->ptr, hostname) == 0)
      ans = "y";
    else {
      // Str *ep = Strdup(emsg);
      // if (ep->length > COLS - 16)
      //   Strshrink(ep, ep->length - (COLS - 16));
      // Strcat_charp(ep, ": accept? (y/n)");
      // ans = inputAnswer(ep->ptr);
      ans = "y";
    }

    if (ans && TOLOWER(*ans) == 'y') {
      amsg = Strnew_charp("Accept unsecure SSL session:");
      Strcat(amsg, emsg);
    } else {
      const char *e = "This SSL session was rejected "
                      "to prevent security violation";
      disp_err_message(e, FALSE);
      free_ssl_ctx();
      return NULL;
    }
  }
  if (amsg) {
    disp_err_message(amsg->ptr, FALSE);
  }

  ssl_accept_this_site(hostname);
  s = amsg ? amsg : Strnew_charp("valid certificate");
  Strcat_charp(s, "\n");
  xn = X509_get_subject_name(x);
  if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
    Strcat_charp(s, " subject=<unknown>");
  else
    Strcat_m_charp(s, " subject=", buf, NULL);
  xn = X509_get_issuer_name(x);
  if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
    Strcat_charp(s, ": issuer=<unknown>");
  else
    Strcat_m_charp(s, ": issuer=", buf, NULL);
  Strcat_charp(s, "\n\n");

  bp = BIO_new(BIO_s_mem());
  X509_print(bp, x);
  len = (int)BIO_ctrl(bp, BIO_CTRL_INFO, 0, (char *)&p);
  Strcat_charp_n(s, p, len);
  BIO_free_all(bp);
  X509_free(x);
  return s;
}

SSL *openSSLHandle(int sock, const char *hostname, char **p_cert) {
  SSL *handle = nullptr;
  static const char *old_ssl_forbid_method = nullptr;
  static int old_ssl_verify_server = -1;

  if (old_ssl_forbid_method != ssl_forbid_method &&
      (!old_ssl_forbid_method || !ssl_forbid_method ||
       strcmp(old_ssl_forbid_method, ssl_forbid_method))) {
    old_ssl_forbid_method = ssl_forbid_method;
    ssl_path_modified = 1;
  }
  if (old_ssl_verify_server != ssl_verify_server) {
    old_ssl_verify_server = ssl_verify_server;
    ssl_path_modified = 1;
  }
  if (ssl_path_modified) {
    free_ssl_ctx();
    ssl_path_modified = 0;
  }
  if (ssl_ctx == NULL) {
    int option;
#if OPENSSL_VERSION_NUMBER < 0x0800
    ssl_ctx = SSL_CTX_new();
    X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
    SSLeay_add_ssl_algorithms();
    SSL_load_error_strings();
#else
    OPENSSL_init_ssl(0, NULL);
#endif
    if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
      goto eend;
#ifdef SSL_CTX_set_min_proto_version
    if (ssl_min_version && *ssl_min_version != '\0') {
      int sslver;
      sslver = str_to_ssl_version(ssl_min_version);
      if (sslver < 0 || !SSL_CTX_set_min_proto_version(ssl_ctx, sslver)) {
        free_ssl_ctx();
        goto eend;
      }
    }
#endif
    if (ssl_cipher && *ssl_cipher != '\0')
      if (!SSL_CTX_set_cipher_list(ssl_ctx, ssl_cipher)) {
        free_ssl_ctx();
        goto eend;
      }
    option = SSL_OP_ALL;
    if (ssl_forbid_method) {
      if (strchr(ssl_forbid_method, '2'))
        option |= SSL_OP_NO_SSLv2;
      if (strchr(ssl_forbid_method, '3'))
        option |= SSL_OP_NO_SSLv3;
      if (strchr(ssl_forbid_method, 't'))
        option |= SSL_OP_NO_TLSv1;
      if (strchr(ssl_forbid_method, 'T'))
        option |= SSL_OP_NO_TLSv1;
      if (strchr(ssl_forbid_method, '4'))
        option |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_1
      if (strchr(ssl_forbid_method, '5'))
        option |= SSL_OP_NO_TLSv1_1;
#endif
#ifdef SSL_OP_NO_TLSv1_2
      if (strchr(ssl_forbid_method, '6'))
        option |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_NO_TLSv1_3
      if (strchr(ssl_forbid_method, '7'))
        option |= SSL_OP_NO_TLSv1_3;
#endif
    }
#ifdef SSL_OP_NO_COMPRESSION
    option |= SSL_OP_NO_COMPRESSION;
#endif
    SSL_CTX_set_options(ssl_ctx, option);

#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

    /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
    SSL_CTX_set_verify(
        ssl_ctx, ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
    if (ssl_cert_file != NULL && *ssl_cert_file != '\0') {
      int ng = 1;
      if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file,
                                       SSL_FILETYPE_PEM) > 0) {
        const char *key_file = (ssl_key_file == NULL || *ssl_key_file == '\0')
                                   ? ssl_cert_file
                                   : ssl_key_file;
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) >
            0)
          if (SSL_CTX_check_private_key(ssl_ctx))
            ng = 0;
      }
      if (ng) {
        free_ssl_ctx();
        goto eend;
      }
    }
    if (ssl_verify_server) {
      const char *file = NULL, *path = NULL;
      if (ssl_ca_file && *ssl_ca_file != '\0')
        file = ssl_ca_file;
      if (ssl_ca_path && *ssl_ca_path != '\0')
        path = ssl_ca_path;
      if ((file || path) &&
          !SSL_CTX_load_verify_locations(ssl_ctx, file, path)) {
        free_ssl_ctx();
        goto eend;
      }
      if (ssl_ca_default)
        SSL_CTX_set_default_verify_paths(ssl_ctx);
    }
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
  }
  handle = SSL_new(ssl_ctx);
  SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
  init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
  SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) \
        */
  if (SSL_connect(handle) > 0) {
    Str *serv_cert = ssl_get_certificate(handle, hostname);
    if (serv_cert) {
      *p_cert = serv_cert->ptr;
      return handle;
    }
    close(sock);
    SSL_free(handle);
    return NULL;
  }
eend:
  close(sock);
  if (handle)
    SSL_free(handle);
  /* FIXME: gettextize? */
  disp_err_message(
      Sprintf("SSL error: %s, a workaround might be: w3m -insecure",
              ERR_error_string(ERR_get_error(), NULL))
          ->ptr,
      FALSE);
  return NULL;
}
void free_ssl_ctx() {
  if (ssl_ctx) {
    SSL_CTX_free(ssl_ctx);
  }
  ssl_ctx = nullptr;
  ssl_accept_this_site(nullptr);
}

void SSL_write_from_file(SSL *ssl, const char *file) {
  int c;
  char buf[1];
  auto fd = fopen(file, "r");
  if (fd != NULL) {
    while ((c = fgetc(fd)) != EOF) {
      buf[0] = c;
      SSL_write(ssl, buf, 1);
    }
    fclose(fd);
  }
}
