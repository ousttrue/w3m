#include "alloc.h"
#include "app.h"
#include "buffer.h"
#include "compression.h"
#include "config.h"
#include "ctrlcode.h"
#include "downloadlist.h"
#include "etc.h"
#include "file.h"
#include "fm.h"
#include "html.h"
#include "indep.h"
#include "istream.h"
#include "linein.h"
#include "myctype.h"
#include "os.h"
#include "proto.h"
#include "readbuffer.h"
#include "tabbuffer.h"
#include "terms.h"
#include "termsize.h"
#include "tty.h"
#include <netdb.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>

void mySystem(char *command, int background) {
  if (background) {
    tty_flush();
    if (!fork()) {
      setup_child(false, 0, -1);
      myExec(command);
    }
  } else {
    system(command);
  }
}

static int setModtime(char *path, time_t modtime) {
  struct utimbuf t;
  struct stat st;

  if (stat(path, &st) == 0)
    t.actime = st.st_atime;
  else
    t.actime = time(NULL);
  t.modtime = modtime;
  return utime(path, &t);
}

int checkCopyFile(char *path1, char *path2) {
  if (*path2 == '|' && PermitSaveToPipe)
    return 0;

  struct stat st1, st2;
  if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int _doFileCopy(const char *tmpf, const char *defstr, int download) {
  Str msg;
  Str filen;
  char *p, *q = NULL;
  pid_t pid;
  char *lock;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
  FILE *f;
#endif
  struct stat st;
  int64_t size = 0;
  int is_pipe = false;

  if (term_is_initialized()) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      /* FIXME: gettextize? */
      q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
                        SaveHist);
      if (q == NULL || *q == '\0')
        return false;
      p = conv_to_system(q);
    }
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = true;
    else {
      if (q) {
        p = unescape_spaces(Strnew_charp(q))->ptr;
        p = conv_to_system(p);
      }
      p = expandPath(p);
      if (checkOverWrite(p) < 0)
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      /* FIXME: gettextize? */
      msg = Sprintf("Can't copy. %s and %s are identical.",
                    conv_from_system(tmpf), conv_from_system(p));
      disp_err_message(msg->ptr, false);
      return -1;
    }
    if (!download) {
      if (_MoveFile(tmpf, p) < 0) {
        /* FIXME: gettextize? */
        msg = Sprintf("Can't save to %s", conv_from_system(p));
        disp_err_message(msg->ptr, false);
      }
      return -1;
    }
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#ifndef _WIN32
    symlink(p, lock);
#else
    auto f = fopen(lock, "w");
    if (f)
      fclose(f);
