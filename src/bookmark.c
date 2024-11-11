#include "bookmark.h"
#include "rc.h"

#define BOOKMARK "bookmark.html"
const char *BookmarkFile = nullptr;

void init_bookmark() {
  if (!BookmarkFile) {
    BookmarkFile = rcFile(BOOKMARK);
  }
}
