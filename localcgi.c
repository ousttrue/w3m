#include "localcgi.h"
#include "fm.h"
#include <stdio.h>

#ifdef _WIN32
#include "rand48_win32.h"
#endif

static char *Local_cookie_file = NULL;
static Str Local_cookie = NULL;

static void writeLocalCookie() {
  if (no_rc_dir)
    return;

  if (Local_cookie_file)
    return;

  Local_cookie_file = tmpfname(TMPF_COOKIE, NULL)->ptr;
  set_environ("LOCAL_COOKIE_FILE", Local_cookie_file);
  auto f = fopen(Local_cookie_file, "wb");
  if (!f)
    return;
  localCookie();
  fwrite(Local_cookie->ptr, sizeof(char), Local_cookie->length, f);
  fclose(f);
  chmod(Local_cookie_file, S_IRUSR | S_IWUSR);
}

/* setup cookie for local CGI */
Str localCookie() {
  if (Local_cookie)
    return Local_cookie;
  srand48((long)New(char) + (long)time(NULL));
  Local_cookie =
      Sprintf("%ld@%s", lrand48(), HostName ? HostName : "localhost");
  return Local_cookie;
}
