#include "file_util.h"
#include "quote.h"
#include "myctype.h"
#include "downloadlist.h"
#include "terms.h"
#include "etc.h"
#include "linein.h"
#include "Str.h"
#include "app.h"
#include "w3m.h"
#include "message.h"
#include "url_stream.h"
#include "istream.h"
#include "alloc.h"
#include <stdio.h>
#include <sys/stat.h>

int _MoveFile(const char *path1, const char *path2) {
#ifdef _MSC_VER
  return {};
#else
  FILE *f2;
  int is_pipe;
  long long linelen = 0, trbyte = 0;
  char *buf = nullptr;
  int count;

  auto f1 = openIS(path1);
  if (f1 == nullptr)
    return -1;
  if (*path2 == '|' && PermitSaveToPipe) {
    is_pipe = true;
    f2 = popen(path2 + 1, "w");
  } else {
    is_pipe = false;
    f2 = fopen(path2, "wb");
  }
  if (f2 == nullptr) {
    f1->close();
    return -1;
  }

  auto current_content_length = 0;
  buf = NewWithoutGC_N(char, SAVE_BUF_SIZE);
  while ((count = f1->ISread_n(buf, SAVE_BUF_SIZE)) > 0) {
    fwrite(buf, 1, count, f2);
    linelen += count;
    showProgress(&linelen, &trbyte, current_content_length);
  }

  xfree(buf);
  f1->close();
  if (is_pipe)
    pclose(f2);
  else
    fclose(f2);
  return 0;
#endif
}

int checkCopyFile(const char *path1, const char *path2) {
  struct stat st1, st2;

  if (*path2 == '|' && PermitSaveToPipe)
    return 0;
  if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int _doFileCopy(const char *tmpf, const char *defstr, int download) {
#ifdef _MSC_VER
  return {};
#else
  Str *msg;
  Str *filen;
  const char *p, *q = nullptr;
  int pid;
  struct stat st;
  long long size = 0;
  int is_pipe = false;

  if (fmInitialized) {
    p = App::instance().searchKeyData();
    if (p == nullptr || *p == '\0') {
      // q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
      //                   SaveHist);
      if (q == nullptr || *q == '\0')
        return false;
      p = q;
    }
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = true;
    else {
      if (q) {
        p = unescape_spaces(Strnew_charp(q))->ptr;
      }
      p = expandPath(p);
      if (!couldWrite(p))
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      msg = Sprintf("Can't copy. %s and %s are identical.", tmpf, p);
      disp_err_message(msg->ptr);
      return -1;
    }
    if (!download) {
      if (_MoveFile(tmpf, p) < 0) {
        msg = Sprintf("Can't save to %s", p);
        disp_err_message(msg->ptr);
      }
      return -1;
    }
    auto lock = App::instance().tmpfname(TMPF_DFL, ".lock");
    symlink(p, lock.c_str());
    flush_tty();
    pid = fork();
    if (!pid) {
      setup_child(false, 0, -1);
      if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe &&
          !stat(tmpf, &st))
        setModtime(p, st.st_mtime);
      unlink(lock.c_str());
      exit(0);
    }
    if (!stat(tmpf, &st))
      size = st.st_size;
    addDownloadList(pid, tmpf, p, Strnew(lock)->ptr, size);
  } else {
    q = App::instance().searchKeyData();
    if (q == nullptr || *q == '\0') {
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(char *)(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = q;
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = true;
    else {
      p = expandPath((char *)p);
      if (!couldWrite(p))
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      printf("Can't copy. %s and %s are identical.", tmpf, p);
      return -1;
    }
    if (_MoveFile(tmpf, p) < 0) {
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
      setModtime(p, st.st_mtime);
  }
  return 0;
#endif
}

int doFileMove(const char *tmpf, const char *defstr) {
  int ret = doFileCopy(tmpf, defstr);
  unlink(tmpf);
  return ret;
}

const char *shell_quote(const char *str) {
  Str *tmp = nullptr;
  const char *p;

  for (p = str; *p; p++) {
    if (is_shell_unsafe(*p)) {
      if (tmp == nullptr)
        tmp = Strnew_charp_n(str, (int)(p - str));
      Strcat_char(tmp, '\\');
      Strcat_char(tmp, *p);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp->ptr;
  return str;
}

int setModtime(const char *path, time_t modtime) {
#ifdef _MSC_VER
  return {};
#else
  struct utimbuf t;
  struct stat st;

  if (stat(path, &st) == 0)
    t.actime = st.st_atime;
  else
    t.actime = time(nullptr);
  t.modtime = modtime;
  return utime(path, &t);
#endif
}

#define PIPEBUFFERNAME "*stream*"

bool couldWrite(const char *path) {
  struct stat st;
  if (stat(path, &st) < 0) {
    // not exists
    return true;
  }

  // auto ans = inputAnswer("File exists. Overwrite? (y/n)");
  // if (ans && TOLOWER(*ans) == 'y'){
  //   return true;
  // }

  return false;
}
