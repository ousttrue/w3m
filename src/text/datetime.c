#include "datetime.h"
#include "Str.h"
#include "myctype.h"
#include <stdlib.h>

static char *monthtbl[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static int get_day(const char **s) {
  if (!**s)
    return -1;
  auto ss = *s;

  Str tmp = Strnew();
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));

  auto day = atoi(tmp->ptr);
  if (day < 1 || day > 31) {
    *s = ss;
    return -1;
  }

  return day;
}

static int get_month(const char **s) {
  if (!**s)
    return -1;
  auto ss = *s;

  Str tmp = Strnew();
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));

  int mon;
  if (tmp->length > 0) {
    mon = atoi(tmp->ptr);
  } else {
    while (**s && IS_ALPHA(**s))
      Strcat_char(tmp, *((*s)++));
    for (mon = 1; mon <= 12; mon++) {
      if (strncmp(tmp->ptr, monthtbl[mon - 1], 3) == 0)
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
  auto ss = *s;

  Str tmp = Strnew();
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));
  if (tmp->length != 2 && tmp->length != 4) {
    *s = ss;
    return -1;
  }

  int year = atoi(tmp->ptr);
  if (tmp->length == 2) {
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
  auto ss = *s;

  Str tmp = Strnew();
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));
  if (**s != ':') {
    *s = ss;
    return -1;
  }
  *hour = atoi(tmp->ptr);

  (*s)++;
  Strclear(tmp);
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));
  if (**s != ':') {
    *s = ss;
    return -1;
  }
  *min = atoi(tmp->ptr);

  (*s)++;
  Strclear(tmp);
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));
  *sec = atoi(tmp->ptr);

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
  auto ss = *s;

  Str tmp = Strnew();
  if (**s == '+' || **s == '-')
    Strcat_char(tmp, *((*s)++));
  while (**s && IS_DIGIT(**s))
    Strcat_char(tmp, *((*s)++));
  if (!(tmp->length == 4 && IS_DIGIT(*ss)) &&
      !(tmp->length == 5 && (*ss == '+' || *ss == '-'))) {
    *s = ss;
    return -1;
  }

  int zone = atoi(tmp->ptr);
  *z_hour = zone / 100;
  *z_min = zone - (zone / 100) * 100;
  return 0;
}

/* RFC 1123 or RFC 850 or ANSI C asctime() format string -> time_t */
time_t mymktime(const char *timestr) {
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

  int day, mon, year, hour, min, sec, z_hour = 0, z_min = 0;
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
