#include "form_item.h"
#include "ioutil.h"
#include "readbuffer.h"
#include "etc.h"
#include "app.h"
#include "form.h"
#include "quote.h"
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

std::string FormItem::form2str() const {
  std::stringstream tmp;
  if (this->type != FORM_SELECT && this->type != FORM_TEXTAREA)
    tmp << "input type=";
  tmp << _formtypetbl[this->type];
  if (this->name.size())
    tmp << " name=\"" << this->name << "\"";
  if ((this->type == FORM_INPUT_RADIO || this->type == FORM_INPUT_CHECKBOX ||
       this->type == FORM_SELECT) &&
      this->value)
    tmp << " value=\"" << this->value->ptr << "\"";
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
  if (this->value) {
    form_fputs_decode(this->value, f);
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
  this->value = Strnew();
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
    cleanup_line(tmp, RAW_MODE);
    Strcat(this->value, tmp);
  }
  fclose(f);
input_end:
  unlink(tmpf.c_str());
}
