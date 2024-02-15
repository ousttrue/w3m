#include "history.h"
#include "app.h"
#include "readbuffer.h"
#include "anchor.h"
#include "url_quote.h"
#include "url_stream.h"
#include "http_session.h"
#include "rc.h"
#include "message.h"
#include "buffer.h"
#include "proto.h"
#include "alloc.h"
#include <stdio.h>
#include <sys/stat.h>

#define HISTORY_FILE "history"
int UseHistory = true;
int URLHistSize = 100;
int SaveURLHist = true;

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

/* Merge entries from their history into ours */
static int mergeHistory(Hist *ours, Hist *theirs) {
  HistItem *item;

  for (item = theirs->list->first; item; item = item->next)
    if (!getHashHist(ours, (const char *)item->ptr))
      pushHist(ours, (const char *)item->ptr);

  return 0;
}

std::shared_ptr<Buffer> historyBuffer(Hist *hist) {
  Str *src = Strnew();
  HistItem *item;
  const char *p, *q;

  Strcat_charp(src, "<html>\n<head><title>History Page</title></head>\n");
  Strcat_charp(src, "<body>\n<h1>History Page</h1>\n<hr>\n");
  Strcat_charp(src, "<ol>\n");
  if (hist && hist->list) {
    for (item = hist->list->last; item; item = item->prev) {
      q = html_quote((const char *)item->ptr);
      if (DecodeURL)
        p = html_quote(url_decode0((const char *)item->ptr));
      else
        p = q;
      Strcat_charp(src, "<li><a href=\"");
      Strcat_charp(src, q);
      Strcat_charp(src, "\">");
      Strcat_charp(src, p);
      Strcat_charp(src, "</a>\n");
    }
  }
  Strcat_charp(src, "</ol>\n</body>\n</html>");
  return loadHTMLString(src);
}

int loadHistory(Hist *hist) {
  FILE *f;
  Str *line;
  struct stat st;

  if (hist == NULL)
    return 1;
  if ((f = fopen(rcFile(HISTORY_FILE), "rt")) == NULL)
    return 1;

  if (fstat(fileno(f), &st) == -1) {
    fclose(f);
    return 1;
  }
  hist->mtime = (long long)st.st_mtime;

  while (!feof(f)) {
    line = Strfgets(f);
    Strchop(line);
    Strremovefirstspaces(line);
    Strremovetrailingspaces(line);
    if (line->length == 0)
      continue;
    pushHist(hist, url_quote(line->ptr));
  }
  fclose(f);
  return 0;
}

void saveHistory(Hist *hist, int size) {
  FILE *f;
  Hist *fhist;
  HistItem *item;
  const char *histf;
  std::string tmpf;
  int rename_ret;
  struct stat st;

  if (hist == NULL || hist->list == NULL)
    return;

  histf = rcFile(HISTORY_FILE);
  if (stat(histf, &st) == -1)
    goto fail;
  if (hist->mtime != (long long)st.st_mtime) {
    fhist = newHist();
    if (loadHistory(fhist) || mergeHistory(fhist, hist))
      disp_err_message("Can't merge history");
    else
      hist = fhist;
  }

  tmpf = App::instance().tmpfname(TMPF_HIST, {});
  if ((f = fopen(tmpf.c_str(), "w")) == NULL)
    goto fail;
  for (item = hist->list->first; item && hist->list->nitem > size;
       item = item->next)
    size++;
  for (; item; item = item->next)
    fprintf(f, "%s\n", (const char *)item->ptr);
  if (fclose(f) == EOF)
    goto fail;
  rename_ret = rename(tmpf.c_str(), rcFile(HISTORY_FILE));
  if (rename_ret != 0)
    goto fail;

  return;

fail:
  disp_err_message("Can't open history");
  return;
}

/*
 * The following functions are used for internal stuff, we need them regardless
 * if history is used or not.
 */

Hist *newHist(void) {
  auto hist = (Hist *)New(Hist);
  hist->list = (HistList *)newGeneralList();
  hist->current = NULL;
  hist->hash = NULL;
  return hist;
}

Hist *copyHist(Hist *hist) {
  Hist *_new;
  HistItem *item;

  if (hist == NULL)
    return NULL;
  _new = newHist();
  for (item = hist->list->first; item; item = item->next)
    pushHist(_new, (const char *)item->ptr);
  return _new;
}

HistItem *unshiftHist(Hist *hist, const char *ptr) {
  HistItem *item;

  if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
    return NULL;
  item = (HistItem *)newListItem((void *)allocStr(ptr, -1),
                                 (ListItem *)hist->list->first, NULL);
  if (hist->list->first)
    hist->list->first->prev = item;
  else
    hist->list->last = item;
  hist->list->first = item;
  hist->list->nitem++;
  return item;
}

HistItem *pushHist(Hist *hist, std::string_view ptr) {
  HistItem *item;

  if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
    return NULL;
  item = (HistItem *)newListItem((void *)allocStr(ptr.data(), ptr.size()), NULL,
                                 (ListItem *)hist->list->last);
  if (hist->list->last)
    hist->list->last->next = item;
  else
    hist->list->first = item;
  hist->list->last = item;
  hist->list->nitem++;
  return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem *pushHashHist(Hist *hist, const char *ptr) {
  HistItem *item;

  if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
    return NULL;
  item = getHashHist(hist, ptr);
  if (item) {
    if (item->next)
      item->next->prev = item->prev;
    else /* item == hist->list->last */
      hist->list->last = item->prev;
    if (item->prev)
      item->prev->next = item->next;
    else /* item == hist->list->first */
      hist->list->first = item->next;
    hist->list->nitem--;
  }
  item = pushHist(hist, ptr);
  putHash_sv(hist->hash, ptr, (void *)item);
  return item;
}

HistItem *getHashHist(Hist *hist, const char *ptr) {
  HistItem *item;

  if (hist == NULL || hist->list == NULL)
    return NULL;
  if (hist->hash == NULL) {
    hist->hash = newHash_sv(HIST_HASH_SIZE);
    for (item = hist->list->first; item; item = item->next)
      putHash_sv(hist->hash, (const char *)item->ptr, (void *)item);
  }
  return (HistItem *)getHash_sv(hist->hash, ptr, NULL);
}

const char *lastHist(Hist *hist) {
  if (hist == NULL || hist->list == NULL)
    return NULL;
  if (hist->list->last) {
    hist->current = hist->list->last;
    return (const char *)hist->current->ptr;
  }
  return NULL;
}

const char *nextHist(Hist *hist) {
  if (hist == NULL || hist->list == NULL)
    return NULL;
  if (hist->current && hist->current->next) {
    hist->current = hist->current->next;
    return (char *)hist->current->ptr;
  }
  return NULL;
}

const char *prevHist(Hist *hist) {
  if (hist == NULL || hist->list == NULL)
    return NULL;
  if (hist->current && hist->current->prev) {
    hist->current = hist->current->prev;
    return (char *)hist->current->ptr;
  }
  return NULL;
}
