extern "C" {
#include "core.h"
#include "fm.h"
#include "proto.h"
}

DEFUN(nulcmd, NOTHING NULL @ @ @, "Do nothing") { /* do nothing */
}

DEFUN(escmap, ESCMAP, "ESC map") {
  char c;
  c = getch();
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESC, EscKeymap);
}

DEFUN(escbmap, ESCBMAP, "ESC [ map") {
  char c;
  c = getch();
  if (IS_DIGIT(c)) {
    escdmap(c);
    return;
  }
  if (IS_ASCII(c))
    escKeyProc((int)c, K_ESCB, EscBKeymap);
}

DEFUN(multimap, MULTIMAP, "multimap") {
  char c;
  c = getch();
  if (IS_ASCII(c)) {
    CurrentKey = K_MULTI | (CurrentKey << 16) | c;
    escKeyProc((int)c, 0, NULL);
  }
}

/* Move page backward */
DEFUN(pgBack, PREV_PAGE, "Scroll up one page") {
  if (vi_prec_num)
    nscroll(-searchKeyNum() * (Currentbuf->LINES - 1), B_NORMAL);
  else
    nscroll(
        -(prec_num ? searchKeyNum() : searchKeyNum() * (Currentbuf->LINES - 1)),
        prec_num ? B_SCROLL : B_NORMAL);
}

/* Move half page forward */
DEFUN(hpgFore, NEXT_HALF_PAGE, "Scroll down half a page") {
  nscroll(searchKeyNum() * (Currentbuf->LINES / 2 - 1), B_NORMAL);
}

/* Move half page backward */
DEFUN(hpgBack, PREV_HALF_PAGE, "Scroll up half a page") {
  nscroll(-searchKeyNum() * (Currentbuf->LINES / 2 - 1), B_NORMAL);
}

/* 1 line up */
DEFUN(lup1, UP, "Scroll the screen up one line") {
  nscroll(searchKeyNum(), B_SCROLL);
}

/* 1 line down */
DEFUN(ldown1, DOWN, "Scroll the screen down one line") {
  nscroll(-searchKeyNum(), B_SCROLL);
}

