#include "digest_auth.h"
#include <openssl/md5.h>

/* RFC2617: 3.2.1 The WWW-Authenticate Response Header
 * challenge        =  "Digest" digest-challenge
 *
 * digest-challenge  = 1#( realm | [ domain ] | nonce |
 *                       [ opaque ] |[ stale ] | [ algorithm ] |
 *                        [ qop-options ] | [auth-param] )
 *
 * domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 * URI               = absoluteURI | abs_path
 * nonce             = "nonce" "=" nonce-value
 * nonce-value       = quoted-string
 * opaque            = "opaque" "=" quoted-string
 * stale             = "stale" "=" ( "true" | "false" )
 * algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" |
 *                        token )
 * qop-options       = "qop" "=" <"> 1#qop-value <">
 * qop-value         = "auth" | "auth-int" | token
 */
struct auth_param digest_auth_param[] = {
    {"realm", NULL}, {"domain", NULL},    {"nonce", NULL}, {"opaque", NULL},
    {"stale", NULL}, {"algorithm", NULL}, {"qop", NULL},   {NULL, NULL}};

/* RFC2617: 3.2.2 The Authorization Request Header
 *
 * credentials      = "Digest" digest-response
 * digest-response  = 1#( username | realm | nonce | digest-uri
 *                    | response | [ algorithm ] | [cnonce] |
 *                     [opaque] | [message-qop] |
 *                         [nonce-count]  | [auth-param] )
 *
 * username         = "username" "=" username-value
 * username-value   = quoted-string
 * digest-uri       = "uri" "=" digest-uri-value
 * digest-uri-value = request-uri   ; As specified by HTTP/1.1
 * message-qop      = "qop" "=" qop-value
 * cnonce           = "cnonce" "=" cnonce-value
 * cnonce-value     = nonce-value
 * nonce-count      = "nc" "=" nc-value
 * nc-value         = 8LHEX
 * response         = "response" "=" request-digest
 * request-digest = <"> 32LHEX <">
 * LHEX             =  "0" | "1" | "2" | "3" |
 *                     "4" | "5" | "6" | "7" |
 *                     "8" | "9" | "a" | "b" |
 *                     "c" | "d" | "e" | "f"
 */

static Str digest_hex(unsigned char *p) {
  char *h = "0123456789abcdef";
  Str tmp = Strnew_size(MD5_DIGEST_LENGTH * 2 + 1);
  int i;
  for (i = 0; i < MD5_DIGEST_LENGTH; i++, p++) {
    Strcat_char(tmp, h[(*p >> 4) & 0x0f]);
    Strcat_char(tmp, h[*p & 0x0f]);
  }
  return tmp;
}

enum {
  QOP_NONE,
  QOP_AUTH,
  QOP_AUTH_INT,
};

Str qstr_unquote(Str s) {
  char *p;

  if (s == NULL)
    return NULL;
  p = s->ptr;
  if (*p == '"') {
    Str tmp = Strnew();
    for (p++; *p != '\0'; p++) {
      if (*p == '\\')
        p++;
      Strcat_char(tmp, *p);
    }
    if (Strlastchar(tmp) == '"')
      Strshrink(tmp, 1);
    return tmp;
  } else
    return s;
}

Str get_auth_param(struct auth_param *auth, char *name) {
  struct auth_param *ap;
  for (ap = auth; ap->name != NULL; ap++) {
    if (strcasecmp(name, ap->name) == 0)
      return ap->val;
  }
  return NULL;
}

