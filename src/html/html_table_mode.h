#pragma once
#include "html_readbuffer_flags.h"
#include "html_command.h"

struct table_mode {
  ReadBufferFlags pre_mode;
  char indent_level;
  char caption;
  short nobr_offset;
  char nobr_level;
  short anchor_offset;
  HtmlCommand end_tag;
};
