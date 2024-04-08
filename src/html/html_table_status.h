#pragma once
#include "html_readbuffer_flags.h"
#include "html_command.h"
#include <memory>

struct TableStatus {
  std::shared_ptr<struct table> tbl;
  struct table_mode *tbl_mode = nullptr;
  int tbl_width = 0;

  ReadBufferFlags pre_mode(class html_feed_environ *h_env);
  HtmlCommand end_tag(html_feed_environ *h_env);
  bool is_active(html_feed_environ *h_env);
  int feed(html_feed_environ *parser, std::string_view str, bool internal);
};
