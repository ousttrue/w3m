#include "option.h"

Option::Option() {}

Option::~Option() {}

Option &Option::instance() {
  static Option s_instance;
  return s_instance;
}
