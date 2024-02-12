#include "w3m.h"
#include "ssl_util.h"
#include "downloadlist.h"
#include "etc.h"
#include "url_stream.h"
#include "quote.h"
#include "history.h"
#include "terms.h"
#include "display.h"
#include "tabbuffer.h"
#include "http_request.h"
#include "cookie.h"
#include "buffer.h"
#include "textlist.h"
#include "proto.h"
#include "myctype.h"

#define DEFAULT_COLS 80
#define SITECONF_FILE RC_DIR "/siteconf"
#define DEF_EDITOR "/usr/bin/vi"
#define DEF_MAILER "/usr/bin/mail"
#define DEF_EXT_BROWSER "/usr/bin/firefox"
#define PRE_FORM_FILE RC_DIR "/pre_form"

const char *w3m_reqlog = {};
bool fmInitialized = 0;
bool IsForkChild = 0;
bool confirm_on_quit = true;
int vi_prec_num = false;
int label_topline = false;
int displayLineInfo = false;
int show_srch_str = true;
int displayImage = false;
const char *Editor = DEF_EDITOR;
const char *Mailer = DEF_MAILER;
int MailtoOptions = MAILTO_OPTIONS_IGNORE;
const char *ExtBrowser = DEF_EXT_BROWSER;
char *ExtBrowser2 = nullptr;
char *ExtBrowser3 = nullptr;
char *ExtBrowser4 = nullptr;
char *ExtBrowser5 = nullptr;
char *ExtBrowser6 = nullptr;
char *ExtBrowser7 = nullptr;
char *ExtBrowser8 = nullptr;
char *ExtBrowser9 = nullptr;
int BackgroundExtViewer = true;
const char *pre_form_file = PRE_FORM_FILE;
const char *siteconf_file = SITECONF_FILE;
const char *ftppasswd = nullptr;
int ftppass_hostnamegen = true;
int WrapDefault = false;
int IgnoreCase = true;
int WrapSearch = false;
int squeezeBlankLine = false;
const char *BookmarkFile = nullptr;
int UseExternalDirBuffer = true;
const char *DirBufferCommand = "file:///$LIB/dirlist" CGI_EXTENSION;
int UseDictCommand = true;
const char *DictCommand = "file:///$LIB/w3mdict" CGI_EXTENSION;
int FoldTextarea = false;
int DefaultURLString = DEFAULT_URL_CURRENT;
struct auth_cookie *Auth_cookie = nullptr;
struct Cookie *First_cookie = nullptr;
char UseAltEntity = false;
int no_rc_dir = false;
char *param_tmp_dir = nullptr;
const char *mkd_tmp_dir = nullptr;
const char *config_file = nullptr;
int is_redisplay = false;
int clear_buffer = true;
int set_pixel_per_char = false;
const char *keymap_file = KEYMAP_FILE;
char *HostName = nullptr;
char *CurrentDir = nullptr;
int CurrentPid = {};
TextList *fileToDelete = nullptr;
char *document_root = nullptr;

void _quitfm(bool confirm) {
  const char *ans = "y";
  // if (checkDownloadList()) {
  //   ans = inputChar("Download process retains. "
  //                   "Do you want to exit w3m? (y/n)");
  // } else if (confirm) {
  //   ans = inputChar("Do you want to exit w3m? (y/n)");
  // }

  if (!(ans && TOLOWER(*ans) == 'y')) {
    // cancel
    displayBuffer(B_NORMAL);
    return;
  }

  // exit
  term_title(""); /* XXX */
  fmTerm();
  save_cookies();
  if (UseHistory && SaveURLHist)
    saveHistory(URLHist, URLHistSize);
  w3m_exit(0);
}

