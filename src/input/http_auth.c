#include "input/http_auth.h"
#include "input/http_response.h"
#include "buffer/buffer.h"
#include "term/terms.h"
#include "myctype.h"
#include "alloc.h"
#include "etc.h"
#include "core.h"
#include <sys/stat.h>

#define PASSWD_FILE RC_DIR "/passwd"
const char *passwd_file = PASSWD_FILE;

bool disable_secret_security_check = false;

struct auth_param none_auth_param[] = {{nullptr, nullptr}};
struct auth_param basic_auth_param[] = {{"realm", nullptr}, {nullptr, nullptr}};

static char Base64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct auth_pass *passwords = NULL;

static Str next_token(Str arg) {
  Str narg = NULL;
  char *p, *q;
  if (arg == NULL || arg->length == 0)
    return NULL;
  p = arg->ptr;
  q = p;
  SKIP_NON_BLANKS(q);
  if (*q != '\0') {
    *q++ = '\0';
    SKIP_BLANKS(q);
    if (*q != '\0')
      narg = Strnew_charp(q);
  }
  return narg;
}

static void parsePasswd(FILE *fp, int netrc) {
  struct auth_pass ent;
  Str line = NULL;

  memset(&ent, 0, sizeof(struct auth_pass));
  while (1) {
    Str arg = NULL;
    char *p;

    if (line == NULL || line->length == 0)
      line = Strfgets(fp);
    if (line->length == 0)
      break;
    Strchop(line);
    Strremovefirstspaces(line);
    p = line->ptr;
    if (*p == '#' || *p == '\0') {
      line = NULL;
      continue; /* comment or empty line */
    }
    arg = next_token(line);

    if (!strcmp(p, "machine") || !strcmp(p, "host") ||
        (netrc && !strcmp(p, "default"))) {
      add_auth_pass_entry(&ent, netrc, 0);
      memset(&ent, 0, sizeof(struct auth_pass));
      if (netrc)
        ent.port = 21; /* XXX: getservbyname("ftp"); ? */
      if (strcmp(p, "default") != 0) {
        line = next_token(arg);
        ent.host = arg;
      } else {
        line = arg;
      }
    } else if (!netrc && !strcmp(p, "port") && arg) {
      line = next_token(arg);
      ent.port = atoi(arg->ptr);
    } else if (!netrc && !strcmp(p, "proxy")) {
      ent.is_proxy = 1;
      line = arg;
    } else if (!netrc && !strcmp(p, "path")) {
      line = next_token(arg);
      /* ent.file = arg; */
    } else if (!netrc && !strcmp(p, "realm")) {
      /* XXX: rest of line becomes arg for realm */
      line = NULL;
      ent.realm = arg;
    } else if (!strcmp(p, "login")) {
      line = next_token(arg);
      ent.uname = arg;
    } else if (!strcmp(p, "password") || !strcmp(p, "passwd")) {
      line = next_token(arg);
      ent.pwd = arg;
    } else if (netrc && !strcmp(p, "machdef")) {
      while ((line = Strfgets(fp))->length != 0) {
        if (*line->ptr == '\n')
          break;
      }
      line = NULL;
    } else if (netrc && !strcmp(p, "account")) {
      /* ignore */
      line = next_token(arg);
    } else {
      /* ignore rest of line */
      line = NULL;
    }
  }
  add_auth_pass_entry(&ent, netrc, 0);
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG                                                   \
  "SECURITY NOTE: file %s must not be accessible by others"

FILE *openSecretFile(const char *fname) {
  if (fname == NULL)
    return NULL;
  const char *efname = expandPath(fname);
  struct stat st;
  if (stat(efname, &st) < 0)
    return NULL;

  /* check permissions, if group or others readable or writable,
   * refuse it, because it's insecure.
   *
   * XXX: disable_secret_security_check will introduce some
   *    security issues, but on some platform such as Windows
   *    it's not possible (or feasible) to disable group|other
   *    readable and writable.
   *   [w3m-dev 03368][w3m-dev 03369][w3m-dev 03370]
   */
  if (disable_secret_security_check)
    /* do nothing */;
  else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
    term_message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr);
    sleepSeconds(2);
    return NULL;
  }

  return fopen(efname, "r");
}
void loadPasswd(void) {
  FILE *fp;

  passwords = NULL;
  fp = openSecretFile(passwd_file);
  if (fp != NULL) {
    parsePasswd(fp, 0);
    fclose(fp);
  }

  /* for FTP */
  fp = openSecretFile("~/.netrc");
  if (fp != NULL) {
    parsePasswd(fp, 1);
    fclose(fp);
  }
  return;
}

