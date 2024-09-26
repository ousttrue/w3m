#include "localcgi.h"
#include "fm.h"
#include "termsize.h"
#include <stdio.h>

#ifdef _WIN32
#include "rand48_win32.h"
#endif

static char *Local_cookie_file = NULL;
static Str Local_cookie = NULL;

static void writeLocalCookie() {
  if (no_rc_dir)
    return;

  if (Local_cookie_file)
    return;

  Local_cookie_file = tmpfname(TMPF_COOKIE, NULL)->ptr;
  set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
  auto f = fopen(Local_cookie_file, "wb");
  if (!f)
    return;
  localCookie();
  fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->length, f);
  fclose(f);
  chmod(Local_cookie_file, S_IRUSR | S_IWUSR);
}

/* setup cookie for local CGI */
Str localCookie() {
  if (Local_cookie)
    return Local_cookie;
  srand48((size_t)New(char) + (long)time(NULL));
  Local_cookie =
      Sprintf("%ld@%s", lrand48(), HostName ? HostName : "localhost");
  return Local_cookie;
}

#define CGIFN_NORMAL 0
#define CGIFN_LIBDIR 1
#define CGIFN_CGIBIN 2

Str loadLocalDir(char *dname) {
  Directory *dir;
  struct stat st;
  char *p;
  Str fbuf = Strnew();
#ifndef _WIN32
  struct stat lst;
#ifdef HAVE_READLINK
  char lbuf[1024];
#endif /* HAVE_READLINK */
#endif /* HAVE_LSTAT */
  int i, l, nrow = 0, n = 0, maxlen = 0;
  int nfile, nfile_max = 100;

#ifdef _WIN32
  WIN32_FIND_DATA FindFileData;
  auto hFind = FindFirstFile(dname, &FindFileData);
#else
  auto d = opendir(dname);
  if (d == NULL)
    return NULL;
#endif

  auto dirname = Strnew_charp(dname);
  if (Strlastchar(dirname) != '/')
    Strcat_char(dirname, '/');

  auto qdir = html_quote(Str_conv_from_system(dirname)->ptr);
  /* FIXME: gettextize? */
  auto tmp = Strnew_m_charp("<HTML>\n<HEAD>\n<BASE HREF=\"file://",
                            html_quote(file_quote(dirname->ptr)),
                            "\">\n<TITLE>Directory list of ", qdir,
                            "</TITLE>\n</HEAD>\n<BODY>\n<H1>Directory list of ",
                            qdir, "</H1>\n", NULL);
  char **flist = New_N(char *, nfile_max);
  nfile = 0;
#ifdef _WIN32
  for (auto enable = true; enable;
       enable = FindNextFile(hFind, &FindFileData)) {
    auto name = FindFileData.cFileName;
#else
  for (auto dir = readdir(d); dir; dir = readdir(d)) {
    auto name = dir->d_name;
#endif
    flist[nfile++] = allocStr(name, -1);
    if (nfile == nfile_max) {
      nfile_max *= 2;
      flist = New_Reuse(char *, flist, nfile_max);
    }
    if (multicolList) {
      l = strlen(name);
      if (l > maxlen)
        maxlen = l;
      n++;
    }
  }
#ifdef _WIN32
  FindClose(hFind);
#else
  closedir(d);
#endif

  if (multicolList) {
    l = COLS / (maxlen + 2);
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
#ifndef WIN32
    if (lstat(fbuf->ptr, &lst) < 0)
      continue;
#endif /* HAVE_LSTAT */
    if (stat(fbuf->ptr, &st) < 0)
      continue;
    if (multicolList) {
      if (n == 1)
        Strcat_charp(tmp, "<TD><NOBR>");
    } else {
#ifndef _WIN32 
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
    Strcat_m_charp(tmp, "\">", html_quote(conv_from_system(p)), NULL);
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
#ifndef _WIN32
      if (S_ISLNK(lst.st_mode)) {
        if ((l = readlink(fbuf->ptr, lbuf, sizeof(lbuf) - 1)) > 0) {
          lbuf[l] = '\0';
          Strcat_m_charp(tmp, " -> ", html_quote(conv_from_system(lbuf)), NULL);
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
}
