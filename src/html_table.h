#pragma once
#include "html_table_border_mode.h"
#include <memory>
#include <string>

class html_feed_environ;
struct table {
  struct tableimpl *_impl;

private:
  table(int cols);

public:
  ~table();
  table(const table &) = delete;
  table &operator=(const table &) = delete;
  static std::shared_ptr<table> newTable(int cols);
  static std::shared_ptr<table> begin_table(HtmlTableBorderMode border,
                                            int spacing, int padding,
                                            int vspace, int cols, int width);
  int total_width() const;
  int ntable() const;
  int row() const;
  int col() const;
  int vspace() const;
  int feed_table(html_feed_environ *parser, std::string line,
                 struct table_mode *mode, int width, int internal);
  void end_table();
  void pushTable(const std::shared_ptr<table> &);
  void renderTable(html_feed_environ *parser, int max_width);

private:
  void feed_table1(html_feed_environ *parser, const std::string &line,
                   table_mode *mode, int width);
  int feed_table_tag(html_feed_environ *parser, const std::string &line,
                     table_mode *mode, int width,
                     const std::shared_ptr<class HtmlTag> &tag);
  void table_close_textarea(html_feed_environ *parser, table_mode *mode,
                            int width);
  void table_close_select(html_feed_environ *parser, table_mode *mode,
                          int width);
};

void initRenderTable(void);
