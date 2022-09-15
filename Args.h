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
  int load_argc = 0;

  int parse(int argc, char **argv, char **load_argv);
};
