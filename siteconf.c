#include "siteconf.h"
#include "rc.h"
#include "text.h"
// #include "func.h"
#include "app.h"
#include "alloc.h"
#include "Str.h"
#include "url.h"
#include "regex.h"
#include "file.h"
#include "myctype.h"

#define SITECONF_FILE RC_DIR "/siteconf"
const char *siteconf_file = SITECONF_FILE;

/* siteconf */
/*
 * url "<url>"|/<re-url>/|m@<re-url>@i [exact]
 * substitute_url "<destination-url>"
 * url_charset <charset>
 * no_referer_from on|off
 * no_referer_to on|off
 * user_agent "<string>"
 *
 * The last match wins.
 */

struct siteconf_rec {
  struct siteconf_rec *next;
  const char *url;
  Regex *re_url;
  int url_exact;
  unsigned char mask[(SCONF_N_FIELD + 7) >> 3];

  const char *substitute_url;
  const char *user_agent;
  bool no_referer_from;
  bool no_referer_to;
};
#define SCONF_TEST(ent, f) ((ent)->mask[(f) >> 3] & (1U << ((f) & 7)))
#define SCONF_SET(ent, f) ((ent)->mask[(f) >> 3] |= (1U << ((f) & 7)))
#define SCONF_CLEAR(ent, f) ((ent)->mask[(f) >> 3] &= ~(1U << ((f) & 7)))

static struct siteconf_rec *siteconf_head = NULL;
static struct siteconf_rec *newSiteconfRec(void);

static struct siteconf_rec *newSiteconfRec(void) {
  struct siteconf_rec *ent;

  ent = New(struct siteconf_rec);
  ent->next = NULL;
  ent->url = NULL;
  ent->re_url = NULL;
  ent->url_exact = false;
  memset(ent->mask, 0, sizeof(ent->mask));

  ent->substitute_url = NULL;
  ent->user_agent = NULL;
  return ent;
}

void loadSiteconf(void) {
  const char *efname;
  FILE *fp;
  Str line;
  struct siteconf_rec *ent = NULL;

  siteconf_head = NULL;
  if (!siteconf_file)
    return;
  if ((efname = expandPath(siteconf_file)) == NULL)
    return;
  fp = fopen(efname, "r");
  if (fp == NULL)
    return;
  while (line = Strfgets(fp), line->length > 0) {
    const char *p, *s;

    Strchop(line);
    p = line->ptr;
    SKIP_BLANKS(p);
    if (*p == '#' || *p == '\0')
      continue;
    s = getWord(&p);

    /* The "url" begins a new record. */
    if (strcmp(s, "url") == 0) {
      const char *url, *opt;
      struct siteconf_rec *newent;

      /* First, register the current record. */
      if (ent) {
        ent->next = siteconf_head;
        siteconf_head = ent;
        ent = NULL;
      }

      /* Second, create a new record. */
      newent = newSiteconfRec();
      url = getRegexWord(&p, &newent->re_url);
      opt = getWord(&p);
      SKIP_BLANKS(p);
      if (!newent->re_url) {
        struct Url pu;
        if (!url || !*url)
          continue;
        parseURL2(url, &pu, NULL);
        newent->url = parsedURL2Str(&pu)->ptr;
      }
      /* If we have an extra or unknown option, ignore this record
       * for future extensions. */
      if (strcmp(opt, "exact") == 0) {
        newent->url_exact = true;
      } else if (*opt != 0)
        continue;
      if (*p)
        continue;
      ent = newent;
      continue;
    }

    /* If the current record is broken, skip to the next "url". */
    if (!ent)
      continue;

    /* Fill the new record. */
    if (strcmp(s, "substitute_url") == 0) {
      ent->substitute_url = getQWord(&p);
      SCONF_SET(ent, SCONF_SUBSTITUTE_URL);
    }
    if (strcmp(s, "user_agent") == 0) {
      ent->user_agent = getQWord(&p);
      SCONF_SET(ent, SCONF_USER_AGENT);
    } else if (strcmp(s, "no_referer_from") == 0) {
      ent->no_referer_from = str_to_bool(getWord(&p), 0);
      SCONF_SET(ent, SCONF_NO_REFERER_FROM);
    } else if (strcmp(s, "no_referer_to") == 0) {
      ent->no_referer_to = str_to_bool(getWord(&p), 0);
      SCONF_SET(ent, SCONF_NO_REFERER_TO);
    }
  }
  if (ent) {
    ent->next = siteconf_head;
    siteconf_head = ent;
    ent = NULL;
  }
  fclose(fp);
}

int strmatchlen(const char *s1, const char *s2, int maxlen) {
  int i;
  /* To allow the maxlen to be negatie (infinity),
   * compare by "!=" instead of "<=". */
  for (i = 0; i != maxlen; ++i) {
    if (!s1[i] || !s2[i] || s1[i] != s2[i])
      break;
  }
  return i;
}

const void *querySiteconf(struct Url *query_pu, enum SiteConfField field) {
  if (field < 0 || field >= SCONF_N_FIELD)
    return NULL;

  if (!query_pu || IS_EMPTY_PARSED_URL(query_pu))
    return NULL;

  auto u = parsedURL2Str(query_pu);
  if (u->length == 0)
    return NULL;

  auto ent = siteconf_head;
  const char *firstp, *lastp;
  for (; ent; ent = ent->next) {
    if (!SCONF_TEST(ent, field))
      continue;
    if (ent->re_url) {
      if (RegexMatch(ent->re_url, u->ptr, u->length, 1)) {
        MatchedPosition(ent->re_url, &firstp, &lastp);
        if (!ent->url_exact)
          goto url_found;
        if (firstp != u->ptr || lastp == firstp)
          continue;
        if (*lastp == 0 || *lastp == '?' || *(lastp - 1) == '?' ||
            *lastp == '#' || *(lastp - 1) == '#')
          goto url_found;
      }
    } else {
      int matchlen = strmatchlen(ent->url, u->ptr, u->length);
      if (matchlen == 0 || ent->url[matchlen] != 0)
        continue;
      firstp = u->ptr;
      lastp = u->ptr + matchlen;
      if (*lastp == 0 || *lastp == '?' || *(lastp - 1) == '?' ||
          *lastp == '#' || *(lastp - 1) == '#')
        goto url_found;
      if (!ent->url_exact && (*lastp == '/' || *(lastp - 1) == '/'))
        goto url_found;
    }
  }
  return NULL;

url_found:
  switch (field) {
  case SCONF_SUBSTITUTE_URL:
    if (ent->substitute_url && *ent->substitute_url) {
      Str tmp = Strnew_charp_n(u->ptr, firstp - u->ptr);
      Strcat_charp(tmp, ent->substitute_url);
      Strcat_charp(tmp, lastp);
      return tmp->ptr;
    }
    return NULL;
  case SCONF_USER_AGENT:
    if (ent->user_agent && *ent->user_agent) {
      return ent->user_agent;
    }
    return NULL;
  case SCONF_NO_REFERER_FROM:
    return &ent->no_referer_from;
  case SCONF_NO_REFERER_TO:
    return &ent->no_referer_to;
  }
  return NULL;
}