static struct auth_pass *find_auth_pass_entry(char *host, int port, char *realm,
                                              char *uname, int is_proxy) {
  struct auth_pass *ent;
  for (ent = passwords; ent != NULL; ent = ent->next) {
    if (ent->is_proxy == is_proxy && (ent->bad != true) &&
        (!ent->host || !Strcasecmp_charp(ent->host, host)) &&
        (!ent->port || ent->port == port) &&
        (!ent->uname || !uname || !Strcmp_charp(ent->uname, uname)) &&
        (!ent->realm || !realm || !Strcmp_charp(ent->realm, realm)))
      return ent;
  }
  return NULL;
}

int find_auth_user_passwd(struct Url *pu, char *realm, Str *uname, Str *pwd,
                          int is_proxy) {
  struct auth_pass *ent;

  if (pu->user && pu->pass) {
    *uname = Strnew_charp(pu->user);
    *pwd = Strnew_charp(pu->pass);
    return 1;
  }
  ent = find_auth_pass_entry(pu->host, pu->port, realm, pu->user, is_proxy);
  if (ent) {
    *uname = ent->uname;
    *pwd = ent->pwd;
    return 1;
  }
  return 0;
}

/*
 * RFC2617: 1.2 Access Authentication Framework
 *
 * The realm value (case-sensitive), in combination with the canonical root
 * URL (the absoluteURI for the server whose abs_path is empty; see section
 * 5.1.2 of RFC2616 ) of the server being accessed, defines the protection
 * space. These realms allow the protected resources on a server to be
 * partitioned into a set of protection spaces, each with its own
 * authentication scheme and/or authorization database.
 *
 */
void add_auth_pass_entry(const struct auth_pass *ent, int netrc, int override) {
  if ((ent->host || netrc) /* netrc accept default (host == NULL) */
      && (ent->is_proxy || ent->realm || netrc) && ent->uname && ent->pwd) {
    struct auth_pass *newent = New(struct auth_pass);
    memcpy(newent, ent, sizeof(struct auth_pass));
    if (override) {
      newent->next = passwords;
      passwords = newent;
    } else {
      if (passwords == NULL)
        passwords = newent;
      else if (passwords->next == NULL)
        passwords->next = newent;
      else {
        struct auth_pass *ep = passwords;
        for (; ep->next; ep = ep->next)
          ;
        ep->next = newent;
      }
    }
  }
  /* ignore invalid entries */
}
void add_auth_user_passwd(struct Url *pu, char *realm, Str uname, Str pwd,
                          int is_proxy) {
  struct auth_pass ent;
  memset(&ent, 0, sizeof(ent));

  ent.is_proxy = is_proxy;
  ent.host = Strnew_charp(pu->host);
  ent.port = pu->port;
  ent.realm = Strnew_charp(realm);
  ent.uname = uname;
  ent.pwd = pwd;
  add_auth_pass_entry(&ent, 0, 1);
}

/* passwd */
/*
 * machine <host>
 * host <host>
 * port <port>
 * proxy
 * path <file>	; not used
 * realm <realm>
 * login <login>
 * passwd <passwd>
 * password <passwd>
 */

void invalidate_auth_user_passwd(struct Url *pu, char *realm, Str uname,
                                 Str pwd, int is_proxy) {
  struct auth_pass *ent;
  ent = find_auth_pass_entry(pu->host, pu->port, realm, NULL, is_proxy);
  if (ent) {
    ent->bad = true;
  }
  return;
}

