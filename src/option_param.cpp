#include "option_param.h"
#include "quote.h"
#include "myctype.h"

#include <sstream>

double pixel_per_char = DEFAULT_PIXEL_PER_CHAR;

int str_to_bool(const char *value, int old) {
  if (value == NULL)
    return 1;
  switch (TOLOWER(*value)) {
  case '0':
  case 'f': /* false */
  case 'n': /* no */
  case 'u': /* undef */
    return 0;
  case 'o':
    if (TOLOWER(value[1]) == 'f') /* off */
      return 0;
    return 1; /* on */
  case 't':
    if (TOLOWER(value[1]) == 'o') /* toggle */
      return !old;
    return 1; /* true */
  case '!':
  case 'r': /* reverse */
  case 'x': /* exchange */
    return !old;
  }
  return 1;
}

std::string param_ptr::toOptionPanelHtml() const {
  std::stringstream src;
  src << "<tr><td>" << this->comment;
  src << "</td><td width=" << static_cast<int>(28 * pixel_per_char) << ">";

  // switch (this->inputtype) {
  // case PI_TEXT:
  src << "<input type=text name=" << this->name << " value=\""
      << html_quote(this->to_str()) << "\">";
  // break;

  // }
  src << "</td></tr>\n";
  return src.str();
}

//
// param_int
//
int param_int::setParseValue(const std::string &src) {
  auto value = atoi(src.c_str());

  if (this->notZero) {
    //   case P_NZINT:
    if (value > 0) {
      *this->varptr = value;
    }
    //     break;
  } else {
    //   case P_INT:
    if (value >= 0) {
      // if (this->inputtype == PI_ONOFF) {
      //   *this->varptr = str_to_bool(src.c_str(), *this->varptr);
      // } else
      // {
      *this->varptr = value;
      // }
      //     break;
    }
  }

  return 1;
}

std::string param_int::to_str() const {
  std::stringstream ss;
  ss << this->varptr;
  return ss.str();
}

//
// param_int_select
//
std::string param_int_select::toOptionPanelHtml() const {
  std::stringstream src;
  src << "<tr><td>" << this->comment;
  src << "</td><td width=" << static_cast<int>(28 * pixel_per_char) << ">";

  // switch (this->inputtype) {
  // case PI_SEL_C: {
  auto tmp = this->to_str();
  src << "<select name=" << this->name << ">";
  for (auto s = this->select; s->text != NULL; s++) {
    src << "<option value=";
    src << s->cvalue << "\n";
    if (/*(this->type != P_CHAR &&*/ s->value == atoi(tmp.c_str()) /*||
        (this->type == P_CHAR && (char)s->value == *(tmp->c_str()))*/) {
      // if (this->isSelected()) {
      src << " selected";
    }
    src << '>';
    src << s->text;
  }
  src << "</select>";
  //   break;
  // }
  // }
  src << "</td></tr>\n";
  return src.str();
}

//
// param_pixels
//
int param_pixels::setParseValue(const std::string &value) {

  //   case P_PIXELS:
  auto ppc = atof(value.c_str());
  if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
    *this->varptr = ppc;
  }
  //     break;

  return 1;
}

std::string param_pixels::to_str() const {
  std::stringstream ss;
  ss << *this->varptr;
  return ss.str();
}

//
// param_bool
//
int param_bool::setParseValue(const std::string &src) {
  auto value = atoi(src.c_str());

  //   case P_INT:
  if (value >= 0) {
    // if (this->inputtype == PI_ONOFF) {
    *this->varptr = str_to_bool(src.c_str(), *this->varptr);
    // } //     break;
  }

  return 1;
}

std::string param_bool::to_str() const { return *this->varptr ? "1" : "0"; }

std::string param_bool::toOptionPanelHtml() const {
  std::stringstream src;
  src << "<tr><td>" << this->comment;
  src << "</td><td width=" << static_cast<int>(28 * pixel_per_char) << ">";

  // case PI_ONOFF: {
  auto x = atoi(this->to_str().c_str());
  src << "<input type=radio name=" << this->name << " value=1"
      << (x ? " checked" : "")
      << ">YES&nbsp;&nbsp;<input type=radio name=" << this->name << " value=0"
      << (x ? "" : " checked") << ">NO";
  //   break;
  src << "</td></tr>\n";
  return src.str();
}

//
// param_string
//
int param_string::setParseValue(const std::string &value) {

  //   case P_STRING:
  *this->varptr = value;
  //     break;

  return 1;
}

std::string param_string::to_str() const { return *this->varptr; }

// int param_ptr::setParseValue(const std::string &value) {
//   double ppc;
//   switch (p->type) {
//
//
//   case P_SHORT:
//     *(short *)p->varptr = (p->inputtype == PI_ONOFF)
//                               ? str_to_bool(value.c_str(), *(short
//                               *)p->varptr) : atoi(value.c_str());
//     break;
//
//   case P_CHARINT:
//     *(char *)p->varptr = (p->inputtype == PI_ONOFF)
//                              ? str_to_bool(value.c_str(), *(char
//                              *)p->varptr) : atoi(value.c_str());
//     break;
//
//   case P_CHAR:
//     *(char *)p->varptr = value[0];
//     break;
//
//
// #if defined(USE_SSL) && defined(USE_SSL_VERIFY)
//   case P_SSLPATH:
//     if (value.size())
//       *(const char **)p->varptr = rcFile(value.c_str());
//     else
//       *(char **)p->varptr = NULL;
//     ssl_path_modified = 1;
//     break;
// #endif
//
//
//   case P_SCALE:
//     ppc = atof(value.c_str());
//     if (ppc >= 10 && ppc <= 1000)
//       *(double *)p->varptr = ppc;
//     break;
//   }

//   return 1;
// }

// std::string param_ptr::to_str() const {
// switch (this->type) {
// case P_SHORT:
//   return Sprintf("%d", *(short *)this->varptr)->ptr;
// case P_CHARINT:
//   return Sprintf("%d", *(char *)this->varptr)->ptr;
// case P_CHAR:
//   return Sprintf("%c", *(char *)this->varptr)->ptr;
// case P_STRING:
// case P_SSLPATH:
//   /*  SystemCharset -> InnerCharset */
//   return Strnew_charp(*(char **)this->varptr)->ptr;
// }
/* not reached */
//   return "";
// }
