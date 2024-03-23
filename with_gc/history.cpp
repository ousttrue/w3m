#include "history.h"
#include "url_decode.h"
#include "app.h"
#include "html/html_quote.h"
#include "url_quote.h"
#include "Str.h"
#include "tmpfile.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sstream>

#define HIST_LIST_MAX GENERAL_LIST_MAX
#define HIST_HASH_SIZE 127

bool UseHistory = true;
int URLHistSize = 100;
bool SaveURLHist = true;

std::shared_ptr<Hist> LoadHist;
std::shared_ptr<Hist> SaveHist;
std::shared_ptr<Hist> URLHist;
std::shared_ptr<Hist> ShellHist;
std::shared_ptr<Hist> TextHist;

/* Merge entries from their history into ours */
void Hist::mergeHistory(const Hist &theirs) {
  for (auto &item : theirs.list) {
    if (!this->getHashHist(item)) {
      this->push(item);
    }
  }
}

std::string Hist::toHtml() const {
  std::stringstream src;
  src << "<html>\n<head><title>History Page</title></head>\n";
  src << "<body>\n<h1>History Page</h1>\n<hr>\n";
  src << "<ol>\n";

  for (auto &item : this->list) {
    auto q = html_quote(item.c_str());
    std::string p;
    if (DecodeURL)
      p = html_quote(url_decode0(item));
    else
      p = q;
    src << "<li><a href=\"";
    src << q;
    src << "\">";
    src << p;
    src << "</a>\n";
  }
  src << "</ol>\n</body>\n</html>";
  return src.str();
}

bool Hist::loadHistory(const std::string &file) {
  if (auto f = fopen(file.c_str(), "rt")) {
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
      this->push(url_quote(line->ptr));
    }
    fclose(f);
    return true;
  } else {
    return false;
  }
}

void Hist::saveHistory(const std::string &histf, int size) {
  struct stat st;
  if (stat(histf.c_str(), &st) == -1) {
    // App::instance().disp_err_message("Can't open history");
    return;
  }

  if (this->mtime != (long long)st.st_mtime) {
    auto fhist = Hist::newHist();
    if (fhist->loadHistory(histf)) {
      this->mergeHistory(*fhist);
    } else {
      // App::instance().disp_err_message("Can't merge history");
    }
  }

  auto tmpf = TmpFile::instance().tmpfname(TMPF_HIST, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (!f) {
    App::instance().disp_err_message("Can't open history");
    return;
  }

  // for (item = this->list->first; item && this->list->nitem > size;
  //      item = item->next)
  //   size++;
  for (auto &item : this->list) {
    fprintf(f, "%s\n", item.c_str());
  }
  if (fclose(f) == EOF) {
    App::instance().disp_err_message("Can't open history");
    return;
  }
  auto rename_ret = rename(tmpf.c_str(), histf.c_str());
  if (rename_ret != 0) {
    App::instance().disp_err_message("Can't open history");
    return;
  }
}

/*
 * The following functions are used for internal stuff, we need them regardless
 * if history is used or not.
 */
Hist::Hist() {}

std::shared_ptr<Hist> Hist::newHist(void) {
  return std::shared_ptr<Hist>(new Hist);
}

std::shared_ptr<Hist> Hist::copyHist() const {
  auto _new = newHist();
  for (auto &item : this->list) {
    _new->push(item);
  }
  return _new;
}

void Hist::push(std::string_view ptr) {
  if (this->hash.insert({{ptr.begin(), ptr.end()}, true}).second) {
    this->list.push_back({ptr.begin(), ptr.end()});
  }
}

bool Hist::getHashHist(std::string_view ptr) const {
  return this->hash.contains({ptr.begin(), ptr.end()});
}

std::string_view Hist::lastHist() {
  if (this->list.empty()) {
    return {};
  }
  this->current = --this->list.end();
  return *this->current;
}

std::string_view Hist::nextHist() {
  if (this->list.empty()) {
    return {};
  }
  if (this->current == this->list.end()) {
    return {};
  }
  auto next = this->current;
  ++next;
  if (next == this->list.end()) {
    return {};
  }
  this->current = next;
  return *this->current;
}

std::string_view Hist::prevHist() {
  if (this->list.empty()) {
    return {};
  }
  if (this->current == this->list.end()) {
    return {};
  }
  auto prev = this->current;
  if (prev == this->list.begin()) {
    return {};
  }
  --prev;
  this->current = prev;
  return *this->current;
}