Str base64_encode(const unsigned char *src, size_t len) {
  Str dest;
  const unsigned char *in, *endw;
  unsigned long j;
  size_t k;

  k = len;
  if (k % 3)
    k += 3 - (k % 3);

  k = k / 3 * 4;

  if (!len || k + 1 < len)
    return Strnew();

  dest = Strnew_size(k);
  if (dest->area_size <= k) {
    Strfree(dest);
    return Strnew();
  }

  in = src;

  endw = src + len - 2;

  while (in < endw) {
    j = *in++;
    j = j << 8 | *in++;
    j = j << 8 | *in++;

    Strcatc(dest, Base64Table[(j >> 18) & 0x3f]);
    Strcatc(dest, Base64Table[(j >> 12) & 0x3f]);
    Strcatc(dest, Base64Table[(j >> 6) & 0x3f]);
    Strcatc(dest, Base64Table[j & 0x3f]);
  }

  if (src + len - in) {
    j = *in++;
    if (src + len - in) {
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

static Str AuthBasicCred(struct http_auth *ha, Str uname, Str pw,
                         struct Url *pu, struct HttpRequest *hr,
                         struct FormList *request) {
  Str s = Strdup(uname);
  Strcat_char(s, ':');
  Strcat(s, pw);
  return Strnew_m_charp(
      "Basic ", base64_encode((const unsigned char *)s->ptr, s->length)->ptr,
      NULL);
}

/* for RFC2617: HTTP Authentication */
struct http_auth www_auth[] = {
    {1, "Basic ", basic_auth_param, AuthBasicCred},
#ifdef USE_DIGEST_AUTH
    {10, "Digest ", digest_auth_param, AuthDigestCred},
#endif
    {
        0,
        NULL,
        NULL,
        NULL,
    }};

static Str extract_auth_val(char **q) {
  unsigned char *qq = *(unsigned char **)q;
  int quoted = 0;
  Str val = Strnew();

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

enum AUTHCHR {
  AUTHCHR_NUL,
  AUTHCHR_SEP,
  AUTHCHR_TOKEN,
};

static enum AUTHCHR skip_auth_token(char **pp) {
  char *p;
  enum AUTHCHR first = AUTHCHR_NUL;
  int typ;
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

static char *extract_auth_param(char *q, struct auth_param *auth) {
  struct auth_param *ap;
  char *p;

  for (ap = auth; ap->name != NULL; ap++) {
    ap->val = NULL;
  }

  while (*q != '\0') {
    SKIP_BLANKS(q);
    for (ap = auth; ap->name != NULL; ap++) {
      size_t len;

      len = strlen(ap->name);
      if (strncasecmp(q, ap->name, len) == 0 &&
          (IS_SPACE(q[len]) || q[len] == '=')) {
        p = q + len;
        SKIP_BLANKS(p);
        if (*p != '=')
          return q;
        q = p + 1;
        ap->val = extract_auth_val(&q);
        break;
      }
    }
    if (ap->name == NULL) {
      /* skip unknown param */
      p = q;
      if (skip_auth_token(&q) == AUTHCHR_TOKEN && (IS_SPACE(*q) || *q == '=')) {
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

struct http_auth *findAuthentication(struct http_auth *hauth,
                                     struct Buffer *buf, char *auth_field) {
  struct http_auth *ha;
  int len = strlen(auth_field), slen;
  char *p0, *p;

  memset(hauth, 0, sizeof(struct http_auth));
  for (auto i = buf->http_response->document_header->first; i != NULL;
       i = i->next) {
    if (strncasecmp(i->ptr, auth_field, len) == 0) {
      for (p = i->ptr + len; p != NULL && *p != '\0';) {
        SKIP_BLANKS(p);
        p0 = p;
        for (ha = &www_auth[0]; ha->scheme != NULL; ha++) {
          slen = strlen(ha->scheme);
          if (strncasecmp(p, ha->scheme, slen) == 0) {
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
          if (skip_auth_token(&p) == AUTHCHR_TOKEN && IS_SPACE(*p)) {
            SKIP_BLANKS(p);
            p = extract_auth_param(p, none_auth_param);
          } else
            break;
        }
      }
    }
  }
  return hauth->scheme ? hauth : NULL;
}

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

void getAuthCookie(struct http_auth *hauth, char *auth_header,
                   struct TextList *extra_header, struct Url *pu,
                   struct HttpRequest *hr, struct FormList *request, Str *uname,
                   Str *pwd) {
  Str ss = NULL;
  Str tmp;
  struct TextListItem *i;
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
    term_message("Wrong username or password");
    sleepSeconds(1);
    /* delete Authenticate: header from extra_header */
    delText(extra_header, i);
    invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
  }
  *uname = NULL;
  *pwd = NULL;

  if (!a_found &&
      find_auth_user_passwd(pu, realm, (Str *)uname, (Str *)pwd, proxy)) {
    /* found username & password in passwd file */;
  } else {
    if (QuietMessage)
      return;
    /* input username and password */
    sleepSeconds(2);

#ifdef _WIN32
    return;
#else
    if (!term_inputAuth(realm, proxy, uname, pwd)) {
      return;
    }
#endif
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
