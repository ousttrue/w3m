#include "history.h"
#include "app.h"
#include "html/readbuffer.h"
#include "html/anchor.h"
#include "url_quote.h"
#include "url_stream.h"
#include "http_session.h"
#include "rc.h"
#include "buffer.h"
#include "proto.h"
// #include "alloc.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sstream>

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127
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
    if (!ours->getHashHist((const char *)item->ptr))
      ours->pushHist((const char *)item->ptr);

  return 0;
}

std::string Hist::toHtml() const {
  std::stringstream src;
  src << "<html>\n<head><title>History Page</title></head>\n";
  src << "<body>\n<h1>History Page</h1>\n<hr>\n";
  src << "<ol>\n";

  const char *p, *q;
  if (this->list) {
    for (auto item = this->list->last; item; item = item->prev) {
      q = html_quote((const char *)item->ptr);
      if (DecodeURL)
        p = html_quote(url_decode0((const char *)item->ptr));
      else
        p = q;
      src << "<li><a href=\"";
      src << q;
      src << "\">";
      src << p;
      src << "</a>\n";
    }
  }
  src << "</ol>\n</body>\n</html>";
  // return loadHTMLString(src);
  return src.str();
}

bool Hist::loadHistory() {
  if (auto f = fopen(rcFile(HISTORY_FILE), "rt")) {
    struct stat st;
    if (fstat(fileno(f), &st) == -1) {
      fclose(f);
      return false;
    }
    this->mtime = (long long)st.st_mtime;

    while (!feof(f)) {
      auto line = Strfgets(f);
      Strchop(line);
      Strremovefirstspaces(line);
      Strremovetrailingspaces(line);
      if (line->length == 0)
        continue;
      this->pushHist(url_quote(line->ptr));
    }
    fclose(f);
    return true;
  } else {
    return false;
  }
}

void Hist::saveHistory(int size) {
  FILE *f;
  HistItem *item;
  const char *histf;
  std::string tmpf;
  int rename_ret;
  struct stat st;

  if (this->list == NULL)
    return;

  histf = rcFile(HISTORY_FILE);
  if (stat(histf, &st) == -1) {
    goto fail;
  }

  if (this->mtime != (long long)st.st_mtime) {
    auto fhist = Hist::newHist();
    if (!fhist->loadHistory() || mergeHistory(this, fhist))
      App::instance().disp_err_message("Can't merge history");
  }

  tmpf = App::instance().tmpfname(TMPF_HIST, {});
  if ((f = fopen(tmpf.c_str(), "w")) == NULL)
    goto fail;
  for (item = this->list->first; item && this->list->nitem > size;
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
  App::instance().disp_err_message("Can't open history");
  return;
}

/*
 * The following functions are used for internal stuff, we need them regardless
 * if history is used or not.
 */
Hist::Hist() {}

Hist *Hist::newHist(void) {
  auto hist = new Hist;
  hist->list = (HistList *)newGeneralList();
  hist->current = NULL;
  hist->hash = NULL;
  return hist;
}

Hist *Hist::copyHist() const {
  auto _new = newHist();
  for (auto item = this->list->first; item; item = item->next)
    _new->pushHist((const char *)item->ptr);
  return _new;
}

HistItem *Hist::pushHist(std::string_view ptr) {
  if (this->list == NULL || this->list->nitem >= HIST_LIST_MAX)
    return NULL;
  auto item = (HistItem *)newListItem((void *)allocStr(ptr.data(), ptr.size()),
                                      NULL, (ListItem *)this->list->last);
  if (this->list->last)
    this->list->last->next = item;
  else
    this->list->first = item;
  this->list->last = item;
  this->list->nitem++;
  return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem *Hist::pushHashHist(const char *ptr) {
  if (this->list == NULL || this->list->nitem >= HIST_LIST_MAX)
    return NULL;
  auto item = this->getHashHist(ptr);
  if (item) {
    if (item->next)
      item->next->prev = item->prev;
    else /* item == this->list->last */
      this->list->last = item->prev;
    if (item->prev)
      item->prev->next = item->next;
    else /* item == this->list->first */
      this->list->first = item->next;
    this->list->nitem--;
  }
  item = this->pushHist(ptr);
  putHash_sv(this->hash, ptr, (void *)item);
  return item;
}

HistItem *Hist::getHashHist(const char *ptr) {
  if (this->list == NULL)
    return NULL;
  if (this->hash == NULL) {
    this->hash = newHash_sv(HIST_HASH_SIZE);
    for (auto item = this->list->first; item; item = item->next)
      putHash_sv(this->hash, (const char *)item->ptr, (void *)item);
  }
  return (HistItem *)getHash_sv(this->hash, ptr, NULL);
}

const char *Hist::lastHist() {
  if (this->list == NULL) {
    return NULL;
  }
  if (this->list->last) {
    this->current = this->list->last;
    return (const char *)this->current->ptr;
  }
  return NULL;
}

const char *Hist::nextHist() {
  if (this->list == NULL)
    return NULL;
  if (this->current && this->current->next) {
    this->current = this->current->next;
    return (char *)this->current->ptr;
  }
  return NULL;
}

const char *Hist::prevHist() {
  if (this->list == NULL)
    return NULL;
  if (this->current && this->current->prev) {
    this->current = this->current->prev;
    return (char *)this->current->ptr;
  }
  return NULL;
}
