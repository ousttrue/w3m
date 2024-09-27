#include "os.h"
#include "app.h"
#include "html.h"
#include "linein.h"
#include "ctrlcode.h"
#include "readbuffer.h"
#include "buffer.h"
#include "tabbuffer.h"
#include "tty.h"
#include "terms.h"
#include "file.h"
#include "termsize.h"
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "config.h"
#include "fm.h"
#include "proto.h"
#include "downloadlist.h"
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

int _doFileCopy(char *tmpf, char *defstr, int download) {
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
  int is_pipe = FALSE;

  if (term_is_initialized()) {
    p = searchKeyData();
    if (p == NULL || *p == '\0') {
      /* FIXME: gettextize? */
      q = inputLineHist("(Download)Save file to: ", defstr, IN_COMMAND,
                        SaveHist);
      if (q == NULL || *q == '\0')
        return FALSE;
      p = conv_to_system(q);
    }
    if (*p == '|' && PermitSaveToPipe)
      is_pipe = TRUE;
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
      disp_err_message(msg->ptr, FALSE);
      return -1;
    }
    if (!download) {
      if (_MoveFile(tmpf, p) < 0) {
        /* FIXME: gettextize? */
        msg = Sprintf("Can't save to %s", conv_from_system(p));
        disp_err_message(msg->ptr, FALSE);
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
      setup_child(FALSE, 0, -1);
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
      is_pipe = TRUE;
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

struct Buffer *doExternal(struct URLFile uf, char *type,
                          struct Buffer *defaultbuf) {
  Str tmpf, command;
  struct mailcap *mcap;
  int mc_stat;
  struct Buffer *buf = NULL;
  char *header, *src = NULL, *ext = uf.ext;

  if (!(mcap = searchExtViewer(type)))
    return NULL;

  if (mcap->nametemplate) {
    tmpf = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
    if (tmpf->ptr[0] == '.')
      ext = tmpf->ptr;
  }
  tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);

  if (IStype(uf.stream) != IST_ENCODED)
    uf.stream = newEncodedStream(uf.stream, uf.encoding);
  header = checkHeader(defaultbuf, "Content-Type:");
  if (header)
    header = conv_to_system(header);
  command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &mc_stat);
  if (!(mc_stat & MCSTAT_REPNAME)) {
    Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
    command = tmp;
  }

#ifdef HAVE_SETPGRP
  if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) &&
      !(mcap->flags & MAILCAP_NEEDSTERMINAL) && BackgroundExtViewer) {
    tty_flush();
    if (!fork()) {
      setup_child(FALSE, 0, UFfileno(&uf));
      if (save2tmp(uf, tmpf->ptr) < 0)
        exit(1);
      UFclose(&uf);
      myExec(command->ptr);
    }
    return NO_BUFFER;
  } else
#endif
  {
    if (save2tmp(uf, tmpf->ptr) < 0) {
      return NULL;
    }
  }
  if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) {
    if (defaultbuf == NULL)
      defaultbuf = newBuffer(INIT_BUFFER_WIDTH);
    if (defaultbuf->sourcefile)
      src = defaultbuf->sourcefile;
    else
      src = tmpf->ptr;
    defaultbuf->sourcefile = NULL;
    defaultbuf->mailcap = mcap;
  }
  if (mcap->flags & MAILCAP_HTMLOUTPUT) {
    buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->type = "text/html";
      buf->mailcap_source = buf->sourcefile;
      buf->sourcefile = src;
    }
  } else if (mcap->flags & MAILCAP_COPIOUSOUTPUT) {
    buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
    if (buf && buf != NO_BUFFER) {
      buf->type = "text/plain";
      buf->mailcap_source = buf->sourcefile;
      buf->sourcefile = src;
    }
  } else {
    if (mcap->flags & MAILCAP_NEEDSTERMINAL || !BackgroundExtViewer) {
      term_fmTerm();
      mySystem(command->ptr, 0);
      term_fmInit();
      if (CurrentTab && Currentbuf)
        displayBuffer(Currentbuf, B_FORCE_REDRAW);
    } else {
      mySystem(command->ptr, 1);
    }
    buf = NO_BUFFER;
  }
  if (buf && buf != NO_BUFFER) {
    if ((buf->buffername == NULL || buf->buffername[0] == '\0') &&
        buf->filename)
      buf->buffername = conv_from_system(lastFileName(buf->filename));
    buf->edit = mcap->edit;
    buf->mailcap = mcap;
  }
  return buf;
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
      disp_err_message(msg->ptr, FALSE);
      return -1;
    }
    /*
     * if (save2tmp(uf, p) < 0) {
     * msg = Sprintf("Can't save to %s", conv_from_system(p));
     * disp_err_message(msg->ptr, FALSE);
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
      setup_child(FALSE, 0, UFfileno(&uf));
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
