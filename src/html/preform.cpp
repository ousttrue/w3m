#include "preform.h"
#include "regex.h"
#include "myctype.h"
#include "auth_pass.h"
#include "etc.h"
#include "url.h"
#include "alloc.h"
#include "Str.h"
#include "quote.h"
#include "form_item.h"

#define PRE_FORM_FILE RC_DIR "/pre_form"
const char *pre_form_file = PRE_FORM_FILE;

static pre_form *PreForm = {};

pre_form *add_pre_form(pre_form *prev, const char *url, Regex *re_url,
                       const char *name, const char *action) {
  pre_form *_new;
  if (prev)
    _new = prev->next = (struct pre_form *)New(struct pre_form);
  else
    _new = PreForm = (struct pre_form *)New(struct pre_form);
  if (url && !re_url) {
    _new->url = Strnew(Url(url).to_Str())->ptr;
  } else {
    _new->url = url;
  }

  _new->re_url = re_url;
  _new->name = (name && *name) ? name : NULL;
  _new->action = (action && *action) ? action : NULL;
  _new->item = NULL;
  _new->next = NULL;
  return _new;
}

pre_form_item *add_pre_form_item(struct pre_form *pf,
                                 struct pre_form_item *prev, int type,
                                 const char *name, const char *value,
                                 const char *checked) {
  struct pre_form_item *_new;

  if (!pf)
    return NULL;
  if (prev)
    _new = prev->next = (struct pre_form_item *)New(struct pre_form_item);
  else
    _new = pf->item = (struct pre_form_item *)New(struct pre_form_item);
  _new->type = type;
  _new->name = name;
  _new->value = value;
  if (checked && *checked &&
      (!strcmp(checked, "0") || !strcasecmp(checked, "off") ||
       !strcasecmp(checked, "no")))
    _new->checked = 0;
  else
    _new->checked = 1;
  _new->next = NULL;
  return _new;
}

/*
 * url <url>|/<re-url>/
 * form [<name>] <action>
 * text <name> <value>
 * file <name> <value>
 * passwd <name> <value>
 * checkbox <name> <value> [<checked>]
 * radio <name> <value>
 * select <name> <value>
 * submit [<name> [<value>]]
 * image [<name> [<value>]]
 * textarea <name>
 * <value>
 * /textarea
 */
void loadPreForm(void) {
  FILE *fp;
  Str *line = NULL;
  Str *textarea = NULL;
  struct pre_form *pf = NULL;
  struct pre_form_item *pi = NULL;
  int type = -1;
  const char *name = NULL;

  PreForm = NULL;
  fp = openSecretFile(pre_form_file);
  if (fp == NULL)
    return;
  while (1) {
    const char *p, *s, *arg;
    Regex *re_arg;

    line = Strfgets(fp);
    if (line->length == 0)
      break;
    if (textarea &&
        !(!strncmp(line->ptr, "/textarea", 9) && IS_SPACE(line->ptr[9]))) {
      Strcat(textarea, line);
      continue;
    }
    Strchop(line);
    Strremovefirstspaces(line);
    p = line->ptr;
    if (*p == '#' || *p == '\0')
      continue; /* comment or empty line */
    s = getWord(&p);

    if (!strcmp(s, "url")) {
      arg = getRegexWord((const char **)&p, &re_arg);
      if (!arg || !*arg)
        continue;
      p = getQWord(&p);
      pf = add_pre_form(pf, arg, re_arg, NULL, p);
      pi = pf->item;
      continue;
    }
    if (!pf)
      continue;

    arg = getWord(&p);
    if (!strcmp(s, "form")) {
      if (!arg || !*arg)
        continue;
      s = getQWord(&p);
      p = getQWord(&p);
      if (!p || !*p) {
        p = s;
        s = NULL;
      }
      if (pf->item) {
        struct pre_form *prev = pf;
        pf = add_pre_form(prev, "", NULL, s, p);
        /* copy previous URL */
        pf->url = prev->url;
        pf->re_url = prev->re_url;
      } else {
        pf->name = s;
        pf->action = (p && *p) ? p : NULL;
      }
      pi = pf->item;
      continue;
    }
    if (!strcmp(s, "text"))
      type = FORM_INPUT_TEXT;
    else if (!strcmp(s, "file"))
      type = FORM_INPUT_FILE;
    else if (!strcmp(s, "passwd") || !strcmp(s, "password"))
      type = FORM_INPUT_PASSWORD;
    else if (!strcmp(s, "checkbox"))
      type = FORM_INPUT_CHECKBOX;
    else if (!strcmp(s, "radio"))
      type = FORM_INPUT_RADIO;
    else if (!strcmp(s, "submit"))
      type = FORM_INPUT_SUBMIT;
    else if (!strcmp(s, "image"))
      type = FORM_INPUT_IMAGE;
    else if (!strcmp(s, "select"))
      type = FORM_SELECT;
    else if (!strcmp(s, "textarea")) {
      type = FORM_TEXTAREA;
      name = Strnew_charp(arg)->ptr;
      textarea = Strnew();
      continue;
    } else if (textarea && name && !strcmp(s, "/textarea")) {
      pi = add_pre_form_item(pf, pi, type, name, textarea->ptr, NULL);
      textarea = NULL;
      name = NULL;
      continue;
    } else
      continue;
    s = getQWord(&p);
    pi = add_pre_form_item(pf, pi, type, arg, s, getQWord(&p));
  }
  fclose(fp);
}
