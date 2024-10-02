#include "fm.h"
#include "buffer.h"
#include "http_request.h"
#include "termsize.h"
#include "terms.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_READLINK
#include <unistd.h>
#endif /* HAVE_READLINK */
#include "localcgi.h"
#include "hash.h"

void set_environ(const char *var, const char *value) {
  if (var != NULL && value != NULL) {
    setenv(var, value, 1);
  }
}




