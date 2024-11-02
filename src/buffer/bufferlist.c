#include "buffer/bufferlist.h"
#include "buffer/buffer.h"
#include "buffer/document.h"
#include "input/content.h"
#include "term/scr.h"
#include "term/tty.h"
#include "text/ctrlcode.h"
#include <string.h>

static void writeBufferName(struct Buffer *buf, int n, struct TermSize size) {
  auto all = buf->document->allLine;
  if (all == 0 && buf->document->lastLine != NULL)
    all = buf->document->lastLine->linenumber;
  scr_move(n, 0);
  auto msg = Sprintf("<%s> [%d lines]", buf->document->title, all);
  if (buf->content->filename != NULL) {
    switch (buf->content->url.scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
      if (strcmp(buf->content->url.file, "-")) {
        Strcat_char(msg, ' ');
        Strcat_charp(msg, buf->content->url.real_file);
      }
      break;
    case SCM_UNKNOWN:
    case SCM_MISSING:
      break;
    default:
      Strcat_char(msg, ' ');
      Strcat(msg, parsedURL2Str(&buf->content->url));
      break;
    }
  }
  scr_addnstr_sup(msg->ptr, size.cols - 1);
}

static struct Buffer *listBuffer(struct Buffer *top, struct Buffer *current,
                                 struct TermSize size) {
  int i, c = 0;
  struct Buffer *buf = top;

  scr_move(0, 0);
  scr_clrtobotx();
  for (i = 0; i < size.lines - 1; i++) {
    if (buf == current) {
      c = i;
      scr_standout();
    }
    writeBufferName(buf, i, size);
    if (buf == current) {
      scr_standend();
      scr_clrtoeolx();
      scr_move(i, 0);
      scr_toggle_stand();
    } else
      scr_clrtoeolx();
    if (buf->nextBuffer == NULL) {
      scr_move(i + 1, 0);
      scr_clrtobotx();
      break;
    }
    buf = buf->nextBuffer;
  }
  scr_standout();
  /* FIXME: gettextize? */
  scr_message("Buffer selection mode: SPC for select / D for delete buffer", 0,
              0);
  scr_standend();
  /*
   * scr_move(LASTLINE, COLS - 1); */
  scr_move(c, 0);
  // term_refresh();
  return buf->nextBuffer;
}

/*
 * Select buffer visually
 */
struct Buffer *selectBuffer(struct Buffer *firstbuf, struct Buffer *currentbuf,
                            struct TermSize size, char *selectchar) {
  int i = 0;
  /* Current Buffer Number */
  int cpoint = 0;
  for (auto buf = firstbuf; buf; buf = buf->nextBuffer, ++i) {
    if (buf == currentbuf)
      cpoint = i;
    i++;
  }
  int maxbuf = i;

  struct Buffer *topbuf;
  int spoint; /* Current Line on Screen */
  int sclimit =
      size.lines - 1; /* Upper limit of line * number in the * screen */
  if (cpoint >= sclimit) {
    spoint = sclimit / 2;
    topbuf = nthBuffer(firstbuf, cpoint - spoint);
  } else {
    topbuf = firstbuf;
    spoint = cpoint;
  }
  listBuffer(topbuf, currentbuf, size);

  for (;;) {
    char c;
    if ((c = tty_getch()) == ESC_CODE) {
      if ((c = tty_getch()) == '[' || c == 'O') {
        switch (c = tty_getch()) {
        case 'A':
          c = 'k';
          break;
        case 'B':
          c = 'j';
          break;
        case 'C':
          c = ' ';
          break;
        case 'D':
          c = 'B';
          break;
        }
      }
    }
    switch (c) {
    case CTRL_N:
    case 'j':
      if (spoint < sclimit - 1) {
        if (currentbuf->nextBuffer == NULL)
          continue;
        writeBufferName(currentbuf, spoint, size);
        currentbuf = currentbuf->nextBuffer;
        cpoint++;
        spoint++;
        scr_standout();
        writeBufferName(currentbuf, spoint, size);
        scr_standend();
        scr_move(spoint, 0);
        scr_toggle_stand();
      } else if (cpoint < maxbuf - 1) {
        topbuf = currentbuf;
        currentbuf = currentbuf->nextBuffer;
        cpoint++;
        spoint = 1;
        listBuffer(topbuf, currentbuf, size);
      }
      break;
    case CTRL_P:
    case 'k':
      if (spoint > 0) {
        writeBufferName(currentbuf, spoint, size);
        currentbuf = nthBuffer(topbuf, --spoint);
        cpoint--;
        scr_standout();
        writeBufferName(currentbuf, spoint, size);
        scr_standend();
        scr_move(spoint, 0);
        scr_toggle_stand();
      } else if (cpoint > 0) {
        i = cpoint - sclimit;
        if (i < 0)
          i = 0;
        cpoint--;
        spoint = cpoint - i;
        currentbuf = nthBuffer(firstbuf, cpoint);
        topbuf = nthBuffer(firstbuf, i);
        listBuffer(topbuf, currentbuf, size);
      }
      break;
    default:
      *selectchar = c;
      return currentbuf;
    }
    /*
     * scr_move(LASTLINE, COLS - 1);
     */
    scr_move(spoint, 0);
    // term_refresh();
  }
}
