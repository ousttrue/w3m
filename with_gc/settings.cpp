#include "settings.h"

Settings::Settings() {}

Settings::~Settings() {}

Settings &Settings::instance() {
  static Settings s_instance;
  return s_instance;
}
