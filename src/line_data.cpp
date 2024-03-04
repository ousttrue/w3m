#include "line_data.h"
#include "html/anchor.h"

void LineData::clear() {
  lines.clear();
  this->_href->clear();
  this->_name->clear();
  this->_img->clear();
  this->_formitem->clear();
  this->formlist = nullptr;
  this->linklist = nullptr;
  this->maplist = nullptr;
  this->_hmarklist->clear();
  this->_imarklist->clear();
}

Anchor *LineData::registerName(const char *url, int line, int pos) {
  return this->name()->putAnchor(url, NULL, {}, NULL, '\0', line, pos);
}

Anchor *LineData::registerImg(const char *url, const char *title, int line,
                              int pos) {
  return this->img()->putAnchor(url, NULL, {}, title, '\0', line, pos);
}
