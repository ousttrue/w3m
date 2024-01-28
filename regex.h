/* $Id: regex.h,v 1.6 2003/09/22 21:02:21 ukai Exp $ */
#define REGEX_MAX 64
#define STORAGE_MAX 256

struct longchar {
  char type;
  unsigned char ch;
};

struct Regex;
struct regexchar {
  union {
    longchar *pattern;
    Regex *sub;
  } p;
  unsigned char mode;
};

struct Regex {
  regexchar re[REGEX_MAX];
  longchar storage[STORAGE_MAX];
  char *position;
  char *lposition;
  Regex *alt_regex;
};

Regex *newRegex(char *ex, int igncase, Regex *regex, char **error_msg);

int RegexMatch(Regex *re, char *str, int len, int firstp);

void MatchedPosition(Regex *re, char **first, char **last);

/* backward compatibility */
char *regexCompile(char *ex, int igncase);

int regexMatch(char *str, int len, int firstp);

void matchedPosition(char **first, char **last);

char *getRegexWord(const char **str, Regex **regex_ret);

