#include "filepath.h"
#include "alloc.h"
#include <string.h>
#include <sys/stat.h>

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)

const char *lastFileName(const char *path) {
  const char *p, *q;

  p = q = path;
  while (*p != '\0') {
    if (*p == '/')
      q = p + 1;
    p++;
  }

  return allocStr(q, -1);
}

const char *mybasename(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  while (s <= p && *p != '/')
    p--;
  if (*p == '/')
    p++;
  else
    p = s;
  return allocStr(p, -1);
}

const char *mydirname(const char *s) {
  const char *p = s;
  while (*p)
    p++;
  if (s != p)
    p--;
  while (s != p && *p == '/')
    p--;
  while (s != p && *p != '/')
    p--;
  if (*p != '/')
    return ".";
  while (s != p && *p == '/')
    p--;
  return allocStr(s, strlen(s) - strlen(p) + 1);
}

#define DEF_SAVE_FILE "index.html"

char *guess_filename(const char *file) {
  char *p = NULL;
  if (file != NULL)
    p = allocStr(mybasename(file), -1);
  if (p == NULL || *p == '\0')
    return DEF_SAVE_FILE;

  auto s = p;
  if (*p == '#')
    p++;
  while (*p != '\0') {
    if ((*p == '#' && *(p + 1) != '\0') || *p == '?') {
      *p = '\0';
      break;
    }
    p++;
  }
  return s;
}

bool dir_exist(const char *path) {
  struct stat stbuf;

  if (path == NULL || *path == '\0')
    return false;
  if (stat(path, &stbuf) == -1)
    return false;

  return IS_DIRECTORY(stbuf.st_mode);
}
