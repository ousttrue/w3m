#include "mytime.h"
#include "myctype.h"
#include <stdlib.h>
#include <string.h>
#include <sstream>

static const char *monthtbl[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int get_day(const char **s) {
  if (!**s)
    return -1;

  std::stringstream tmp;
  const char *ss = *s;
  while (**s && IS_DIGIT(**s))
    tmp << *((*s)++);

  int day = stoi(tmp.str());
  if (day < 1 || day > 31) {
    *s = ss;
    return -1;
  }
  return day;
}

static int get_month(const char **s) {
  if (!**s)
    return -1;

  const char *ss = *s;
  std::string tmp;
  while (**s && IS_DIGIT(**s))
    tmp += *((*s)++);

  int mon;
  if (tmp.size() > 0) {
    mon = stoi(tmp);
  } else {
    while (**s && IS_ALPHA(**s))
      tmp += *((*s)++);
    for (mon = 1; mon <= 12; mon++) {
      if (strncmp(tmp.c_str(), monthtbl[mon - 1], 3) == 0)
        break;
    }
  }
  if (mon < 1 || mon > 12) {
    *s = ss;
    return -1;
  }
  return mon;
}

static int get_year(const char **s) {
  if (!**s)
    return -1;

  std::string tmp;
  const char *ss = *s;

  while (**s && IS_DIGIT(**s))
    tmp += *((*s)++);
  if (tmp.size() != 2 && tmp.size() != 4) {
    *s = ss;
    return -1;
  }

  int year = stoi(tmp);
  if (tmp.size() == 2) {
    if (year >= 70)
      year += 1900;
    else
      year += 2000;
  }
  return year;
}

static int get_time(const char **s, int *hour, int *min, int *sec) {
  if (!**s)
    return -1;

  std::string tmp;
  const char *ss = *s;
  while (**s && IS_DIGIT(**s))
    tmp += *((*s)++);
  if (**s != ':') {
    *s = ss;
    return -1;
  }
  *hour = stoi(tmp);

  (*s)++;
  tmp.clear();
  while (**s && IS_DIGIT(**s))
    tmp += *((*s)++);
  if (**s != ':') {
    *s = ss;
    return -1;
  }
  *min = stoi(tmp);

  (*s)++;
  tmp.clear();
  while (**s && IS_DIGIT(**s))
    tmp += *((*s)++);
  *sec = stoi(tmp);

  if (*hour < 0 || *hour >= 24 || *min < 0 || *min >= 60 || *sec < 0 ||
      *sec >= 60) {
    *s = ss;
    return -1;
  }
  return 0;
}

static int get_zone(const char **s, int *z_hour, int *z_min) {
  if (!**s)
    return -1;

  std::string tmp;
  const char *ss = *s;

  if (**s == '+' || **s == '-')
    tmp += *((*s)++);
  while (**s && IS_DIGIT(**s))
    tmp += *((*s)++);
  if (!(tmp.size() == 4 && IS_DIGIT(*ss)) &&
      !(tmp.size() == 5 && (*ss == '+' || *ss == '-'))) {
    *s = ss;
    return -1;
  }

  int zone = stoi(tmp);
  *z_hour = zone / 100;
  *z_min = zone - (zone / 100) * 100;
  return 0;
}

/* RFC 1123 or RFC 850 or ANSI C asctime() format string -> time_t */
time_t mymktime(const char *timestr) {
  int day, mon, year, hour, min, sec, z_hour = 0, z_min = 0;

  if (!(timestr && *timestr))
    return -1;
  auto s = timestr;

#ifdef DEBUG
  fprintf(stderr, "mktime: %s\n", timestr);
#endif /* DEBUG */

  while (*s && IS_ALPHA(*s))
    s++;
  while (*s && !IS_ALNUM(*s))
    s++;

  if (IS_DIGIT(*s)) {
    /* RFC 1123 or RFC 850 format */
    if ((day = get_day(&s)) == -1)
      return -1;

    while (*s && !IS_ALNUM(*s))
      s++;
    if ((mon = get_month(&s)) == -1)
      return -1;

    while (*s && !IS_DIGIT(*s))
      s++;
    if ((year = get_year(&s)) == -1)
      return -1;

    while (*s && !IS_DIGIT(*s))
      s++;
    if (!*s) {
      hour = 0;
      min = 0;
      sec = 0;
    } else {
      if (get_time(&s, &hour, &min, &sec) == -1)
        return -1;
      while (*s && !IS_DIGIT(*s) && *s != '+' && *s != '-')
        s++;
      get_zone(&s, &z_hour, &z_min);
    }
  } else {
    /* ANSI C asctime() format. */
    while (*s && !IS_ALNUM(*s))
      s++;
    if ((mon = get_month(&s)) == -1)
      return -1;

    while (*s && !IS_DIGIT(*s))
      s++;
    if ((day = get_day(&s)) == -1)
      return -1;

    while (*s && !IS_DIGIT(*s))
      s++;
    if (get_time(&s, &hour, &min, &sec) == -1)
      return -1;

    while (*s && !IS_DIGIT(*s))
      s++;
    if ((year = get_year(&s)) == -1)
      return -1;
  }
#ifdef DEBUG
  fprintf(stderr, "year=%d month=%d day=%d hour:min:sec=%d:%d:%d zone=%d:%d\n",
          year, mon, day, hour, min, sec, z_hour, z_min);
#endif /* DEBUG */

  mon -= 3;
  if (mon < 0) {
    mon += 12;
    year--;
  }
  day += (year - 1968) * 1461 / 4;
  day += ((((mon * 153) + 2) / 5) - 672);
  hour -= z_hour;
  min -= z_min;
  return (time_t)((day * 60 * 60 * 24) + (hour * 60 * 60) + (min * 60) + sec);
}
