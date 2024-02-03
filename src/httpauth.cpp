#include "httpauth.h"
#include "authpass.h"
#include "httprequest.h"
#include "auth_digest.h"
#include "form.h"
#include "screen.h"
#include "w3m.h"
#include "message.h"
#include "Str.h"
#include "myctype.h"
#include "buffer.h"
#include "textlist.h"
#include "terms.h"
#include "etc.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

enum AuthChrType {
  AUTHCHR_NUL,
  AUTHCHR_SEP,
  AUTHCHR_TOKEN,
};

static int skip_auth_token(const char **pp) {
  const char *p;
  AuthChrType first = AUTHCHR_NUL;
  AuthChrType typ = {};
  for (p = *pp;; ++p) {
    switch (*p) {
    case '\0':
      goto endoftoken;
    default:
      if ((unsigned char)*p > 037) {
        typ = AUTHCHR_TOKEN;
        break;
      }
      /* thru */
    case '\177':
    case '[':
    case ']':
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '?':
    case '=':
    case ' ':
    case '\t':
    case ',':
      typ = AUTHCHR_SEP;
      break;
    }

    if (!first)
      first = typ;
    else if (first != typ)
      break;
  }
endoftoken:
  *pp = p;
  return first;
}

static Str *extract_auth_val(const char **q) {
  unsigned char *qq = *(unsigned char **)q;
  int quoted = 0;
  Str *val = Strnew();

  SKIP_BLANKS(qq);
  if (*qq == '"') {
    quoted = true;
    Strcat_char(val, *qq++);
  }
  while (*qq != '\0') {
    if (quoted && *qq == '"') {
      Strcat_char(val, *qq++);
      break;
    }
    if (!quoted) {
      switch (*qq) {
      case '[':
      case ']':
      case '(':
      case ')':
      case '<':
      case '>':
      case '@':
      case ';':
      case ':':
      case '\\':
      case '"':
      case '/':
      case '?':
      case '=':
      case ' ':
      case '\t':
        qq++;
      case ',':
        goto end_token;
      default:
        if (*qq <= 037 || *qq == 0177) {
          qq++;
          goto end_token;
        }
      }
    } else if (quoted && *qq == '\\')
      Strcat_char(val, *qq++);
    Strcat_char(val, *qq++);
  }
end_token:
  *q = (char *)qq;
  return val;
}

