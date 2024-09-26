#pragma once

typedef struct _ParsedURL {
  int scheme;
  char *user;
  char *pass;
  char *host;
  int port;
  char *file;
  char *real_file;
  char *query;
  char *label;
  int is_nocache;
} ParsedURL;