Url urlParse(const char *src, std::optional<Url> current) {
  auto url = Url::parse(src, current);
  if (url.schema == SCM_DATA) {
    return url;
  }

  if (url.schema == SCM_LOCAL) {
    auto q = expandName(file_unquote(url.file.c_str()));
    if (IS_ALPHA(q[0]) && q[1] == ':') {
      std::string drive(q, q + 2);
      drive += file_quote(q + 2);
      url.file = drive;
    } else {
      url.file = file_quote(q);
    }
  }

  int relative_uri = false;
  if (current &&
      (url.schema == current->schema ||
       (url.schema == SCM_LOCAL && current->schema == SCM_LOCAL_CGI)) &&
      url.host.empty()) {
    /* Copy omitted element from the current URL */
    url.user = current->user;
    url.pass = current->pass;
    url.host = current->host;
    url.port = current->port;
    if (url.file.size()) {
      if (url.file[0] != '/' &&
          !(url.schema == SCM_LOCAL && IS_ALPHA(url.file[0]) &&
            url.file[1] == ':')) {
        /* file is relative [process 1] */
        auto p = url.file;
        if (current->file.size()) {
          std::string tmp = current->file;
          while (tmp.size()) {
            if (tmp.back() == '/') {
              break;
            }
            tmp.pop_back();
          }
          tmp += p;
          url.file = tmp;
          relative_uri = true;
        }
      }
    } else { /* schema:[?query][#label] */
      url.file = current->file;
      if (url.query.empty()) {
        url.query = current->query;
      }
    }
    /* comment: query part need not to be completed
     * from the current URL. */
  }

  if (url.file.size()) {
    if (url.schema == SCM_LOCAL && url.file[0] != '/' &&
        !(IS_ALPHA(url.file[0]) && url.file[1] == ':') && url.file != "-") {
      /* local file, relative path */
      std::string tmp = CurrentDir;
      if (tmp.back() != '/') {
        tmp += '/';
      }
      tmp += file_unquote(url.file.c_str());
      url.file = file_quote(cleanupName(tmp.c_str()));
    } else if (url.schema == SCM_HTTP || url.schema == SCM_HTTPS) {
      if (relative_uri) {
        /* In this case, url.file is created by [process 1] above.
         * url.file may contain relative path (for example,
         * "/foo/../bar/./baz.html"), cleanupName() must be applied.
         * When the entire abs_path is given, it still may contain
         * elements like `//', `..' or `.' in the url.file. It is
         * server's responsibility to canonicalize such path.
         */
        url.file = cleanupName(url.file.c_str());
      }
    } else if (url.file[0] == '/') {
      /*
       * this happens on the following conditions:
       * (1) ftp schema (2) local, looks like absolute path.
       * In both case, there must be no side effect with
       * cleanupName(). (I hope so...)
       */
      url.file = cleanupName(url.file.c_str());
    }
    if (url.schema == SCM_LOCAL) {
      if (url.host.size() && !is_localhost(url.host.c_str())) {
        std::string tmp("//");
        tmp += url.host;
        tmp += cleanupName(file_unquote(url.file.c_str()));
        url.real_file = tmp;
      } else {
        url.real_file = cleanupName(file_unquote(url.file.c_str()));
      }
    }
  }
  return url;
}

void deleteFiles() {
  for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
    while (CurrentTab->firstBuffer /*&& Firstbuf != NO_BUFFER*/) {
      auto buf = CurrentTab->firstBuffer->backBuffer;
      CurrentTab->firstBuffer->discardBuffer();
      CurrentTab->firstBuffer = buf;
    }
  }
  while (auto f = popText(fileToDelete)) {
    unlink(f);
  }
}

void w3m_exit(int i) {
  stopDownload();
  deleteFiles();
  free_ssl_ctx();
  if (mkd_tmp_dir)
    if (rmdir(mkd_tmp_dir) != 0) {
      fprintf(stderr, "Can't remove temporary directory (%s)!\n", mkd_tmp_dir);
      exit(1);
    }
  exit(i);
}

#define HAVE_GETCWD 1
char *currentdir() {
  char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
  path = (char *)NewAtom_N(char, MAXPATHLEN);
  getcwd(path, MAXPATHLEN);
#else
  path = getcwd(NULL, 0);
#endif
#else /* not HAVE_GETCWD */
#ifdef HAVE_GETWD
  path = NewAtom_N(char, 1024);
  getwd(path);
#else  /* not HAVE_GETWD */
  FILE *f;
  char *p;
  path = (char *)NewAtom_N(char, 1024);
  f = popen("pwd", "r");
  fgets(path, 1024, f);
  pclose(f);
  for (p = path; *p; p++)
    if (*p == '\n') {
      *p = '\0';
      break;
    }
#endif /* not HAVE_GETWD */
#endif /* not HAVE_GETCWD */
  return path;
}
