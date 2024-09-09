#pragma once

int tgetent(char *, const char *);
int tgetnum(const char *);
int tgetflag(const char *);
char *tgetstr(const char *, char **);
