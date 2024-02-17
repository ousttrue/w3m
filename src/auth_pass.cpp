#include "auth_pass.h"
#include "app.h"
#include "quote.h"
#include "screen.h"
#include "w3m.h"
#include "etc.h"
#include "alloc.h"
#include "url.h"
#include "Str.h"
#include "myctype.h"
#include "terms.h"
#include "display.h"
#include <string.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#endif

#define PASSWD_FILE RC_DIR "/passwd"

#ifndef RC_DIR
#define RC_DIR "~/.w3m"
#endif
#define PASSWD_FILE RC_DIR "/passwd"
const char *passwd_file = PASSWD_FILE;

bool disable_secret_security_check = false;

/*
 * RFC2617: 1.2 Access Authentication Framework
 *
 * The realm value (case-sensitive), in combination with the canonical root
 * URL (the absoluteURI for the server whose abs_path is empty; see section
 * 5.1.2 of RFC2616 ) of the server being accessed, defines the protection
 * space. These realms allow the protected resources on a server to be
 * partitioned into a set of protection spaces, each with its own
 * authentication schema and/or authorization database.
 *
 */

auth_pass *passwords = {};

static void add_auth_pass_entry(const auth_pass &ent, int netrc, int override) {
  if ((ent.host || netrc) /* netrc accept default (host == NULL) */
      && (ent.is_proxy || ent.realm || netrc) && ent.uname && ent.pwd) {
    auto newent = (struct auth_pass *)New(auth_pass);
    memcpy(newent, &ent, sizeof(auth_pass));
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

void add_auth_user_passwd(const Url &pu, const char *realm, Str *uname,
                          Str *pwd, bool is_proxy) {
  auth_pass ent{
      .is_proxy = is_proxy,
      .host = Strnew(pu.host),
      .port = pu.port,
      .realm = Strnew_charp(realm),
      .uname = uname,
      .pwd = pwd,
  };
  add_auth_pass_entry(ent, 0, 1);
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
static Str *next_token(Str *arg) {
  Str *narg = NULL;
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
  auth_pass ent = {0};
  Str *line = NULL;

  while (1) {
    Str *arg = NULL;
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
      add_auth_pass_entry(ent, netrc, 0);
      ent = {0};
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
  add_auth_pass_entry(ent, netrc, 0);
}

static struct auth_pass *find_auth_pass_entry(const char *host, int port,
                                              const char *realm,
                                              std::string_view uname,
                                              bool is_proxy) {
  struct auth_pass *ent;
  for (ent = passwords; ent != NULL; ent = ent->next) {
    if (ent->is_proxy == is_proxy && (ent->bad != true) &&
        (!ent->host || !Strcasecmp_charp(ent->host, host)) &&
        (!ent->port || ent->port == port) &&
        (!ent->uname || uname.empty() || uname == ent->uname->ptr) &&
        (!ent->realm || !realm || !Strcmp_charp(ent->realm, realm)))
      return ent;
  }
  return NULL;
}

void invalidate_auth_user_passwd(const Url &pu, const char *realm, Str *uname,
                                 Str *pwd, bool is_proxy) {
  auto ent =
      find_auth_pass_entry(pu.host.c_str(), pu.port, realm, {}, is_proxy);
  if (ent) {
    ent->bad = true;
  }
  return;
}

int find_auth_user_passwd(const Url &pu, const char *realm, Str **uname,
                          Str **pwd, bool is_proxy) {
  struct auth_pass *ent;

  if (pu.user.size() && pu.pass.size()) {
    *uname = Strnew(pu.user);
    *pwd = Strnew(pu.pass);
    return 1;
  }
  ent =
      find_auth_pass_entry(pu.host.c_str(), pu.port, realm, pu.user, is_proxy);
  if (ent) {
    *uname = ent->uname;
    *pwd = ent->pwd;
    return 1;
  }
  return 0;
}

#define FILE_IS_READABLE_MSG                                                   \
  "SECURITY NOTE: file %s must not be accessible by others"

FILE *openSecretFile(const char *fname) {
#ifdef _MSC_VER
  return {};
#else
  if (!fname) {
    return nullptr;
  }

  auto efname = expandPath(fname);
  struct stat st;
  if (stat(efname, &st) < 0) {
    return {};
  }

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
    App::instance().message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, 0, 0);
    sleep(2);
    return NULL;
  }

  return fopen(efname, "r");
#endif
}

void loadPasswd() {

  passwords = NULL;
  auto fp = openSecretFile(passwd_file);
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
