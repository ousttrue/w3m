#pragma once

int tputs(const char *, int, int (*)(char));
char *tgoto(const char *, int, int);

int tgetent(char *, const char *);
int tgetnum(const char *);
int tgetflag(char *);
char *tgetstr(char *, char **);


