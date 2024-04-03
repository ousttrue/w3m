#include "html_table_status.h"
#include "html_feed_env.h"
#include "html_table.h"

ReadBufferFlags TableStatus::pre_mode(html_feed_environ *h_env) {
  return (h_env->table_level >= 0 && tbl_mode) ? tbl_mode->pre_mode
                                               : h_env->flag;
}

HtmlCommand TableStatus::end_tag(html_feed_environ *h_env) {
  return (h_env->table_level >= 0 && tbl_mode) ? tbl_mode->end_tag
                                               : h_env->end_tag;
}

bool TableStatus::is_active(html_feed_environ *h_env) {
  return h_env->table_level >= 0 && tbl && tbl_mode;
}

int TableStatus::feed(html_feed_environ *parser, const std::string &str,
                      bool internal) {
  return tbl->feed_table(parser, str.c_str(), tbl_mode, tbl_width, internal);
}
