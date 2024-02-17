#include "loaddirectory.h"
#include "url_stream.h"
#include "html/readbuffer.h"
#include "terms.h"
#include "Str.h"
#include "etc.h"
#include "alloc.h"

typedef struct direct Directory;
#include <sys/stat.h>
#include <string.h>
#ifdef _MSC_VER
#else
#include <sys/dir.h>
#endif

bool multicolList = false;

static int strCmp(const void *s1, const void *s2) {
  return strcmp(*(const char **)s1, *(const char **)s2);
}

Str *loadLocalDir(const char *dname) {
#ifdef _MSC_VER
  return {};
#else
  Str *tmp;
  DIR *d;
  Directory *dir;
  struct stat st;
  char **flist;
  const char *p, *qdir;
  Str *fbuf = Strnew();
#ifdef HAVE_LSTAT
  struct stat lst;
#ifdef HAVE_READLINK
  char lbuf[1024];
#endif /* HAVE_READLINK */
#endif /* HAVE_LSTAT */
  int i, l, nrow = 0, n = 0, maxlen = 0;
  int nfile, nfile_max = 100;
  Str *dirname;

  d = opendir(dname);
  if (d == NULL)
    return NULL;
  dirname = Strnew_charp(dname);
  if (Strlastchar(dirname) != '/')
    Strcat_char(dirname, '/');
  qdir = html_quote(dirname->ptr);
  tmp = Strnew_m_charp("<HTML>\n<HEAD>\n<BASE HREF=\"file://",
                       html_quote(file_quote(dirname->ptr)),
                       "\">\n<TITLE>Directory list of ", qdir,
                       "</TITLE>\n</HEAD>\n<BODY>\n<H1>Directory list of ",
                       qdir, "</H1>\n", NULL);
  flist = (char **)New_N(char *, nfile_max);
  nfile = 0;
  while ((dir = readdir(d)) != NULL) {
    flist[nfile++] = allocStr(dir->d_name, -1);
    if (nfile == nfile_max) {
      nfile_max *= 2;
      flist = (char **)New_Reuse(char *, flist, nfile_max);
    }
    if (multicolList) {
      l = strlen(dir->d_name);
      if (l > maxlen)
        maxlen = l;
      n++;
    }
  }
  closedir(d);

  if (multicolList) {
    l = COLS() / (maxlen + 2);
    if (!l)
      l = 1;
    nrow = (n + l - 1) / l;
    n = 1;
    Strcat_charp(tmp, "<TABLE CELLPADDING=0>\n<TR VALIGN=TOP>\n");
  }
  qsort((void *)flist, nfile, sizeof(char *), strCmp);
  for (i = 0; i < nfile; i++) {
    p = flist[i];
    if (strcmp(p, ".") == 0)
      continue;
    Strcopy(fbuf, dirname);
    if (Strlastchar(fbuf) != '/')
      Strcat_char(fbuf, '/');
    Strcat_charp(fbuf, p);
#ifdef HAVE_LSTAT
    if (lstat(fbuf->ptr, &lst) < 0)
      continue;
#endif /* HAVE_LSTAT */
    if (stat(fbuf->ptr, &st) < 0)
      continue;
    if (multicolList) {
      if (n == 1)
        Strcat_charp(tmp, "<TD><NOBR>");
    } else {
#ifdef HAVE_LSTAT
      if (S_ISLNK(lst.st_mode))
        Strcat_charp(tmp, "[LINK] ");
      else
#endif /* HAVE_LSTAT */
        if (S_ISDIR(st.st_mode))
          Strcat_charp(tmp, "[DIR]&nbsp; ");
        else
          Strcat_charp(tmp, "[FILE] ");
    }
    Strcat_m_charp(tmp, "<A HREF=\"", html_quote(file_quote(p)), NULL);
    if (S_ISDIR(st.st_mode))
      Strcat_char(tmp, '/');
    Strcat_m_charp(tmp, "\">", html_quote(p), NULL);
    if (S_ISDIR(st.st_mode))
      Strcat_char(tmp, '/');
    Strcat_charp(tmp, "</A>");
    if (multicolList) {
      if (n++ == nrow) {
        Strcat_charp(tmp, "</NOBR></TD>\n");
        n = 1;
      } else {
        Strcat_charp(tmp, "<BR>\n");
      }
    } else {
#if defined(HAVE_LSTAT) && defined(HAVE_READLINK)
      if (S_ISLNK(lst.st_mode)) {
        if ((l = readlink(fbuf->ptr, lbuf, sizeof(lbuf) - 1)) > 0) {
          lbuf[l] = '\0';
          Strcat_m_charp(tmp, " -> ", html_quote(lbuf), NULL);
          if (S_ISDIR(st.st_mode))
            Strcat_char(tmp, '/');
        }
      }
#endif /* HAVE_LSTAT && HAVE_READLINK */
      Strcat_charp(tmp, "<br>\n");
    }
  }
  if (multicolList) {
    Strcat_charp(tmp, "</TR>\n</TABLE>\n");
  }
  Strcat_charp(tmp, "</BODY>\n</HTML>\n");

  return tmp;
#endif
}
