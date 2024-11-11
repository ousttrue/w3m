#pragma once

extern bool confirm_on_quit;

#define DEFUN(funcname, macroname, docstring)                                  \
  void funcname(struct Current current)

const char *searchKeyData();
void w3m_exit(int i);