/* move cursor position to the center of screen */
DEFUN(ctrCsrV, CENTER_V, "Center on cursor line") {
  int offsety;
  if (Currentbuf->firstLine == NULL)
    return;
  offsety = Currentbuf->LINES / 2 - Currentbuf->cursorY;
  if (offsety != 0) {
    Currentbuf->topLine =
        lineSkip(Currentbuf, Currentbuf->topLine, -offsety, FALSE);
    arrangeLine(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

DEFUN(ctrCsrH, CENTER_H, "Center on cursor column") {
  int offsetx;
  if (Currentbuf->firstLine == NULL)
    return;
  offsetx = Currentbuf->cursorX - Currentbuf->COLS / 2;
  if (offsetx != 0) {
    columnSkip(Currentbuf, offsetx);
    arrangeCursor(Currentbuf);
    displayBuffer(Currentbuf, B_NORMAL);
  }
}

/* Redraw screen */
DEFUN(rdrwSc, REDRAW, "Draw the screen anew") {
  clear();
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Search regular expression forward */

DEFUN(srchfor, SEARCH SEARCH_FORE WHEREIS, "Search forward") {
  srch(forwardSearch, "Forward: ");
}

DEFUN(isrchfor, ISEARCH, "Incremental search forward") {
  isrch(forwardSearch, "I-search: ");
}

/* Search regular expression backward */

DEFUN(srchbak, SEARCH_BACK, "Search backward") {
  srch(backwardSearch, "Backward: ");
}

DEFUN(isrchbak, ISEARCH_BACK, "Incremental search backward") {
  isrch(backwardSearch, "I-search backward: ");
}

/* Search next matching */
DEFUN(srchnxt, SEARCH_NEXT, "Continue search forward") { srch_nxtprv(0); }

/* Search previous matching */
DEFUN(srchprv, SEARCH_PREV, "Continue search backward") { srch_nxtprv(1); }

static void shiftvisualpos(Buffer *buf, int shift) {
  Line *l = buf->currentLine;
  buf->visualpos -= shift;
  if (buf->visualpos - l->bwidth >= buf->COLS)
    buf->visualpos = l->bwidth + buf->COLS - 1;
  else if (buf->visualpos - l->bwidth < 0)
    buf->visualpos = l->bwidth;
  arrangeLine(buf);
  if (buf->visualpos - l->bwidth == -shift && buf->cursorX == 0)
    buf->visualpos = l->bwidth;
}

/* Shift screen left */
DEFUN(shiftl, SHIFT_LEFT, "Shift screen left") {
  int column;

  if (Currentbuf->firstLine == NULL)
    return;
  column = Currentbuf->currentColumn;
  columnSkip(Currentbuf, searchKeyNum() * (-Currentbuf->COLS + 1) + 1);
  shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Shift screen right */
DEFUN(shiftr, SHIFT_RIGHT, "Shift screen right") {
  int column;

  if (Currentbuf->firstLine == NULL)
    return;
  column = Currentbuf->currentColumn;
  columnSkip(Currentbuf, searchKeyNum() * (Currentbuf->COLS - 1) - 1);
  shiftvisualpos(Currentbuf, Currentbuf->currentColumn - column);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1R, RIGHT, "Shift screen one column right") {
  Buffer *buf = Currentbuf;
  Line *l = buf->currentLine;
  int j, column, n = searchKeyNum();

  if (l == NULL)
    return;
  for (j = 0; j < n; j++) {
    column = buf->currentColumn;
    columnSkip(Currentbuf, 1);
    if (column == buf->currentColumn)
      break;
    shiftvisualpos(Currentbuf, 1);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(col1L, LEFT, "Shift screen one column left") {
  Buffer *buf = Currentbuf;
  Line *l = buf->currentLine;
  int j, n = searchKeyNum();

  if (l == NULL)
    return;
  for (j = 0; j < n; j++) {
    if (buf->currentColumn == 0)
      break;
    columnSkip(Currentbuf, -1);
    shiftvisualpos(Currentbuf, -1);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(setEnv, SETENV, "Set environment variable") {
  char *env;
  char *var, *value;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  env = searchKeyData();
  if (env == NULL || *env == '\0' || strchr(env, '=') == NULL) {
    if (env != NULL && *env != '\0')
      env = Sprintf("%s=", env)->ptr;
    env = inputStrHist("Set environ: ", env, TextHist);
    if (env == NULL || *env == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  if ((value = strchr(env, '=')) != NULL && value > env) {
    var = allocStr(env, value - env);
    value++;
    set_environ(var, value);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(pipeBuf, PIPE_BUF,
      "Pipe current buffer through a shell command and display output") {
  Buffer *buf;
  char *cmd, *tmpf;
  FILE *f;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    /* FIXME: gettextize? */
    cmd = inputLineHist("Pipe buffer to: ", "", IN_COMMAND, ShellHist);
  }
  if (cmd != NULL)
    cmd = conv_to_system(cmd);
  if (cmd == NULL || *cmd == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
  f = fopen(tmpf, "w");
  if (f == NULL) {
    /* FIXME: gettextize? */
    disp_message(Sprintf("Can't save buffer to %s", cmd)->ptr, TRUE);
    return;
  }
  saveBuffer(Currentbuf, f, TRUE);
  fclose(f);
  buf = getpipe(myExtCommand(cmd, shell_quote(tmpf), TRUE)->ptr);
  if (buf == NULL) {
    disp_message("Execution failed", TRUE);
    return;
  } else {
    buf->filename = cmd;
    buf->buffername =
        Sprintf("%s %s", PIPEBUFFERNAME, conv_from_system(cmd))->ptr;
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (buf->type == NULL)
      buf->type = "text/plain";
    buf->currentURL.file = "-";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and read output ac pipe. */
DEFUN(pipesh, PIPE_SHELL, "Execute shell command and display output") {
  Buffer *buf;
  char *cmd;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    cmd = inputLineHist("(read shell[pipe])!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != NULL)
    cmd = conv_to_system(cmd);
  if (cmd == NULL || *cmd == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  buf = getpipe(cmd);
  if (buf == NULL) {
    disp_message("Execution failed", TRUE);
    return;
  } else {
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (buf->type == NULL)
      buf->type = "text/plain";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command and load entire output to buffer */
DEFUN(readsh, READ_SHELL, "Execute shell command and display output") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  char *cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    cmd = inputLineHist("(read shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != NULL)
    cmd = conv_to_system(cmd);
  if (cmd == NULL || *cmd == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  auto prevtrap = mySignal(SIGINT, intTrap);
  crmode();
  Buffer *buf = getshell(cmd);
  mySignal(SIGINT, prevtrap);
  term_raw();
  if (buf == NULL) {
    /* FIXME: gettextize? */
    disp_message("Execution failed", TRUE);
    return;
  } else {
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (buf->type == NULL)
      buf->type = "text/plain";
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Execute shell command */
DEFUN(execsh, EXEC_SHELL SHELL, "Execute shell command and display output") {
  char *cmd;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  cmd = searchKeyData();
  if (cmd == NULL || *cmd == '\0') {
    cmd = inputLineHist("(exec shell)!", "", IN_COMMAND, ShellHist);
  }
  if (cmd != NULL)
    cmd = conv_to_system(cmd);
  if (cmd != NULL && *cmd != '\0') {
    fmTerm();
    printf("\n");
    system(cmd);
    /* FIXME: gettextize? */
    printf("\n[Hit any key]");
    fflush(stdout);
    fmInit();
    getch();
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Load file */
DEFUN(ldfile, LOAD, "Open local file in a new buffer") {
  char *fn;

  fn = searchKeyData();
  if (fn == NULL || *fn == '\0') {
    /* FIXME: gettextize? */
    fn = inputFilenameHist("(Load)Filename? ", NULL, LoadHist);
  }
  if (fn != NULL)
    fn = conv_to_system(fn);
  if (fn == NULL || *fn == '\0') {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  cmd_loadfile(fn);
}

/* Load help file */
DEFUN(ldhelp, HELP, "Show help panel") {
  char *lang;
  int n;
  Str tmp;

  lang = AcceptLang;
  n = strcspn(lang, ";, \t");
  tmp = Sprintf("file:///$LIB/" HELP_CGI CGI_EXTENSION "?version=%s&lang=%s",
                Str_form_quote(Strnew_charp(w3m_version))->ptr,
                Str_form_quote(Strnew_charp_n(lang, n))->ptr);
  cmd_loadURL(tmp->ptr, NULL, NO_REFERER, NULL);
}

DEFUN(movL, MOVE_LEFT, "Cursor left") { _movL(Currentbuf->COLS / 2); }

DEFUN(movL1, MOVE_LEFT1, "Cursor left. With edge touched, slide") { _movL(1); }

DEFUN(movD, MOVE_DOWN, "Cursor down") { _movD((Currentbuf->LINES + 1) / 2); }

DEFUN(movD1, MOVE_DOWN1, "Cursor down. With edge touched, slide") { _movD(1); }

/* move cursor upward */
static void _movU(int n) {
  int i, m = searchKeyNum();
  if (Currentbuf->firstLine == NULL)
    return;
  for (i = 0; i < m; i++)
    cursorUp(Currentbuf, n);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movU, MOVE_UP, "Cursor up") { _movU((Currentbuf->LINES + 1) / 2); }

DEFUN(movU1, MOVE_UP1, "Cursor up. With edge touched, slide") { _movU(1); }

DEFUN(movR, MOVE_RIGHT, "Cursor right") { _movR(Currentbuf->COLS / 2); }

DEFUN(movR1, MOVE_RIGHT1, "Cursor right. With edge touched, slide") {
  _movR(1);
}

/* movLW, movRW */

DEFUN(movLW, PREV_WORD, "Move to the previous word") {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->firstLine == NULL)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->currentLine;
    ppos = Currentbuf->pos;

    if (prev_nonnull_line(Currentbuf->currentLine) < 0)
      goto end;

    while (1) {
      l = Currentbuf->currentLine;
      lb = l->lineBuf;
      while (Currentbuf->pos > 0) {
        int tmp = Currentbuf->pos;
        prevChar(tmp, l);
        if (is_wordchar(getChar(&lb[tmp])))
          break;
        Currentbuf->pos = tmp;
      }
      if (Currentbuf->pos > 0)
        break;
      if (prev_nonnull_line(Currentbuf->currentLine->prev) < 0) {
        Currentbuf->currentLine = pline;
        Currentbuf->pos = ppos;
        goto end;
      }
      Currentbuf->pos = Currentbuf->currentLine->len;
    }

    l = Currentbuf->currentLine;
    lb = l->lineBuf;
    while (Currentbuf->pos > 0) {
      int tmp = Currentbuf->pos;
      prevChar(tmp, l);
      if (!is_wordchar(getChar(&lb[tmp])))
        break;
      Currentbuf->pos = tmp;
    }
  }
end:
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(movRW, NEXT_WORD, "Move to the next word") {
  char *lb;
  Line *pline, *l;
  int ppos;
  int i, n = searchKeyNum();

  if (Currentbuf->firstLine == NULL)
    return;

  for (i = 0; i < n; i++) {
    pline = Currentbuf->currentLine;
    ppos = Currentbuf->pos;

    if (next_nonnull_line(Currentbuf->currentLine) < 0)
      goto end;

    l = Currentbuf->currentLine;
    lb = l->lineBuf;
    while (Currentbuf->pos < l->len &&
           is_wordchar(getChar(&lb[Currentbuf->pos])))
      nextChar(Currentbuf->pos, l);

    while (1) {
      while (Currentbuf->pos < l->len &&
             !is_wordchar(getChar(&lb[Currentbuf->pos])))
        nextChar(Currentbuf->pos, l);
      if (Currentbuf->pos < l->len)
        break;
      if (next_nonnull_line(Currentbuf->currentLine->next) < 0) {
        Currentbuf->currentLine = pline;
        Currentbuf->pos = ppos;
        goto end;
      }
      Currentbuf->pos = 0;
      l = Currentbuf->currentLine;
      lb = l->lineBuf;
    }
  }
end:
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Quit */
DEFUN(quitfm, ABORT EXIT, "Quit at once") { _quitfm(FALSE); }

/* Question and Quit */
DEFUN(qquitfm, QUIT, "Quit with confirmation request") {
  _quitfm(confirm_on_quit);
}

/* Select buffer */
DEFUN(selBuf, SELECT, "Display buffer-stack panel") {
  Buffer *buf;
  int ok;
  char cmd;

  ok = FALSE;
  do {
    buf = selectBuffer(Firstbuf, Currentbuf, &cmd);
    switch (cmd) {
    case 'B':
      ok = TRUE;
      break;
    case '\n':
    case ' ':
      Currentbuf = buf;
      ok = TRUE;
      break;
    case 'D':
      delBuffer(buf);
      if (Firstbuf == NULL) {
        /* No more buffer */
        Firstbuf = nullBuffer();
        Currentbuf = Firstbuf;
      }
      break;
    case 'q':
      qquitfm();
      break;
    case 'Q':
      quitfm();
      break;
    }
  } while (!ok);

  for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
    if (buf == Currentbuf)
      continue;
    deleteImage(buf);
    if (clear_buffer)
      tmpClearBuffer(buf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* Suspend (on BSD), or run interactive shell (on SysV) */
DEFUN(susp, INTERRUPT SUSPEND, "Suspend w3m to background") {
  char *shell;
  move(LASTLINE, 0);
  clrtoeolx();
  refresh();
  fmTerm();
  shell = getenv("SHELL");
  if (shell == NULL)
    shell = "/bin/sh";
  system(shell);
  fmInit();
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(goLine, GOTO_LINE, "Go to the specified line") {

  char *str = searchKeyData();
  if (prec_num)
    _goLine("^");
  else if (str)
    _goLine(str);
  else
    /* FIXME: gettextize? */
    _goLine(inputStr("Goto line: ", ""));
}

DEFUN(goLineF, BEGIN, "Go to the first line") { _goLine("^"); }

DEFUN(goLineL, END, "Go to the last line") { _goLine("$"); }

/* Go to the beginning of the line */
DEFUN(linbeg, LINE_BEGIN, "Go to the beginning of the line") {
  if (Currentbuf->firstLine == NULL)
    return;
  while (Currentbuf->currentLine->prev && Currentbuf->currentLine->bpos)
    cursorUp0(Currentbuf, 1);
  Currentbuf->pos = 0;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Go to the bottom of the line */
DEFUN(linend, LINE_END, "Go to the end of the line") {
  if (Currentbuf->firstLine == NULL)
    return;
  while (Currentbuf->currentLine->next && Currentbuf->currentLine->next->bpos)
    cursorDown0(Currentbuf, 1);
  Currentbuf->pos = Currentbuf->currentLine->len - 1;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* Run editor on the current buffer */
DEFUN(editBf, EDIT, "Edit local source") {
  char *fn = Currentbuf->filename;
  Str cmd;

  if (fn == NULL || Currentbuf->pagerSource != NULL || /* Behaving as a pager */
      (Currentbuf->type == NULL &&
       Currentbuf->edit == NULL) || /* Reading shell */
      Currentbuf->real_scheme != SCM_LOCAL ||
      !strcmp(Currentbuf->currentURL.file, "-") || /* file is std input  */
      Currentbuf->bufferprop & BP_FRAME) {         /* Frame */
    disp_err_message("Can't edit other than local file", TRUE);
    return;
  }
  if (Currentbuf->edit)
    cmd = unquote_mailcap(Currentbuf->edit, Currentbuf->real_type, fn,
                          checkHeader(Currentbuf, "Content-Type:"), NULL);
  else
    cmd = myEditor(Editor, shell_quote(fn), cur_real_linenumber(Currentbuf));
  fmTerm();
  system(cmd->ptr);
  fmInit();

  displayBuffer(Currentbuf, B_FORCE_REDRAW);
  reload();
}

/* Run editor on the current screen */
DEFUN(editScr, EDIT_SCREEN, "Edit rendered copy of document") {
  char *tmpf;
  FILE *f;

  tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
  f = fopen(tmpf, "w");
  if (f == NULL) {
    /* FIXME: gettextize? */
    disp_err_message(Sprintf("Can't open %s", tmpf)->ptr, TRUE);
    return;
  }
  saveBuffer(Currentbuf, f, TRUE);
  fclose(f);
  fmTerm();
  system(myEditor(Editor, shell_quote(tmpf), cur_real_linenumber(Currentbuf))
             ->ptr);
  fmInit();
  unlink(tmpf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* follow HREF link */
DEFUN(followA, GOTO_LINK, "Follow current hyperlink in a new buffer") {
  Anchor *a;
  ParsedURL u;
  int x = 0, y = 0, map = 0;
  char *url;

  if (Currentbuf->firstLine == NULL)
    return;

  a = retrieveCurrentImg(Currentbuf);
  if (a && a->image && a->image->map) {
    _followForm(FALSE);
    return;
  }
  if (a && a->image && a->image->ismap) {
    getMapXY(Currentbuf, a, &x, &y);
    map = 1;
  }
  a = retrieveCurrentAnchor(Currentbuf);
  if (a == NULL) {
    _followForm(FALSE);
    return;
  }
  if (*a->url == '#') { /* index within this buffer */
    gotoLabel(a->url + 1);
    return;
  }
  parseURL2(a->url, &u, baseURL(Currentbuf));
  if (Strcmp(parsedURL2Str(&u), parsedURL2Str(&Currentbuf->currentURL)) == 0) {
    /* index within this buffer */
    if (u.label) {
      gotoLabel(u.label);
      return;
    }
  }
  if (handleMailto(a->url))
    return;
  url = a->url;
  if (map)
    url = Sprintf("%s?%d,%d", a->url, x, y)->ptr;

  if (check_target && open_tab_blank && a->target &&
      (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
    Buffer *buf;

    _newT();
    buf = Currentbuf;
    loadLink(url, a->target, a->referer, NULL);
    if (buf != Currentbuf)
      delBuffer(buf);
    else
      deleteTab(CurrentTab);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  }
  loadLink(url, a->target, a->referer, NULL);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* view inline image */
DEFUN(followI, VIEW_IMAGE, "Display image in viewer") {
  Anchor *a;
  Buffer *buf;

  if (Currentbuf->firstLine == NULL)
    return;

  a = retrieveCurrentImg(Currentbuf);
  if (a == NULL)
    return;
  /* FIXME: gettextize? */
  message(Sprintf("loading %s", a->url)->ptr, 0, 0);
  refresh();
  buf = loadGeneralFile(a->url, baseURL(Currentbuf), NULL, 0, NULL);
  if (buf == NULL) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't load %s", a->url)->ptr;
    disp_err_message(emsg, FALSE);
  } else if (buf != NO_BUFFER) {
    pushBuffer(buf);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

/* submit form */
DEFUN(submitForm, SUBMIT, "Submit form") { _followForm(TRUE); }

/* go to the top anchor */
DEFUN(topA, LINK_BEGIN, "Move to the first hyperlink") {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq = 0;

  if (Currentbuf->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  if (prec_num > hl->nmark)
    hseq = hl->nmark - 1;
  else if (prec_num > 0)
    hseq = prec_num - 1;
  do {
    if (hseq >= hl->nmark)
      return;
    po = hl->marks + hseq;
    an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
    if (an == NULL)
      an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
    hseq++;
  } while (an == NULL);

  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the last anchor */
DEFUN(lastA, LINK_END, "Move to the last hyperlink") {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an;
  int hseq;

  if (Currentbuf->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  if (prec_num >= hl->nmark)
    hseq = 0;
  else if (prec_num > 0)
    hseq = hl->nmark - prec_num;
  else
    hseq = hl->nmark - 1;
  do {
    if (hseq < 0)
      return;
    po = hl->marks + hseq;
    an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
    if (an == NULL)
      an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
    hseq--;
  } while (an == NULL);

  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the nth anchor */
DEFUN(nthA, LINK_N, "Go to the nth link") {
  HmarkerList *hl = Currentbuf->hmarklist;
  BufferPoint *po;
  Anchor *an;

  int n = searchKeyNum();
  if (n < 0 || n > hl->nmark)
    return;

  if (Currentbuf->firstLine == NULL)
    return;
  if (!hl || hl->nmark == 0)
    return;

  po = hl->marks + n - 1;
  an = retrieveAnchor(Currentbuf->href, po->line, po->pos);
  if (an == NULL)
    an = retrieveAnchor(Currentbuf->formitem, po->line, po->pos);
  if (an == NULL)
    return;

  gotoLine(Currentbuf, po->line);
  Currentbuf->pos = po->pos;
  arrangeCursor(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* go to the next anchor */
DEFUN(nextA, NEXT_LINK, "Move to the next hyperlink") { _nextA(FALSE); }

/* go to the previous anchor */
DEFUN(prevA, PREV_LINK, "Move to the previous hyperlink") { _prevA(FALSE); }

/* go to the next visited anchor */
DEFUN(nextVA, NEXT_VISITED, "Move to the next visited hyperlink") {
  _nextA(TRUE);
}

/* go to the previous visited anchor */
DEFUN(prevVA, PREV_VISITED, "Move to the previous visited hyperlink") {
  _prevA(TRUE);
}

/* go to the next left anchor */
DEFUN(nextL, NEXT_LEFT, "Move left to the next hyperlink") { nextX(-1, 0); }

/* go to the next left-up anchor */
DEFUN(nextLU, NEXT_LEFT_UP, "Move left or upward to the next hyperlink") {
  nextX(-1, -1);
}

/* go to the next right anchor */
DEFUN(nextR, NEXT_RIGHT, "Move right to the next hyperlink") { nextX(1, 0); }

/* go to the next right-down anchor */
DEFUN(nextRD, NEXT_RIGHT_DOWN, "Move right or downward to the next hyperlink") {
  nextX(1, 1);
}

/* go to the next downward anchor */
DEFUN(nextD, NEXT_DOWN, "Move downward to the next hyperlink") { nextY(1); }

/* go to the next upward anchor */
DEFUN(nextU, NEXT_UP, "Move upward to the next hyperlink") { nextY(-1); }

/* go to the next bufferr */
DEFUN(nextBf, NEXT, "Switch to the next buffer") {
  Buffer *buf;
  int i;

  for (i = 0; i < PREC_NUM; i++) {
    buf = prevBuffer(Firstbuf, Currentbuf);
    if (!buf) {
      if (i == 0)
        return;
      break;
    }
    Currentbuf = buf;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* go to the previous bufferr */
DEFUN(prevBf, PREV, "Switch to the previous buffer") {
  Buffer *buf;
  int i;

  for (i = 0; i < PREC_NUM; i++) {
    buf = Currentbuf->nextBuffer;
    if (!buf) {
      if (i == 0)
        return;
      break;
    }
    Currentbuf = buf;
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* delete current buffer and back to the previous buffer */
DEFUN(backBf, BACK,
      "Close current buffer and return to the one below in stack") {
  Buffer *buf = Currentbuf->linkBuffer[LB_N_FRAME];

  if (!checkBackBuffer(Currentbuf)) {
    if (close_tab_back && nTab >= 1) {
      deleteTab(CurrentTab);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
    } else
      /* FIXME: gettextize? */
      disp_message("Can't go back...", TRUE);
    return;
  }

  delBuffer(Currentbuf);

  if (buf) {
    if (buf->frameQ) {
      struct frameset *fs;
      long linenumber = buf->frameQ->linenumber;
      long top = buf->frameQ->top_linenumber;
      int pos = buf->frameQ->pos;
      int currentColumn = buf->frameQ->currentColumn;
      AnchorList *formitem = buf->frameQ->formitem;

      fs = popFrameTree(&(buf->frameQ));
      deleteFrameSet(buf->frameset);
      buf->frameset = fs;

      if (buf == Currentbuf) {
        rFrame();
        Currentbuf->topLine =
            lineSkip(Currentbuf, Currentbuf->firstLine, top - 1, FALSE);
        gotoLine(Currentbuf, linenumber);
        Currentbuf->pos = pos;
        Currentbuf->currentColumn = currentColumn;
        arrangeCursor(Currentbuf);
        formResetBuffer(Currentbuf, formitem);
      }
    } else if (RenderFrame && buf == Currentbuf) {
      delBuffer(Currentbuf);
    }
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(deletePrevBuf, DELETE_PREVBUF,
      "Delete previous buffer (mainly for local CGI-scripts)") {
  Buffer *buf = Currentbuf->nextBuffer;
  if (buf)
    delBuffer(buf);
}

DEFUN(goURL, GOTO, "Open specified document in a new buffer") {
  goURL0("Goto URL: ", FALSE);
}

DEFUN(goHome, GOTO_HOME, "Open home page in a new buffer") {
  char *url;
  if ((url = getenv("HTTP_HOME")) != NULL ||
      (url = getenv("WWW_HOME")) != NULL) {
    ParsedURL p_url;
    Buffer *cur_buf = Currentbuf;
    SKIP_BLANKS(url);
    url = url_encode(url, NULL, 0);
    parseURL2(url, &p_url, NULL);
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    cmd_loadURL(url, NULL, NULL, NULL);
    if (Currentbuf != cur_buf) /* success */
      pushHashHist(URLHist, parsedURL2Str(&Currentbuf->currentURL)->ptr);
  }
}

DEFUN(gorURL, GOTO_RELATIVE, "Go to relative address") {
  goURL0("Goto relative URL: ", TRUE);
}

/* load bookmark */
DEFUN(ldBmark, BOOKMARK VIEW_BOOKMARK, "View bookmarks") {
  cmd_loadURL(BookmarkFile, NULL, NO_REFERER, NULL);
}

/* Add current to bookmark */
DEFUN(adBmark, ADD_BOOKMARK, "Add current page to bookmarks") {
  Str tmp;
  FormList *request;

  tmp = Sprintf("mode=panel&cookie=%s&bmark=%s&url=%s&title=%s"
                "&charset=%s",
                (Str_form_quote(localCookie()))->ptr,
                (Str_form_quote(Strnew_charp(BookmarkFile)))->ptr,
                (Str_form_quote(parsedURL2Str(&Currentbuf->currentURL)))->ptr,
                (Str_form_quote(wc_conv_strict(Currentbuf->buffername,
                                               InnerCharset, BookmarkCharset)))
                    ->ptr,
                wc_ces_to_charset(BookmarkCharset));
  request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
  request->body = tmp->ptr;
  request->length = tmp->length;
  cmd_loadURL("file:///$LIB/" W3MBOOKMARK_CMDNAME, NULL, NO_REFERER, request);
}

/* option setting */
DEFUN(ldOpt, OPTIONS, "Display options setting panel") {
  cmd_loadBuffer(load_option_panel(), BP_NO_URL, LB_NOLINK);
}

/* set an option */
DEFUN(setOpt, SET_OPTION, "Set option") {
  char *opt;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  opt = searchKeyData();
  if (opt == NULL || *opt == '\0' || strchr(opt, '=') == NULL) {
    if (opt != NULL && *opt != '\0') {
      char *v = get_param_option(opt);
      opt = Sprintf("%s=%s", opt, v ? v : "")->ptr;
    }
    opt = inputStrHist("Set option: ", opt, TextHist);
    if (opt == NULL || *opt == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  if (set_param_option(opt))
    sync_with_option();
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

/* error message list */
DEFUN(msgs, MSGS, "Display error messages") {
  cmd_loadBuffer(message_list_panel(), BP_NO_URL, LB_NOLINK);
}

/* page info */
DEFUN(pginfo, INFO, "Display information about the current document") {
  Buffer *buf;

  if ((buf = Currentbuf->linkBuffer[LB_N_INFO]) != NULL) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if ((buf = Currentbuf->linkBuffer[LB_INFO]) != NULL)
    delBuffer(buf);
  buf = page_info_panel(Currentbuf);
  cmd_loadBuffer(buf, BP_NORMAL, LB_INFO);
}

/* link menu */
DEFUN(linkMn, LINK_MENU, "Pop up link element menu") {
  LinkList *l = link_menu(Currentbuf);
  ParsedURL p_url;

  if (!l || !l->url)
    return;
  if (*(l->url) == '#') {
    gotoLabel(l->url + 1);
    return;
  }
  parseURL2(l->url, &p_url, baseURL(Currentbuf));
  pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
  cmd_loadURL(l->url, baseURL(Currentbuf),
              parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
}

/* accesskey */
DEFUN(accessKey, ACCESSKEY, "Pop up accesskey menu") {
  anchorMn(accesskey_menu, TRUE);
}

/* list menu */
DEFUN(listMn, LIST_MENU, "Pop up menu for hyperlinks to browse to") {
  anchorMn(list_menu, TRUE);
}

DEFUN(movlistMn, MOVE_LIST_MENU, "Pop up menu to navigate between hyperlinks") {
  anchorMn(list_menu, FALSE);
}

/* link,anchor,image list */
DEFUN(linkLst, LIST, "Show all URLs referenced") {
  Buffer *buf;

  buf = link_list_panel(Currentbuf);
  if (buf != NULL) {
    buf->document_charset = Currentbuf->document_charset;
    cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
  }
}

/* cookie list */
DEFUN(cooLst, COOKIE, "View cookie list") {
  Buffer *buf;

  buf = cookie_list_panel();
  if (buf != NULL)
    cmd_loadBuffer(buf, BP_NO_URL, LB_NOLINK);
}

/* History page */
DEFUN(ldHist, HISTORY, "Show browsing history") {
  cmd_loadBuffer(historyBuffer(URLHist), BP_NO_URL, LB_NOLINK);
}

/* download HREF link */
DEFUN(svA, SAVE_LINK, "Save hyperlink target") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  do_download = TRUE;
  followA();
  do_download = FALSE;
}

/* download IMG link */
DEFUN(svI, SAVE_IMAGE, "Save inline image") {
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  do_download = TRUE;
  followI();
  do_download = FALSE;
}

/* save buffer */
DEFUN(svBuf, PRINT SAVE_SCREEN, "Save rendered document") {
  char *qfile = NULL, *file;
  FILE *f;
  int is_pipe;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  file = searchKeyData();
  if (file == NULL || *file == '\0') {
    /* FIXME: gettextize? */
    qfile = inputLineHist("Save buffer to: ", NULL, IN_COMMAND, SaveHist);
    if (qfile == NULL || *qfile == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  file = conv_to_system(qfile ? qfile : file);
  if (*file == '|') {
    is_pipe = TRUE;
    f = popen(file + 1, "w");
  } else {
    if (qfile) {
      file = unescape_spaces(Strnew_charp(qfile))->ptr;
      file = conv_to_system(file);
    }
    file = expandPath(file);
    if (checkOverWrite(file) < 0) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
    f = fopen(file, "w");
    is_pipe = FALSE;
  }
  if (f == NULL) {
    /* FIXME: gettextize? */
    char *emsg = Sprintf("Can't open %s", conv_from_system(file))->ptr;
    disp_err_message(emsg, TRUE);
    return;
  }
  saveBuffer(Currentbuf, f, TRUE);
  if (is_pipe)
    pclose(f);
  else
    fclose(f);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* save source */
DEFUN(svSrc, DOWNLOAD SAVE, "Save document source") {
  char *file;

  if (Currentbuf->sourcefile == NULL)
    return;
  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  PermitSaveToPipe = TRUE;
  if (Currentbuf->real_scheme == SCM_LOCAL)
    file = conv_from_system(
        guess_save_name(NULL, Currentbuf->currentURL.real_file));
  else
    file = guess_save_name(Currentbuf, Currentbuf->currentURL.file);
  doFileCopy(Currentbuf->sourcefile, file);
  PermitSaveToPipe = FALSE;
  displayBuffer(Currentbuf, B_NORMAL);
}

/* peek URL */
DEFUN(peekURL, PEEK_LINK, "Show target address") { _peekURL(0); }

/* peek URL of image */
DEFUN(peekIMG, PEEK_IMG, "Show image address") { _peekURL(1); }

DEFUN(curURL, PEEK, "Show current address") {
  static Str s = NULL;
  static Lineprop *p = NULL;
  Lineprop *pp;
  static int offset = 0, n;

  if (Currentbuf->bufferprop & BP_INTERNAL)
    return;
  if (CurrentKey == prev_key && s != NULL) {
    if (s->length - offset >= COLS)
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
  } else {
    offset = 0;
    s = currentURL();
    if (DecodeURL)
      s = Strnew_charp(url_decode2(s->ptr, NULL));
    s = checkType(s, &pp, NULL);
    p = NewAtom_N(Lineprop, s->length);
    bcopy((void *)pp, (void *)p, s->length * sizeof(Lineprop));
  }
  n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS - 1))
    offset = (n - 1) * (COLS - 1);
  while (offset < s->length && p[offset] & PC_WCHAR2)
    offset++;
  disp_message_nomouse(&s->ptr[offset], TRUE);
}
/* view HTML source */

DEFUN(vwSrc, SOURCE VIEW, "Toggle between HTML shown or processed") {
  Buffer *buf;

  if (Currentbuf->type == NULL || Currentbuf->bufferprop & BP_FRAME)
    return;
  if ((buf = Currentbuf->linkBuffer[LB_SOURCE]) != NULL ||
      (buf = Currentbuf->linkBuffer[LB_N_SOURCE]) != NULL) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (Currentbuf->sourcefile == NULL) {
    if (Currentbuf->pagerSource &&
        !strcasecmp(Currentbuf->type, "text/plain")) {
      wc_ces old_charset;
      wc_bool old_fix_width_conv;
      FILE *f;
      Str tmpf = tmpfname(TMPF_SRC, NULL);
      f = fopen(tmpf->ptr, "w");
      if (f == NULL)
        return;
      old_charset = DisplayCharset;
      old_fix_width_conv = WcOption.fix_width_conv;
      DisplayCharset = (Currentbuf->document_charset != WC_CES_US_ASCII)
                           ? Currentbuf->document_charset
                           : 0;
      WcOption.fix_width_conv = WC_FALSE;
      saveBufferBody(Currentbuf, f, TRUE);
      DisplayCharset = old_charset;
      WcOption.fix_width_conv = old_fix_width_conv;
      fclose(f);
      Currentbuf->sourcefile = tmpf->ptr;
    } else {
      return;
    }
  }

  buf = newBuffer(INIT_BUFFER_WIDTH);

  if (is_html_type(Currentbuf->type)) {
    buf->type = "text/plain";
    if (Currentbuf->real_type && is_html_type(Currentbuf->real_type))
      buf->real_type = "text/plain";
    else
      buf->real_type = Currentbuf->real_type;
    buf->buffername = Sprintf("source of %s", Currentbuf->buffername)->ptr;
    buf->linkBuffer[LB_N_SOURCE] = Currentbuf;
    Currentbuf->linkBuffer[LB_SOURCE] = buf;
  } else if (!strcasecmp(Currentbuf->type, "text/plain")) {
    buf->type = "text/html";
    if (Currentbuf->real_type &&
        !strcasecmp(Currentbuf->real_type, "text/plain"))
      buf->real_type = "text/html";
    else
      buf->real_type = Currentbuf->real_type;
    buf->buffername = Sprintf("HTML view of %s", Currentbuf->buffername)->ptr;
    buf->linkBuffer[LB_SOURCE] = Currentbuf;
    Currentbuf->linkBuffer[LB_N_SOURCE] = buf;
  } else {
    return;
  }
  buf->currentURL = Currentbuf->currentURL;
  buf->real_scheme = Currentbuf->real_scheme;
  buf->filename = Currentbuf->filename;
  buf->sourcefile = Currentbuf->sourcefile;
  buf->header_source = Currentbuf->header_source;
  buf->search_header = Currentbuf->search_header;
  buf->document_charset = Currentbuf->document_charset;
  buf->clone = Currentbuf->clone;
  (*buf->clone)++;

  buf->need_reshape = TRUE;
  reshapeBuffer(buf);
  pushBuffer(buf);
  displayBuffer(Currentbuf, B_NORMAL);
}

/* reload */
DEFUN(reload, RELOAD, "Load current document anew") {
  Buffer *buf, *fbuf = NULL, sbuf;
  wc_ces old_charset;
  Str url;
  FormList *request;
  int multipart;

  if (Currentbuf->bufferprop & BP_INTERNAL) {
    if (!strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE)) {
      ldDL();
      return;
    }
    /* FIXME: gettextize? */
    disp_err_message("Can't reload...", TRUE);
    return;
  }
  if (Currentbuf->currentURL.scheme == SCM_LOCAL &&
      !strcmp(Currentbuf->currentURL.file, "-")) {
    /* file is std input */
    /* FIXME: gettextize? */
    disp_err_message("Can't reload stdin", TRUE);
    return;
  }
  copyBuffer(&sbuf, Currentbuf);
  if (Currentbuf->bufferprop & BP_FRAME &&
      (fbuf = Currentbuf->linkBuffer[LB_N_FRAME])) {
    if (fmInitialized) {
      message("Rendering frame", 0, 0);
      refresh();
    }
    if (!(buf = renderFrame(fbuf, 1))) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
    if (fbuf->linkBuffer[LB_FRAME]) {
      if (buf->sourcefile && fbuf->linkBuffer[LB_FRAME]->sourcefile &&
          !strcmp(buf->sourcefile, fbuf->linkBuffer[LB_FRAME]->sourcefile))
        fbuf->linkBuffer[LB_FRAME]->sourcefile = NULL;
      delBuffer(fbuf->linkBuffer[LB_FRAME]);
    }
    fbuf->linkBuffer[LB_FRAME] = buf;
    buf->linkBuffer[LB_N_FRAME] = fbuf;
    pushBuffer(buf);
    Currentbuf = buf;
    if (Currentbuf->firstLine) {
      COPY_BUFROOT(Currentbuf, &sbuf);
      restorePosition(Currentbuf, &sbuf);
    }
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    return;
  } else if (Currentbuf->frameset != NULL)
    fbuf = Currentbuf->linkBuffer[LB_FRAME];
  multipart = 0;
  if (Currentbuf->form_submit) {
    request = Currentbuf->form_submit->parent;
    if (request->method == FORM_METHOD_POST &&
        request->enctype == FORM_ENCTYPE_MULTIPART) {
      Str query;
      struct stat st;
      multipart = 1;
      query_from_followform(&query, Currentbuf->form_submit, multipart);
      stat(request->body, &st);
      request->length = st.st_size;
    }
  } else {
    request = NULL;
  }
  url = parsedURL2Str(&Currentbuf->currentURL);
  /* FIXME: gettextize? */
  message("Reloading...", 0, 0);
  refresh();
  old_charset = DocumentCharset;
  if (Currentbuf->document_charset != WC_CES_US_ASCII)
    DocumentCharset = Currentbuf->document_charset;
  SearchHeader = Currentbuf->search_header;
  DefaultType = Currentbuf->real_type;
  buf = loadGeneralFile(url->ptr, NULL, NO_REFERER, RG_NOCACHE, request);
  DocumentCharset = old_charset;
  SearchHeader = FALSE;
  DefaultType = NULL;

  if (multipart)
    unlink(request->body);
  if (buf == NULL) {
    /* FIXME: gettextize? */
    disp_err_message("Can't reload...", TRUE);
    return;
  } else if (buf == NO_BUFFER) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (fbuf != NULL)
    Firstbuf = deleteBuffer(Firstbuf, fbuf);
  repBuffer(Currentbuf, buf);
  if ((buf->type != NULL) && (sbuf.type != NULL) &&
      ((!strcasecmp(buf->type, "text/plain") && is_html_type(sbuf.type)) ||
       (is_html_type(buf->type) && !strcasecmp(sbuf.type, "text/plain")))) {
    vwSrc();
    if (Currentbuf != buf)
      Firstbuf = deleteBuffer(Firstbuf, buf);
  }
  Currentbuf->search_header = sbuf.search_header;
  Currentbuf->form_submit = sbuf.form_submit;
  if (Currentbuf->firstLine) {
    COPY_BUFROOT(Currentbuf, &sbuf);
    restorePosition(Currentbuf, &sbuf);
  }
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* reshape */
DEFUN(reshape, RESHAPE, "Re-render document") {
  Currentbuf->need_reshape = TRUE;
  reshapeBuffer(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(docCSet, CHARSET,
      "Change the character encoding for the current document") {
  char *cs;
  wc_ces charset;

  cs = searchKeyData();
  if (cs == NULL || *cs == '\0')
    /* FIXME: gettextize? */
    cs = inputStr("Document charset: ",
                  wc_ces_to_charset(Currentbuf->document_charset));
  charset = wc_guess_charset_short(cs, 0);
  if (charset == 0) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  _docCSet(charset);
}

DEFUN(defCSet, DEFAULT_CHARSET, "Change the default character encoding") {
  char *cs;
  wc_ces charset;

  cs = searchKeyData();
  if (cs == NULL || *cs == '\0')
    /* FIXME: gettextize? */
    cs = inputStr("Default document charset: ",
                  wc_ces_to_charset(DocumentCharset));
  charset = wc_guess_charset_short(cs, 0);
  if (charset != 0)
    DocumentCharset = charset;
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(chkURL, MARK_URL, "Turn URL-like strings into hyperlinks") {
  chkURLBuffer(Currentbuf);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(chkWORD, MARK_WORD, "Turn current word into hyperlink") {
  char *p;
  int spos, epos;
  p = getCurWord(Currentbuf, &spos, &epos);
  if (p == NULL)
    return;
  reAnchorWord(Currentbuf, Currentbuf->currentLine, spos, epos);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

/* render frames */
DEFUN(rFrame, FRAME, "Toggle rendering HTML frames") {
  Buffer *buf;

  if ((buf = Currentbuf->linkBuffer[LB_FRAME]) != NULL) {
    Currentbuf = buf;
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  if (Currentbuf->frameset == NULL) {
    if ((buf = Currentbuf->linkBuffer[LB_N_FRAME]) != NULL) {
      Currentbuf = buf;
      displayBuffer(Currentbuf, B_NORMAL);
    }
    return;
  }
  if (fmInitialized) {
    message("Rendering frame", 0, 0);
    refresh();
  }
  buf = renderFrame(Currentbuf, 0);
  if (buf == NULL) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  buf->linkBuffer[LB_N_FRAME] = Currentbuf;
  Currentbuf->linkBuffer[LB_FRAME] = buf;
  pushBuffer(buf);
  if (fmInitialized && display_ok)
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(extbrz, EXTERN, "Display using an external browser") {
  if (Currentbuf->bufferprop & BP_INTERNAL) {
    /* FIXME: gettextize? */
    disp_err_message("Can't browse...", TRUE);
    return;
  }
  if (Currentbuf->currentURL.scheme == SCM_LOCAL &&
      !strcmp(Currentbuf->currentURL.file, "-")) {
    /* file is std input */
    /* FIXME: gettextize? */
    disp_err_message("Can't browse stdin", TRUE);
    return;
  }
  invoke_browser(parsedURL2Str(&Currentbuf->currentURL)->ptr);
}

DEFUN(linkbrz, EXTERN_LINK, "Display target using an external browser") {
  Anchor *a;
  ParsedURL pu;

  if (Currentbuf->firstLine == NULL)
    return;
  a = retrieveCurrentAnchor(Currentbuf);
  if (a == NULL)
    return;
  parseURL2(a->url, &pu, baseURL(Currentbuf));
  invoke_browser(parsedURL2Str(&pu)->ptr);
}

/* show current line number and number of lines in the entire document */
DEFUN(curlno, LINE_INFO, "Display current position in document") {
  Line *l = Currentbuf->currentLine;
  Str tmp;
  int cur = 0, all = 0, col = 0, len = 0;

  if (l != NULL) {
    cur = l->real_linenumber;
    col = l->bwidth + Currentbuf->currentColumn + Currentbuf->cursorX + 1;
    while (l->next && l->next->bpos)
      l = l->next;
    if (l->width < 0)
      l->width = COLPOS(l, l->len);
    len = l->bwidth + l->width;
  }
  if (Currentbuf->lastLine)
    all = Currentbuf->lastLine->real_linenumber;
  if (Currentbuf->pagerSource && !(Currentbuf->bufferprop & BP_CLOSE))
    tmp = Sprintf("line %d col %d/%d", cur, col, len);
  else
    tmp = Sprintf("line %d/%d (%d%%) col %d/%d", cur, all,
                  (int)((double)cur * 100.0 / (double)(all ? all : 1) + 0.5),
                  col, len);
  Strcat_charp(tmp, "  ");
  Strcat_charp(tmp, wc_ces_to_charset_desc(Currentbuf->document_charset));

  disp_message(tmp->ptr, FALSE);
}

DEFUN(dispI, DISPLAY_IMAGE, "Restart loading and drawing of images") {
  if (!displayImage)
    initImage();
  if (!activeImage)
    return;
  displayImage = TRUE;
  /*
   * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
   * return;
   */
  Currentbuf->image_flag = IMG_FLAG_AUTO;
  Currentbuf->need_reshape = TRUE;
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(stopI, STOP_IMAGE, "Stop loading and drawing of images") {
  if (!activeImage)
    return;
  /*
   * if (!(Currentbuf->type && is_html_type(Currentbuf->type)))
   * return;
   */
  Currentbuf->image_flag = IMG_FLAG_SKIP;
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(dispVer, VERSION, "Display the version of w3m") {
  disp_message(Sprintf("w3m version %s", w3m_version)->ptr, TRUE);
}

DEFUN(wrapToggle, WRAP_TOGGLE, "Toggle wrapping mode in searches") {
  if (WrapSearch) {
    WrapSearch = FALSE;
    /* FIXME: gettextize? */
    disp_message("Wrap search off", TRUE);
  } else {
    WrapSearch = TRUE;
    /* FIXME: gettextize? */
    disp_message("Wrap search on", TRUE);
  }
}

DEFUN(dictword, DICT_WORD, "Execute dictionary command (see README.dict)") {
  execdict(inputStr("(dictionary)!", ""));
}

DEFUN(dictwordat, DICT_WORD_AT,
      "Execute dictionary command for word at cursor") {
  execdict(GetWord(Currentbuf));
}

DEFUN(execCmd, COMMAND, "Invoke w3m function(s)") {
  char *data, *p;
  int cmd;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == NULL || *data == '\0') {
    data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == NULL) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  /* data: FUNC [DATA] [; FUNC [DATA] ...] */
  while (*data) {
    SKIP_BLANKS(data);
    if (*data == ';') {
      data++;
      continue;
    }
    p = getWord(&data);
    cmd = getFuncList(p);
    if (cmd < 0)
      break;
    p = getQWord(&data);
    CurrentKey = -1;
    CurrentKeyData = NULL;
    CurrentCmdData = *p ? p : NULL;
    w3mFuncList[cmd].func();
    CurrentCmdData = NULL;
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(setAlarm, ALARM, "Set alarm") {
  char *data;
  int sec = 0, cmd = -1;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == NULL || *data == '\0') {
    data = inputStrHist("(Alarm)sec command: ", "", TextHist);
    if (data == NULL) {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  if (*data != '\0') {
    sec = atoi(getWord(&data));
    if (sec > 0)
      cmd = getFuncList(getWord(&data));
  }
  if (cmd >= 0) {
    data = getQWord(&data);
    setAlarmEvent(&DefaultAlarm, sec, AL_EXPLICIT, cmd, data);
    disp_message_nsec(
        Sprintf("%dsec %s %s", sec, w3mFuncList[cmd].id, data)->ptr, FALSE, 1,
        FALSE, TRUE);
  } else {
    setAlarmEvent(&DefaultAlarm, 0, AL_UNSET, FUNCNAME_nulcmd, NULL);
  }
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(reinit, REINIT, "Reload configuration file") {
  char *resource = searchKeyData();

  if (resource == NULL) {
    init_rc();
    sync_with_option();
    initCookie();
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
    return;
  }

  if (!strcasecmp(resource, "CONFIG") || !strcasecmp(resource, "RC")) {
    init_rc();
    sync_with_option();
    displayBuffer(Currentbuf, B_REDRAW_IMAGE);
    return;
  }

  if (!strcasecmp(resource, "COOKIE")) {
    initCookie();
    return;
  }

  if (!strcasecmp(resource, "KEYMAP")) {
    initKeymap(TRUE);
    return;
  }

  if (!strcasecmp(resource, "MAILCAP")) {
    initMailcap();
    return;
  }

  if (!strcasecmp(resource, "MENU")) {
    initMenu();
    return;
  }

  if (!strcasecmp(resource, "MIMETYPES")) {
    initMimeTypes();
    return;
  }

  if (!strcasecmp(resource, "URIMETHODS")) {
    initURIMethods();
    return;
  }

  disp_err_message(
      Sprintf("Don't know how to reinitialize '%s'", resource)->ptr, FALSE);
}

DEFUN(defKey, DEFINE_KEY,
      "Define a binding between a key stroke combination and a command") {
  char *data;

  CurrentKeyData = NULL; /* not allowed in w3m-control: */
  data = searchKeyData();
  if (data == NULL || *data == '\0') {
    data = inputStrHist("Key definition: ", "", TextHist);
    if (data == NULL || *data == '\0') {
      displayBuffer(Currentbuf, B_NORMAL);
      return;
    }
  }
  setKeymap(allocStr(data, -1), -1, TRUE);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(newT, NEW_TAB, "Open a new tab (with current document)") {
  _newT();
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(closeT, CLOSE_TAB, "Close tab") {
  TabBuffer *tab;

  if (nTab <= 1)
    return;
  if (prec_num)
    tab = numTab(PREC_NUM);
  else
    tab = CurrentTab;
  if (tab)
    deleteTab(tab);
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(nextT, NEXT_TAB, "Switch to the next tab") {
  int i;

  if (nTab <= 1)
    return;
  for (i = 0; i < PREC_NUM; i++) {
    if (CurrentTab->nextTab)
      CurrentTab = CurrentTab->nextTab;
    else
      CurrentTab = FirstTab;
  }
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(prevT, PREV_TAB, "Switch to the previous tab") {
  int i;

  if (nTab <= 1)
    return;
  for (i = 0; i < PREC_NUM; i++) {
    if (CurrentTab->prevTab)
      CurrentTab = CurrentTab->prevTab;
    else
      CurrentTab = LastTab;
  }
  displayBuffer(Currentbuf, B_REDRAW_IMAGE);
}

DEFUN(tabA, TAB_LINK, "Follow current hyperlink in a new tab") {
  followTab(prec_num ? numTab(PREC_NUM) : NULL);
}

DEFUN(tabURL, TAB_GOTO, "Open specified document in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : NULL, "Goto URL on new tab: ", FALSE);
}

DEFUN(tabrURL, TAB_GOTO_RELATIVE, "Open relative address in a new tab") {
  tabURL0(prec_num ? numTab(PREC_NUM) : NULL,
          "Goto relative URL on new tab: ", TRUE);
}

DEFUN(tabR, TAB_RIGHT, "Move right along the tab bar") {
  TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->nextTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : LastTab, TRUE);
}

DEFUN(tabL, TAB_LEFT, "Move left along the tab bar") {
  TabBuffer *tab;
  int i;

  for (tab = CurrentTab, i = 0; tab && i < PREC_NUM; tab = tab->prevTab, i++)
    ;
  moveTab(CurrentTab, tab ? tab : FirstTab, FALSE);
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display downloads panel") {
  Buffer *buf;
  int replace = FALSE, new_tab = FALSE;
  int reload;

  if (Currentbuf->bufferprop & BP_INTERNAL &&
      !strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE))
    replace = TRUE;
  if (!FirstDL) {
    if (replace) {
      if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == NULL) {
        if (nTab > 1)
          deleteTab(CurrentTab);
      } else
        delBuffer(Currentbuf);
      displayBuffer(Currentbuf, B_FORCE_REDRAW);
    }
    return;
  }
  reload = checkDownloadList();
  buf = DownloadListBuffer();
  if (!buf) {
    displayBuffer(Currentbuf, B_NORMAL);
    return;
  }
  buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
  if (replace) {
    COPY_BUFROOT(buf, Currentbuf);
    restorePosition(buf, Currentbuf);
  }
  if (!replace && open_tab_dl_list) {
    _newT();
    new_tab = TRUE;
  }
  pushBuffer(buf);
  if (replace || new_tab)
    deletePrevBuf();
  if (reload)
    Currentbuf->event =
        setAlarmEvent(Currentbuf->event, 1, AL_IMPLICIT, FUNCNAME_reload, NULL);
  displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement") {
  BufferPos *b = Currentbuf->undo;
  int i;

  if (!Currentbuf->firstLine)
    return;
  if (!b || !b->prev)
    return;
  for (i = 0; i < PREC_NUM && b->prev; i++, b = b->prev)
    ;
  resetPos(b);
}

DEFUN(redoPos, REDO, "Cancel the last undo") {
  BufferPos *b = Currentbuf->undo;
  int i;

  if (!Currentbuf->firstLine)
    return;
  if (!b || !b->next)
    return;
  for (i = 0; i < PREC_NUM && b->next; i++, b = b->next)
    ;
  resetPos(b);
}

DEFUN(cursorTop, CURSOR_TOP, "Move cursor to the top of the screen") {
  if (Currentbuf->firstLine == NULL)
    return;
  Currentbuf->currentLine = lineSkip(Currentbuf, Currentbuf->topLine, 0, FALSE);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorMiddle, CURSOR_MIDDLE, "Move cursor to the middle of the screen") {
  int offsety;
  if (Currentbuf->firstLine == NULL)
    return;
  offsety = (Currentbuf->LINES - 1) / 2;
  Currentbuf->currentLine =
      currentLineSkip(Currentbuf, Currentbuf->topLine, offsety, FALSE);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}

DEFUN(cursorBottom, CURSOR_BOTTOM, "Move cursor to the bottom of the screen") {
  int offsety;
  if (Currentbuf->firstLine == NULL)
    return;
  offsety = Currentbuf->LINES - 1;
  Currentbuf->currentLine =
      currentLineSkip(Currentbuf, Currentbuf->topLine, offsety, FALSE);
  arrangeLine(Currentbuf);
  displayBuffer(Currentbuf, B_NORMAL);
}