Str AuthDigestCred(struct http_auth *ha, Str uname, Str pw, struct Url *pu,
                   HRequest *hr, FormList *request) {
  Str tmp, a1buf, a2buf, rd, s;
  unsigned char md5[MD5_DIGEST_LENGTH + 1];
  Str uri = HTTPrequestURI(pu, hr);
  char nc[] = "00000001";
  FILE *fp;

  Str algorithm = qstr_unquote(get_auth_param(ha->param, "algorithm"));
  Str nonce = qstr_unquote(get_auth_param(ha->param, "nonce"));
  Str cnonce /* = qstr_unquote(get_auth_param(ha->param, "cnonce")) */;
  /* cnonce is what client should generate. */
  Str qop = qstr_unquote(get_auth_param(ha->param, "qop"));

  static union {
    int r[4];
    unsigned char s[sizeof(int) * 4];
  } cnonce_seed;
  int qop_i = QOP_NONE;

  cnonce_seed.r[0] = rand();
  cnonce_seed.r[1] = rand();
  cnonce_seed.r[2] = rand();
  MD5(cnonce_seed.s, sizeof(cnonce_seed.s), md5);
  cnonce = digest_hex(md5);
  cnonce_seed.r[3]++;

  if (qop) {
    char *p;
    size_t i;

    p = qop->ptr;
    SKIP_BLANKS(p);

    for (;;) {
      if ((i = strcspn(p, " \t,")) > 0) {
        if (i == sizeof("auth-int") - sizeof("") &&
            !strncasecmp(p, "auth-int", i)) {
          if (qop_i < QOP_AUTH_INT)
            qop_i = QOP_AUTH_INT;
        } else if (i == sizeof("auth") - sizeof("") &&
                   !strncasecmp(p, "auth", i)) {
          if (qop_i < QOP_AUTH)
            qop_i = QOP_AUTH;
        }
      }

      if (p[i]) {
        p += i + 1;
        SKIP_BLANKS(p);
      } else
        break;
    }
  }

  /* A1 = unq(username-value) ":" unq(realm-value) ":" passwd */
  tmp = Strnew_m_charp(uname->ptr, ":",
                       qstr_unquote(get_auth_param(ha->param, "realm"))->ptr,
                       ":", pw->ptr, NULL);
  MD5(tmp->ptr, strlen(tmp->ptr), md5);
  a1buf = digest_hex(md5);

  if (algorithm) {
    if (strcasecmp(algorithm->ptr, "MD5-sess") == 0) {
      /* A1 = H(unq(username-value) ":" unq(realm-value) ":" passwd)
       *      ":" unq(nonce-value) ":" unq(cnonce-value)
       */
      if (nonce == NULL)
        return NULL;
      tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr, ":",
                           qstr_unquote(cnonce)->ptr, NULL);
      MD5(tmp->ptr, strlen(tmp->ptr), md5);
      a1buf = digest_hex(md5);
    } else if (strcasecmp(algorithm->ptr, "MD5") == 0)
      /* ok default */
      ;
    else
      /* unknown algorithm */
      return NULL;
  }

  /* A2 = Method ":" digest-uri-value */
  tmp = Strnew_m_charp(HTTPrequestMethod(hr)->ptr, ":", uri->ptr, NULL);
  if (qop_i == QOP_AUTH_INT) {
    /*  A2 = Method ":" digest-uri-value ":" H(entity-body) */
    if (request && request->body) {
      if (request->method == FORM_METHOD_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        fp = fopen(request->body, "r");
        if (fp != NULL) {
          Str ebody;
          ebody = Strfgetall(fp);
          fclose(fp);
          MD5(ebody->ptr, strlen(ebody->ptr), md5);
        } else {
          MD5("", 0, md5);
        }
      } else {
        MD5(request->body, request->length, md5);
      }
    } else {
      MD5("", 0, md5);
    }
    Strcat_char(tmp, ':');
    Strcat(tmp, digest_hex(md5));
  }
  MD5(tmp->ptr, strlen(tmp->ptr), md5);
  a2buf = digest_hex(md5);

  if (qop_i >= QOP_AUTH) {
    /* request-digest  = <"> < KD ( H(A1),     unq(nonce-value)
     *                      ":" nc-value
     *                      ":" unq(cnonce-value)
     *                      ":" unq(qop-value)
     *                      ":" H(A2)
     *                      ) <">
     */
    if (nonce == NULL)
      return NULL;
    tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr, ":", nc,
                         ":", qstr_unquote(cnonce)->ptr, ":",
                         qop_i == QOP_AUTH ? "auth" : "auth-int", ":",
                         a2buf->ptr, NULL);
    MD5(tmp->ptr, strlen(tmp->ptr), md5);
    rd = digest_hex(md5);
  } else {
    /* compatibility with RFC 2069
     * request_digest = KD(H(A1),  unq(nonce), H(A2))
     */
    tmp = Strnew_m_charp(a1buf->ptr, ":",
                         qstr_unquote(get_auth_param(ha->param, "nonce"))->ptr,
                         ":", a2buf->ptr, NULL);
    MD5(tmp->ptr, strlen(tmp->ptr), md5);
    rd = digest_hex(md5);
  }

  /*
   * digest-response  = 1#( username | realm | nonce | digest-uri
   *                          | response | [ algorithm ] | [cnonce] |
   *                          [opaque] | [message-qop] |
   *                          [nonce-count]  | [auth-param] )
   */

  tmp = Strnew_m_charp("Digest username=\"", uname->ptr, "\"", NULL);
  Strcat_m_charp(tmp, ", realm=", get_auth_param(ha->param, "realm")->ptr,
                 NULL);
  Strcat_m_charp(tmp, ", nonce=", get_auth_param(ha->param, "nonce")->ptr,
                 NULL);
  Strcat_m_charp(tmp, ", uri=\"", uri->ptr, "\"", NULL);
  Strcat_m_charp(tmp, ", response=\"", rd->ptr, "\"", NULL);

  if (algorithm)
    Strcat_m_charp(
        tmp, ", algorithm=", get_auth_param(ha->param, "algorithm")->ptr, NULL);

  if (cnonce)
    Strcat_m_charp(tmp, ", cnonce=\"", cnonce->ptr, "\"", NULL);

  if ((s = get_auth_param(ha->param, "opaque")) != NULL)
    Strcat_m_charp(tmp, ", opaque=", s->ptr, NULL);

  if (qop_i >= QOP_AUTH) {
    Strcat_m_charp(tmp, ", qop=", qop_i == QOP_AUTH ? "auth" : "auth-int",
                   NULL);
    /* XXX how to count? */
    /* Since nonce is unique up to each *-Authenticate and w3m does not re-use
     *-Authenticate: headers, nonce-count should be always "00000001". */
    Strcat_m_charp(tmp, ", nc=", nc, NULL);
  }

  return tmp;
}
