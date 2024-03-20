#include "form_item.h"
#include "ioutil.h"
#include "etc.h"
#include "app.h"
#include "form.h"
#include "quote.h"
#include "html_tag.h"
#include "html_feed_env.h"
#include <sstream>

static const char *_formtypetbl[] = {
    "text",  "password", "checkbox", "radio",  "submit", "reset", "hidden",
    "image", "select",   "textarea", "button", "file",   NULL};

static const char *_formmethodtbl[] = {"GET", "POST", "INTERNAL", "HEAD"};

FormItemType formtype(const char *typestr) {
  int i;
  for (i = 0; _formtypetbl[i]; i++) {
    if (!strcasecmp(typestr, _formtypetbl[i]))
      return (FormItemType)i;
  }
  return FORM_INPUT_TEXT;
}

#define FORM_I_TEXT_DEFAULT_SIZE 40

std::shared_ptr<FormItem>
FormItem::createFromInput(html_feed_environ *h_env,
                          const std::shared_ptr<HtmlTag> &tag) {
  auto item = std::make_shared<FormItem>();
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->value = item->init_value = "";
  item->readonly = 0;
  char *p;
  if (tag->parsedtag_get_value(ATTR_TYPE, &p)) {
    item->type = formtype(p);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (tag->parsedtag_get_value(ATTR_NAME, &p))
    item->name = p;
  if (tag->parsedtag_get_value(ATTR_VALUE, &p))
    item->value = item->init_value = p;
  item->checked = item->init_checked = tag->parsedtag_exists(ATTR_CHECKED);
  item->accept = tag->parsedtag_exists(ATTR_ACCEPT);
  tag->parsedtag_get_value(ATTR_SIZE, &item->size);
  tag->parsedtag_get_value(ATTR_MAXLENGTH, &item->maxlength);
  item->readonly = tag->parsedtag_exists(ATTR_READONLY);
  int i;
  if (tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &i) && i >= 0 &&
      i < (int)h_env->parser.textarea_str.size()) {
    item->value = item->init_value = h_env->parser.textarea_str[i];
  }
  if (tag->parsedtag_get_value(ATTR_ROWS, &p))
    item->rows = atoi(p);
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

std::string FormItem::form2str() const {
  std::stringstream tmp;
  if (this->type != FORM_SELECT && this->type != FORM_TEXTAREA)
    tmp << "input type=";
  tmp << _formtypetbl[this->type];
  if (this->name.size())
    tmp << " name=\"" << this->name << "\"";
  if ((this->type == FORM_INPUT_RADIO || this->type == FORM_INPUT_CHECKBOX ||
       this->type == FORM_SELECT) &&
      this->value.size())
    tmp << " value=\"" << this->value << "\"";
  tmp << " (" << _formmethodtbl[this->parent->method] << " "
      << this->parent->action << ")";
  return tmp.str();
}

static void form_fputs_decode(Str *s, FILE *f) {
  char *p;
  Str *z = Strnew();

  for (p = s->ptr; *p;) {
    switch (*p) {

    case '\r':
      if (*(p + 1) == '\n')
        p++;
    default:
      Strcat_char(z, *p);
      p++;
      break;
    }
  }
  Strfputs(z, f);
}

void FormItem::input_textarea() {
  auto tmpf = App::instance().tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == NULL) {
    App::instance().disp_err_message("Can't open temporary file");
    return;
  }
  if (this->value.size()) {
    form_fputs_decode(Strnew(this->value), f);
  }
  fclose(f);

  if (exec_cmd(ioutil::myEditor(tmpf, 1))) {
    goto input_end;
  }

  if (this->readonly) {
    goto input_end;
  }

  f = fopen(tmpf.c_str(), "r");
  if (f == NULL) {
    App::instance().disp_err_message("Can't open temporary file");
    goto input_end;
  }
  this->value.clear();
  Str *tmp;
  while (tmp = Strfgets(f), tmp->length > 0) {
    if (tmp->length == 1 && tmp->ptr[tmp->length - 1] == '\n') {
      /* null line with bare LF */
      tmp = Strnew_charp("\r\n");
    } else if (tmp->length > 1 && tmp->ptr[tmp->length - 1] == '\n' &&
               tmp->ptr[tmp->length - 2] != '\r') {
      Strshrink(tmp, 1);
      Strcat_charp(tmp, "\r\n");
    }
    this->value = cleanup_line(tmp->ptr, RAW_MODE);
  }
  fclose(f);
input_end:
  unlink(tmpf.c_str());
}