#endif
    tty_flush();
    pid = fork();
    if (!pid) {
      setup_child(false, 0, -1);
      if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe &&
          !stat(tmpf, &st))
        setModtime(p, st.st_mtime);
      unlink(lock);
      exit(0);
    }
    if (!stat(tmpf, &st))
      size = st.st_size;
    addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
  } else {
    q = searchKeyData();
    if (q == NULL || *q == '\0') {
      /* FIXME: gettextize? */
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = q;
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = true;
    else {
      p = expandPath(p);
      if (checkOverWrite(p) < 0)
        return -1;
    }
    if (checkCopyFile(tmpf, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't copy. %s and %s are identical.", tmpf, p);
      return -1;
    }
    if (_MoveFile(tmpf, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
      setModtime(p, st.st_mtime);
  }
  return 0;
}

int checkSaveFile(union input_stream *stream, char *path2) {
  struct stat st1, st2;
  int des = ISfileno(stream);

  if (des < 0)
    return 0;
  if (*path2 == '|' && PermitSaveToPipe)
    return 0;
  if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
    if (st1.st_ino == st2.st_ino)
      return -1;
  return 0;
}

int doFileSave(struct URLFile uf, char *defstr) {
  Str msg;
  Str filen;
  char *p, *q;
  pid_t pid;
  char *lock;
  char *tmpf = NULL;

  if (term_is_initialized()) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      /* FIXME: gettextize? */
      p = inputLineHist("(Download)Save file to: ", defstr, IN_FILENAME,
                        SaveHist);
      if (p == NULL || *p == '\0')
        return -1;
      p = conv_to_system(p);
    }
    if (checkOverWrite(p) < 0)
      return -1;
    if (checkSaveFile(uf.stream, p) < 0) {
      /* FIXME: gettextize? */
      msg = Sprintf("Can't save. Load file and %s are identical.",
                    conv_from_system(p));
      disp_err_message(msg->ptr, false);
      return -1;
    }
    /*
     * if (save2tmp(uf, p) < 0) {
     * msg = Sprintf("Can't save to %s", conv_from_system(p));
     * disp_err_message(msg->ptr, false);
     * }
     */
    lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#ifndef _WIN32
    symlink(p, lock);
#else
    auto f = fopen(lock, "w");
    if (f)
      fclose(f);
#endif
    tty_flush();
    pid = fork();
    if (!pid) {
      int err;
      if ((uf.content_encoding != CMP_NOCOMPRESS) && AutoUncompress) {
        uncompress_stream(&uf, &tmpf);
        if (tmpf)
          unlink(tmpf);
      }
      setup_child(false, 0, UFfileno(&uf));
      err = save2tmp(uf, p);
      if (err == 0 && PreserveTimestamp && uf.modtime != -1)
        setModtime(p, uf.modtime);
      UFclose(&uf);
      unlink(lock);
      if (err != 0)
        exit(-err);
      exit(0);
    }
    addDownloadList(pid, uf.url, p, lock, current_content_length);
  } else {
    q = searchKeyData();
    if (q == NULL || *q == '\0') {
      /* FIXME: gettextize? */
      printf("(Download)Save file to: ");
      fflush(stdout);
      filen = Strfgets(stdin);
      if (filen->length == 0)
        return -1;
      q = filen->ptr;
    }
    for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
      ;
    *(p + 1) = '\0';
    if (*q == '\0')
      return -1;
    p = expandPath(q);
    if (checkOverWrite(p) < 0)
      return -1;
    if (checkSaveFile(uf.stream, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't save. Load file and %s are identical.", p);
      return -1;
    }
    if (uf.content_encoding != CMP_NOCOMPRESS && AutoUncompress) {
      uncompress_stream(&uf, &tmpf);
      if (tmpf)
        unlink(tmpf);
    }
    if (save2tmp(uf, p) < 0) {
      /* FIXME: gettextize? */
      printf("Can't save to %s\n", p);
      return -1;
    }
    if (PreserveTimestamp && uf.modtime != -1)
      setModtime(p, uf.modtime);
  }
  return 0;
}

pid_t open_pipe_rw(FILE **fr, FILE **fw) {
  int fdr[2];
  int fdw[2];
  pid_t pid;

  if (fr && pipe(fdr) < 0)
    goto err0;
  if (fw && pipe(fdw) < 0)
    goto err1;

  tty_flush();
  pid = fork();
  if (pid < 0)
    goto err2;
  if (pid == 0) {
    /* child */
    if (fr) {
      close(fdr[0]);
      dup2(fdr[1], 1);
    }
    if (fw) {
      close(fdw[1]);
      dup2(fdw[0], 0);
    }
  } else {
    if (fr) {
      close(fdr[1]);
      if (*fr == stdin)
        dup2(fdr[0], 0);
      else
        *fr = fdopen(fdr[0], "r");
    }
    if (fw) {
      close(fdw[0]);
      if (*fw == stdout)
        dup2(fdw[1], 1);
      else
        *fw = fdopen(fdw[1], "w");
    }
  }
  return pid;
err2:
  if (fw) {
    close(fdw[0]);
    close(fdw[1]);
  }
err1:
  if (fr) {
    close(fdr[0]);
    close(fdr[1]);
  }
err0:
  return (pid_t)-1;
}

char *expandName(char *name) {
  char *p;
  struct passwd *passent, *getpwnam(const char *);
  Str extpath = NULL;

  if (name == NULL)
    return NULL;
  p = name;
  if (*p == '/') {
    if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root) {
      char *q;
      p += 2;
      q = strchr(p, '/');
      if (q) { /* /~user/dir... */
        passent = getpwnam(allocStr(p, q - p));
        p = q;
      } else { /* /~user */
        passent = getpwnam(p);
        p = "";
      }
      if (!passent)
        goto rest;
      extpath =
          Strnew_m_charp(passent->pw_dir, "/", personal_document_root, NULL);
      if (*personal_document_root == '\0' && *p == '/')
        p++;
    } else
      goto rest;
    if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
      p++;
    Strcat_charp(extpath, p);
    return extpath->ptr;
  } else
    return expandPath(p);
rest:
  return name;
}

void sleepSeconds(uint32_t seconds) { sleep(seconds); }
