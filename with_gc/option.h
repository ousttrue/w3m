#pragma once

class Option {
  Option();

public:
  ~Option();
  Option(const Option &) = delete;
  Option &operator=(const Option &) = delete;
  static Option &instance();
};