Str *qstr_unquote(Str *s) {
  if (s == nullptr)
    return nullptr;
  auto p = s->ptr;
  if (*p == '"') {
    Str *tmp = Strnew();
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

static const char *extract_auth_param(const char *q, struct auth_param *auth) {
  // clear
  for (auto ap = auth; ap->name; ap++) {
    ap->val = nullptr;
  }

  while (*q != '\0') {
    SKIP_BLANKS(q);

    struct auth_param *ap;
    for (ap = auth; ap->name != nullptr; ap++) {
      size_t len;

      len = strlen(ap->name);
      if (strncasecmp(q, ap->name, len) == 0 &&
          (IS_SPACE(q[len]) || q[len] == '=')) {
        auto p = q + len;
        SKIP_BLANKS(p);
        if (*p != '=')
          return q;
        q = p + 1;
        ap->val = extract_auth_val(&q);
        break;
      }
    }
    if (ap->name == nullptr) {
      /* skip unknown param */
      int token_type;
      auto p = q;
      if ((token_type = skip_auth_token(&q)) == AUTHCHR_TOKEN &&
          (IS_SPACE(*q) || *q == '=')) {
        SKIP_BLANKS(q);
        if (*q != '=')
          return p;
        q++;
        extract_auth_val(&q);
      } else
        return p;
    }
    if (*q != '\0') {
      SKIP_BLANKS(q);
      if (*q == ',')
        q++;
      else
        break;
    }
  }
  return q;
}

Str *get_auth_param(auth_param *auth, const char *name) {
  for (auto ap = auth; ap->name != nullptr; ap++) {
    if (strcasecmp(name, ap->name) == 0)
      return ap->val;
  }
  return nullptr;
}

static auto Base64Table =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Str *base64_encode(const char *src, int len) {

  auto k = len;
  if (k % 3)
    k += 3 - (k % 3);
  k = k / 3 * 4;

  if (!len || k + 1 < len)
    return Strnew();

  auto dest = Strnew_size(k);
  if (dest->area_size <= k) {
    Strfree(dest);
    return Strnew();
  }

  auto s = (unsigned char *)src;
  auto in = s;

  auto endw = s + len - 2;

  while (in < endw) {
    auto j = *in++;
    j = j << 8 | *in++;
    j = j << 8 | *in++;

    Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
    Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
    Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
    Strcatc(dest, Base64Table[j & 0x3f]);
  }

  if (s + len - in) {
    auto j = *in++;
    if (s + len - in) {
      j = j << 8 | *in++;
      j = j << 8;
      Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
      Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
      Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
    } else {
      j = j << 8;
      j = j << 8;
      Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
      Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
      Strcatc(dest, '=');
    }
    Strcatc(dest, '=');
  }
  Strnulterm(dest);
  return dest;
}

static Str *AuthBasicCred(struct http_auth *ha, Str *uname, Str *pw, Url *pu,
                          HRequest *hr, FormList *request) {
  Str *s = uname->Strdup();
  Strcat_char(s, ':');
  Strcat(s, pw);
  return Strnew_m_charp("Basic ", base64_encode(s->ptr, s->length)->ptr,
                        nullptr);
}

struct auth_param none_auth_param[] = {{nullptr, nullptr}};

struct auth_param basic_auth_param[] = {{"realm", nullptr}, {nullptr, nullptr}};

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
    {"realm", nullptr},  {"domain", nullptr}, {"nonce", nullptr},
    {"opaque", nullptr}, {"stale", nullptr},  {"algorithm", nullptr},
    {"qop", nullptr},    {nullptr, nullptr}};

/* for RFC2617: HTTP Authentication */
struct http_auth www_auth[] = {
    {1, "Basic ", basic_auth_param, AuthBasicCred},
    {10, "Digest ", digest_auth_param, AuthDigestCred},
    {
        0,
        nullptr,
        nullptr,
        nullptr,
    }};

http_auth *findAuthentication(http_auth *hauth, Buffer *buf,
                              const char *auth_field) {
  struct http_auth *ha;
  int len = strlen(auth_field), slen;
  TextListItem *i;
  const char *p0, *p;

  bzero(hauth, sizeof(struct http_auth));
  for (i = buf->document_header->first; i != nullptr; i = i->next) {
    if (strncasecmp(i->ptr, auth_field, len) == 0) {
      for (p = i->ptr + len; p != nullptr && *p != '\0';) {
        SKIP_BLANKS(p);
        p0 = p;
        for (ha = &www_auth[0]; ha->schema != nullptr; ha++) {
          slen = strlen(ha->schema);
          if (strncasecmp(p, ha->schema, slen) == 0) {
            p += slen;
            SKIP_BLANKS(p);
            if (hauth->pri < ha->pri) {
              *hauth = *ha;
              p = extract_auth_param(p, hauth->param);
              break;
            } else {
              /* weak auth */
              p = extract_auth_param(p, none_auth_param);
            }
          }
        }
        if (p0 == p) {
          /* all unknown auth failed */
          int token_type;
          if ((token_type = skip_auth_token(&p)) == AUTHCHR_TOKEN &&
              IS_SPACE(*p)) {
            SKIP_BLANKS(p);
            p = extract_auth_param(p, none_auth_param);
          } else
            break;
        }
      }
    }
  }
  return hauth->schema ? hauth : nullptr;
}

void getAuthCookie(struct http_auth *hauth, const char *auth_header,
                   TextList *extra_header, Url *pu, HRequest *hr,
                   FormList *request, Str **uname, Str **pwd) {
  Str *ss = NULL;
  Str *tmp;
  TextListItem *i;
  int a_found;
  int auth_header_len = strlen(auth_header);
  char *realm = NULL;
  int proxy;

  if (hauth)
    realm = qstr_unquote(get_auth_param(hauth->param, "realm"))->ptr;

  if (!realm)
    return;

  a_found = false;
  for (i = extra_header->first; i != NULL; i = i->next) {
    if (!strncasecmp(i->ptr, auth_header, auth_header_len)) {
      a_found = true;
      break;
    }
  }
  proxy = !strncasecmp("Proxy-Authorization:", auth_header, auth_header_len);
  if (a_found) {
    /* This means that *-Authenticate: header is received after
     * Authorization: header is sent to the server.
     */
    if (fmInitialized) {
      message("Wrong username or password", 0, 0);
      refresh(term_io());
    } else
      fprintf(stderr, "Wrong username or password\n");
    sleep(1);
    /* delete Authenticate: header from extra_header */
    delText(extra_header, i);
    invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
  }
  *uname = NULL;
  *pwd = NULL;

  if (!a_found &&
      find_auth_user_passwd(pu, realm, (Str **)uname, (Str **)pwd, proxy)) {
    /* found username & password in passwd file */;
  } else {
    if (IsForkChild)
      return;
    /* input username and password */
    sleep(2);
    if (fmInitialized) {
      const char *pp;
      term_raw();
      // if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr, NULL)) ==
      //     NULL)
      //   return;
      *uname = Strnew_charp(pp);
      // if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
      //                     IN_PASSWORD)) == NULL)
      {
        *uname = NULL;
        return;
      }
      *pwd = Strnew_charp(pp);
      term_cbreak();
    } else {
      /*
       * If post file is specified as '-', stdin is closed at this
       * point.
       * In this case, w3m cannot read username from stdin.
       * So exit with error message.
       * (This is same behavior as lwp-request.)
       */
      if (feof(stdin) || ferror(stdin)) {
        fprintf(stderr, "w3m: Authorization required for %s\n", realm);
        exit(1);
      }

      printf(proxy ? "Proxy Username for %s: " : "Username for %s: ", realm);
      fflush(stdout);
      *uname = Strfgets(stdin);
      Strchop(*uname);
#ifdef HAVE_GETPASSPHRASE
      *pwd = Strnew_charp(
          (char *)getpassphrase(proxy ? "Proxy Password: " : "Password: "));
#else
      *pwd = Strnew_charp(
          (char *)getpass(proxy ? "Proxy Password: " : "Password: "));
#endif
    }
  }
  ss = hauth->cred(hauth, *uname, *pwd, pu, hr, request);
  if (ss) {
    tmp = Strnew_charp(auth_header);
    Strcat_m_charp(tmp, " ", ss->ptr, "\r\n", NULL);
    pushText(extra_header, tmp->ptr);
  } else {
    *uname = NULL;
    *pwd = NULL;
  }
  return;
}
