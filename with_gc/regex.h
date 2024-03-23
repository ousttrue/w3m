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
  const char *position;
  const char *lposition;
  Regex *alt_regex;
};

Regex *newRegex(const char *ex, int igncase, Regex *regex,
                const char **error_msg);

int RegexMatch(Regex *re, const char *str, int len, int firstp);

void MatchedPosition(Regex *re, const char **first, const char **last);

/* backward compatibility */
const char *regexCompile(const char *ex, int igncase);

int regexMatch(const char *str, int len, int firstp);

void matchedPosition(const char **first, const char **last);

const char *getRegexWord(const char **str, Regex **regex_ret);
