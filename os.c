#include "os.h"
#include "text.h"
#include "trap_jmp.h"
#include <stdlib.h>
#include <unwind.h>
#include <process.h>

void myExec(char *command) {
  signal_int_default();
  execl("/bin/sh", "sh", "-c", command, NULL);
  exit(127);
}

Str myExtCommand(const char *cmd, const char *arg, int redirect) {
  Str tmp = NULL;
  int set_arg = false;

  for (auto p = cmd; *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_arg) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat_charp(tmp, arg);
      set_arg = true;
      p++;
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (!set_arg) {
    if (redirect)
      tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
    else
      tmp = Strnew_m_charp(cmd, " ", arg, NULL);
  }
  return tmp;
}

Str myEditor(const char *cmd, const char *file, int line) {
  Str tmp = NULL;
  bool set_file = false;
  bool set_line = false;
  for (auto p = cmd; *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_file) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat_charp(tmp, file);
      set_file = true;
      p++;
    } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(cmd, (int)(p - cmd));
      Strcat(tmp, Sprintf("%d", line));
      set_line = true;
      p++;
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (!set_file) {
    if (tmp == NULL)
      tmp = Strnew_charp(cmd);
    if (!set_line && line > 1 && strcasestr(cmd, "vi"))
      Strcat(tmp, Sprintf(" +%d", line));
    Strcat_m_charp(tmp, " ", file, NULL);
  }
  return tmp;
}
