#include "html_feed_env.h"
#include "form.h"
#include "readbuffer.h"
#include "stringtoken.h"
#include "ctrlcode.h"
#include "html_tag.h"
#include <assert.h>
#include <sstream>

void html_feed_environ::purgeline() {
  if (this->buf == NULL || this->obuf.blank_lines == 0)
    return;

  std::shared_ptr<TextLine> tl;
  if (!(tl = this->buf->rpopValue()))
    return;

  const char *p = tl->line.c_str();
  std::stringstream tmp;
  while (*p) {
    stringtoken st(p);
    auto token = st.sloppy_parse_line();
    p = st.ptr();
    if (token) {
      tmp << *token;
    }
  }
  this->buf->appendTextLine(tmp.str(), 0);
  this->obuf.blank_lines--;
}

void html_feed_environ::POP_ENV() {
  if (this->envc_real-- < (int)this->envs.size()) {
    this->envc--;
  }
}

void html_feed_environ::PUSH_ENV_NOINDENT(HtmlCommand cmd) {
  if (++this->envc_real < (int)this->envs.size()) {
    ++this->envc;
    envs[this->envc].env = cmd;
    envs[this->envc].count = 0;
    envs[this->envc].indent = envs[this->envc - 1].indent;
  }
}

void html_feed_environ::PUSH_ENV(HtmlCommand cmd) {
  if (++this->envc_real < (int)this->envs.size()) {
    ++this->envc;
    envs[this->envc].env = cmd;
    envs[this->envc].count = 0;
    if (this->envc <= MAX_INDENT_LEVEL)
      envs[this->envc].indent = envs[this->envc - 1].indent + INDENT_INCR;
    else
      envs[this->envc].indent = envs[this->envc - 1].indent;
  }
}

void html_feed_environ::parseLine(std::string_view istr, bool internal) {
  parser.parse(istr, this, internal);
  this->obuf.status = R_ST_NORMAL;
  this->completeHTMLstream();
  this->obuf.flushline(this->buf, 0, 2, this->limit);
}

void html_feed_environ::completeHTMLstream() {
  parser.completeHTMLstream(this);
}

std::shared_ptr<LineData>
html_feed_environ::render(const Url &currentUrl,
                          const std::shared_ptr<AnchorList<FormAnchor>> &old) {
  return parser.render(currentUrl, this, old);
}

#define FORM_I_TEXT_DEFAULT_SIZE 40
std::shared_ptr<FormItem>
html_feed_environ::createFormItem(const std::shared_ptr<HtmlTag> &tag) {
  auto item = std::make_shared<FormItem>();
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->value = item->init_value = "";
  item->readonly = 0;

  if (auto value = tag->getAttr(ATTR_TYPE)) {
    item->type = formtype(*value);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (auto value = tag->getAttr(ATTR_NAME)) {
    item->name = *value;
  }
  if (auto value = tag->getAttr(ATTR_VALUE)) {
    item->value = item->init_value = *value;
  }
  item->checked = item->init_checked = tag->existsAttr(ATTR_CHECKED);
  item->accept = tag->existsAttr(ATTR_ACCEPT);
  if (auto value = tag->getAttr(ATTR_SIZE)) {
    item->size = stoi(*value);
  }
  if (auto value = tag->getAttr(ATTR_MAXLENGTH)) {
    item->maxlength = stoi(*value);
  }
  item->readonly = tag->existsAttr(ATTR_READONLY);

  if (auto value = tag->getAttr(ATTR_TEXTAREANUMBER)) {
    auto i = stoi(*value);
    if (i >= 0 && i < (int)this->parser.textarea_str.size()) {
      item->value = item->init_value = this->parser.textarea_str[i];
    }
  }
  if (auto value = tag->getAttr(ATTR_ROWS)) {
    item->rows = stoi(*value);
  }
  if (item->type == FORM_UNKNOWN) {
    /* type attribute is missing. Ignore the tag. */
    return NULL;
  }
  if (item->type == FORM_INPUT_FILE && item->value.size()) {
    /* security hole ! */
    assert(false);
    return NULL;
  }
  return item;
}

std::shared_ptr<LineData>
loadHTMLstream(int width, const Url &currentURL, std::string_view body,
               const std::shared_ptr<AnchorList<FormAnchor>> &old,
               bool internal) {
  html_feed_environ htmlenv1(MAX_ENV_LEVEL, width, 0,
                             GeneralList::newGeneralList());
  htmlenv1.parseLine(body, internal);
  return htmlenv1.render(currentURL, old);
}
