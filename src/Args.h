#pragma once

void help_or_exit(int argc, char **argv);
void help();
void usage();

struct Args {
  char *line_str = nullptr;
  bool load_bookmark = false;
  bool visual_start = false;
  bool open_new_tab = false;
  bool search_header = false;
  char *default_type = nullptr;
  char *post_file = nullptr;
  char *load_argv[16] = {0};
  int load_argc = 0;
  int i = 0;

  void parse(int argc, char **argv);
};
