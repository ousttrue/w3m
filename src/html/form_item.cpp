#include "form_item.h"
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

std::string FormItemList::form2str() const {
  std::stringstream tmp;
  if (this->type != FORM_SELECT && this->type != FORM_TEXTAREA)
    tmp << "input type=";
  tmp << _formtypetbl[this->type];
  if (this->name && this->name->length)
    tmp << " name=\"" << this->name->ptr << "\"";
  if ((this->type == FORM_INPUT_RADIO || this->type == FORM_INPUT_CHECKBOX ||
       this->type == FORM_SELECT) &&
      this->value)
    tmp << " value=\"" << this->value->ptr << "\"";
  tmp << " (" << _formmethodtbl[this->parent->method] << " "
      << this->parent->action->ptr << ")";
  return tmp.str();
}
