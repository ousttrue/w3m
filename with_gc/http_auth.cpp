#include "http_auth.h"
#include "quote.h"
#include "cmp.h"
#include "auth_pass.h"
#include "http_response.h"
#include "http_request.h"
#include "Str.h"
#include "myctype.h"
#include "etc.h"
#include "form.h"
#include "readallbytes.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <string>
#include <sstream>

struct auth_param {
  std::string name;
  std::string val;
};

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

static std::string extract_auth_val(const char **q) {
  unsigned char *qq = *(unsigned char **)q;
  int quoted = 0;
  std::stringstream val;

  SKIP_BLANKS(qq);
  if (*qq == '"') {
    quoted = true;
    val << *qq++;
  }
  while (*qq != '\0') {
    if (quoted && *qq == '"') {
      val << *qq++;
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
    } else if (quoted && *qq == '\\') {
      val << *qq++;
    }
    val << *qq++;
  }
end_token:
  *q = (char *)qq;
  return val.str();
}

static const char *extract_auth_param(const char *q, struct auth_param *auth) {
  // clear
  for (auto ap = auth; ap->name.size(); ap++) {
    ap->val = nullptr;
  }

  while (*q != '\0') {
    SKIP_BLANKS(q);

    struct auth_param *ap;
    for (ap = auth; ap->name.size(); ap++) {
      size_t len = ap->name.size();
      if (strncasecmp(q, ap->name.c_str(), len) == 0 &&
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
    if (ap->name.empty()) {
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

std::string http_auth::get_auth_param(const char *name) {
  for (auto ap = this->param; ap->name.size(); ap++) {
    if (strcasecmp(name, ap->name) == 0) {
      return ap->val;
    }
  }
  return {};
}

static auto Base64Table =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const char *src, int len) {

  auto k = len;
  if (k % 3)
    k += 3 - (k % 3);
  k = k / 3 * 4;

  if (!len || k + 1 < len)
    return {};

  auto s = (unsigned char *)src;
  auto in = s;

  auto endw = s + len - 2;

  std::stringstream dest;
  while (in < endw) {
    uint32_t j = *in++;
    j = j << 8 | *in++;
    j = j << 8 | *in++;

    dest << Base64Table[(j >> 18) & 0x3f];
    dest << Base64Table[(j >> 12) & 0x3f];
    dest << Base64Table[(j >> 6) & 0x3f];
    dest << Base64Table[j & 0x3f];
  }

  if (s + len - in) {
    uint32_t j = *in++;
    if (s + len - in) {
      j = j << 8 | *in++;
      j = j << 8;
      dest << Base64Table[(j >> 18) & 0x3f];
      dest << Base64Table[(j >> 12) & 0x3f];
      dest << Base64Table[(j >> 6) & 0x3f];
    } else {
      j = j << 8;
      j = j << 8;
      dest << Base64Table[(j >> 18) & 0x3f];
      dest << Base64Table[(j >> 12) & 0x3f];
      dest << '=';
    }
    dest << '=';
  }
  return dest.str();
}

static std::string AuthBasicCred(struct http_auth *ha, const std::string &uname,
                                 const std::string &pw, const Url &pu,
                                 HttpRequest *hr,
                                 const std::shared_ptr<Form> &) {
  std::string s = uname;
  s += ':';
  s += pw;
  std::stringstream ss;
  ss << "Basic " << base64_encode(s.c_str(), s.size());
  return ss.str();
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

enum {
  QOP_NONE,
  QOP_AUTH,
  QOP_AUTH_INT,
};

static std::string digest_hex(unsigned char *p) {
  auto h = "0123456789abcdef";
  std::stringstream tmp;
  int i;
  for (i = 0; i < MD5_DIGEST_LENGTH; i++, p++) {
    tmp << h[(*p >> 4) & 0x0f];
    tmp << h[*p & 0x0f];
  }
  return tmp.str();
}

std::string AuthDigestCred(struct http_auth *ha, const std::string &uname,
                           const std::string &pw, const Url &pu,
                           HttpRequest *hr,
                           const std::shared_ptr<Form> &request) {
  auto algorithm = qstr_unquote(ha->get_auth_param("algorithm"));
  auto nonce = qstr_unquote(ha->get_auth_param("nonce"));
  // Str *cnonce /* = qstr_unquote(get_auth_param(ha->param, "cnonce")) */;
  /* cnonce is what client should generate. */
  auto qop = qstr_unquote(ha->get_auth_param("qop"));

  static union {
    int r[4];
    unsigned char s[sizeof(int) * 4];
  } cnonce_seed;

  cnonce_seed.r[0] = rand();
  cnonce_seed.r[1] = rand();
  cnonce_seed.r[2] = rand();
  unsigned char md5[MD5_DIGEST_LENGTH + 1];
  MD5(cnonce_seed.s, sizeof(cnonce_seed.s), md5);
  auto cnonce = digest_hex(md5);
  cnonce_seed.r[3]++;

  int qop_i = QOP_NONE;
  if (qop.size()) {
    size_t i;

    auto p = qop.c_str();
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
  std::stringstream tmp0;
  tmp0 << uname << ":" << qstr_unquote(ha->get_auth_param("realm")) << ":"
       << pw;
  auto str0 = tmp0.str();
  MD5((unsigned char *)str0.c_str(), str0.size(), md5);
  auto a1buf = digest_hex(md5);

  if (algorithm.size()) {
    if (strcasecmp(algorithm, "MD5-sess") == 0) {
      /* A1 = H(unq(username-value) ":" unq(realm-value) ":" passwd)
       *      ":" unq(nonce-value) ":" unq(cnonce-value)
       */
      if (nonce.empty()) {
        return nullptr;
      }
      std::stringstream tmp1;
      tmp1 << a1buf << ":" << qstr_unquote(nonce) << ":"
           << qstr_unquote(cnonce);
      auto str1 = tmp1.str();
      MD5((unsigned char *)str1.c_str(), str1.size(), md5);
      a1buf = digest_hex(md5);
    } else if (strcasecmp(algorithm, "MD5") == 0) {
      /* ok default */
      ;
    } else {
      /* unknown algorithm */
      return nullptr;
    }
  }

  /* A2 = Method ":" digest-uri-value */
  auto uri = hr->getRequestURI();
  std::stringstream tmp2;
  tmp2 << to_str(hr->method) << ":" << uri;
  if (qop_i == QOP_AUTH_INT) {
    /*  A2 = Method ":" digest-uri-value ":" H(entity-body) */
    if (request && request->body.size()) {
      if (request->method == FORM_METHOD_POST &&
          request->enctype == FORM_ENCTYPE_MULTIPART) {
        auto ebody = ReadAllBytes(request->body);
        if (ebody.size()) {
          MD5((unsigned char *)ebody.data(), ebody.size(), md5);
        } else {
          MD5((unsigned char *)"", 0, md5);
        }
      } else {
        MD5((unsigned char *)request->body.c_str(), request->length, md5);
      }
    } else {
      MD5((unsigned char *)"", 0, md5);
    }
    tmp2 << ':';
    tmp2 << digest_hex(md5);
  }
  auto str2 = tmp2.str();
  MD5((unsigned char *)str2.c_str(), str2.size(), md5);

  auto a2buf = digest_hex(md5);

  auto nc = "00000001";
  std::string rd;
  if (qop_i >= QOP_AUTH) {
    /* request-digest  = <"> < KD ( H(A1),     unq(nonce-value)
     *                      ":" nc-value
     *                      ":" unq(cnonce-value)
     *                      ":" unq(qop-value)
     *                      ":" H(A2)
     *                      ) <">
     */
    if (nonce.empty())
      return nullptr;
    std::stringstream tmp3;
    tmp3 << a1buf << ":" << qstr_unquote(nonce) << ":" << nc << ":"
         << qstr_unquote(cnonce) << ":"
         << (qop_i == QOP_AUTH ? "auth" : "auth-int") << ":" << a2buf;
    auto str3 = tmp3.str();
    MD5((unsigned char *)str3.c_str(), str3.size(), md5);
    rd = digest_hex(md5);
  } else {
    /* compatibility with RFC 2069
     * request_digest = KD(H(A1),  unq(nonce), H(A2))
     */
    std::stringstream tmp3;
    tmp3 << a1buf << ":" << qstr_unquote(ha->get_auth_param("nonce")) << ":"
         << a2buf;
    auto str3 = tmp3.str();
    MD5((unsigned char *)str3.c_str(), str3.size(), md5);
    rd = digest_hex(md5);
  }

  /*
   * digest-response  = 1#( username | realm | nonce | digest-uri
   *                          | response | [ algorithm ] | [cnonce] |
   *                          [opaque] | [message-qop] |
   *                          [nonce-count]  | [auth-param] )
   */

  std::stringstream tmp4;
  tmp4 << "Digest username=\"" << uname << "\"";

  std::string s;
  if ((s = ha->get_auth_param("realm")).size())
    tmp4 << ", realm=" << s;
  if ((s = ha->get_auth_param("nonce")).size())
    tmp4 << ", nonce=" << s;
  tmp4 << ", uri=\"" << uri << "\"";
  tmp4 << ", response=\"" << rd << "\"";

  if (algorithm.size() && (s = ha->get_auth_param("algorithm")).size())
    tmp4 << ", algorithm=" << s;

  if (cnonce.size())
    tmp4 << ", cnonce=\"" << cnonce << "\"";

  if ((s = ha->get_auth_param("opaque")).size())
    tmp4 << ", opaque=" << s;

  if (qop_i >= QOP_AUTH) {
    tmp4 << ", qop=" << (qop_i == QOP_AUTH ? "auth" : "auth-int");
    /* XXX how to count? */
    /* Since nonce is unique up to each *-Authenticate and w3m does not re-use
     *-Authenticate: headers, nonce-count should be always "00000001". */
    tmp4 << ", nc=" << nc;
  }

  return tmp4.str();
}

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

std::optional<http_auth> findAuthentication(const HttpResponse &res,
                                            const char *auth_field) {
  int len = strlen(auth_field);
  http_auth hauth = {0};
  for (auto &i : res.document_header) {
    if (strncasecmp(i.c_str(), auth_field, len) == 0) {
      for (auto p = i.c_str() + len; p != nullptr && *p != '\0';) {
        SKIP_BLANKS(p);
        auto p0 = p;
        for (auto ha = &www_auth[0]; ha->schema.size(); ha++) {
          auto slen = ha->schema.size();
          if (strncasecmp(p, ha->schema.c_str(), slen) == 0) {
            p += slen;
            SKIP_BLANKS(p);
            if (hauth.pri < ha->pri) {
              hauth = *ha;
              p = extract_auth_param(p, hauth.param);
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
  if (hauth.schema.empty()) {
    return {};
  }
  return hauth;
}

std::tuple<std::string, std::string>
getAuthCookie(struct http_auth *hauth, const char *auth_header, const Url &pu,
              HttpRequest *hr, const std::shared_ptr<Form> &request) {
  std::string realm;
  if (hauth) {
    realm = qstr_unquote(hauth->get_auth_param("realm"));
  }
  if (realm.empty()) {
    return {};
  }

  auto a_found = false;
  int auth_header_len = strlen(auth_header);
  auto i = hr->extra_headers.begin();
  for (; i != hr->extra_headers.end(); ++i) {
    if (!strncasecmp(i->c_str(), auth_header, auth_header_len)) {
      a_found = true;
      break;
    }
  }

  auto proxy =
      !strncasecmp("Proxy-Authorization:", auth_header, auth_header_len);
  if (a_found) {
    /* This means that *-Authenticate: header is received after
     * Authorization: header is sent to the server.
     */
    // App::instance().message("Wrong username or password");
    // sleep(1);
    /* delete Authenticate: header from extra_header */
    hr->extra_headers.erase(i);
    invalidate_auth_user_passwd(pu, realm, proxy);
  }

  std::string uname;
  std::string pwd;

  if (!a_found) {
    auto [_uname, _pwd] = find_auth_user_passwd(pu, realm, proxy);
    if (_uname.size()) {
      uname = _uname;
      pwd = _pwd;
    }
    /* found username & password in passwd file */;
  }

  if (uname.empty() || pwd.empty()) {
    if (IsForkChild)
      return {};
    /* input username and password */
    // sleep(2);
    if (true /*fmInitialized*/) {
      std::string pp = {};
      // term_raw();
      // if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr, NULL)) ==
      //     NULL)
      //   return;
      uname = pp;
      // if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
      //                     IN_PASSWORD)) == NULL)
      if (pp.empty()) {
        return {};
      }
      pwd = pp;
      // term_cbreak();
    } else {
      /*
       * If post file is specified as '-', stdin is closed at this
       * point.
       * In this case, w3m cannot read username from stdin.
       * So exit with error message.
       * (This is same behavior as lwp-request.)
       */
      if (feof(stdin) || ferror(stdin)) {
        fprintf(stderr, "w3m: Authorization required for %s\n", realm.c_str());
        exit(1);
      }

      printf(proxy ? "Proxy Username for %s: " : "Username for %s: ",
             realm.c_str());
      fflush(stdout);
      uname = Strfgets(stdin)->ptr;
      // Strchop(*uname);
#ifdef HAVE_GETPASSPHRASE
      pwd = getpassphrase(proxy ? "Proxy Password: " : "Password: ");
#else
      assert(false);
      // *pwd = Strnew_charp(getpass(proxy ? "Proxy Password: " : "Password:
      // "));
#endif
    }
  }
  auto ss = hauth->cred(hauth, uname, pwd, pu, hr, request);
  if (ss.size()) {
    auto tmp = Strnew_charp(auth_header);
    Strcat_m_charp(tmp, " ", ss.c_str(), "\r\n", NULL);
    hr->extra_headers.push_back(tmp->ptr);
    return {
        uname,
        pwd,
    };
  } else {
    return {};
  }
}
