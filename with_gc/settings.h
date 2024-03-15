#pragma once

class Settings {
  Settings();

public:
  ~Settings();
  Settings(const Settings &) = delete;
  Settings &operator=(const Settings &) = delete;
  static Settings &instance();
};
