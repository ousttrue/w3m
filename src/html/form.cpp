/*
 * HTML forms
 */
#include "form.h"
#include "quote.h"
#include "html_parser.h"
#include "form_item.h"
#include "app.h"
#include "http_response.h"
#include "auth_pass.h"
#include "w3m.h"
#include "etc.h"
#include "mimetypes.h"
#include "readbuffer.h"
#include "alloc.h"
#include "html.h"
#include "utf8.h"
#include "line.h"
#include "buffer.h"
#include "keyvalue.h"
#include "html_tag.h"
#include "myctype.h"
#include "local_cgi.h"
#include "regex.h"
#include "proto.h"
#include <sys/stat.h>

/* *INDENT-OFF* */
struct {
  const char *action;
  void (*rout)(struct keyvalue *);
} internal_action[] = {
    {"option", panel_set_option},
    {"cookie", set_cookie_flag},
    {"download", download_action},
    {"none", NULL},
    {NULL, NULL},
};
/* *INDENT-ON* */

struct FormList *newFormList(const char *action, const char *method,
                             const char *charset, const char *enctype,
                             const char *target, const char *name,
                             FormList *_next) {
  FormList *l;
  Str *a = Strnew_charp(action);
  int m = FORM_METHOD_GET;
  int e = FORM_ENCTYPE_URLENCODED;

  if (method == NULL || !strcasecmp(method, "get"))
    m = FORM_METHOD_GET;
  else if (!strcasecmp(method, "post"))
    m = FORM_METHOD_POST;
  else if (!strcasecmp(method, "internal"))
    m = FORM_METHOD_INTERNAL;
  /* unknown method is regarded as 'get' */

  if (m != FORM_METHOD_GET && enctype != NULL &&
      !strcasecmp(enctype, "multipart/form-data")) {
    e = FORM_ENCTYPE_MULTIPART;
  }

  l = (FormList *)New(FormList);
  l->item = l->lastitem = NULL;
  l->action = a;
  l->method = m;
  l->enctype = e;
  l->target = target;
  l->name = name;
  l->next = _next;
  l->nitems = 0;
  l->body = NULL;
  l->length = 0;
  return l;
}

/*
 * add <input> element to FormList
 */
FormItemList *FormList ::formList_addInput(HtmlParser *parser,
                                           struct HtmlTag *tag) {
  /* if not in <form>..</form> environment, just ignore <input> tag */
  // if (fl == NULL)
  //   return NULL;

  auto item = (FormItemList *)New(FormItemList);
  item->type = FORM_UNKNOWN;
  item->size = -1;
  item->rows = 0;
  item->checked = item->init_checked = 0;
  item->accept = 0;
  item->name = NULL;
  item->value = item->init_value = NULL;
  item->readonly = 0;
  char *p;
  if (tag->parsedtag_get_value(ATTR_TYPE, &p)) {
    item->type = formtype(p);
    if (item->size < 0 &&
        (item->type == FORM_INPUT_TEXT || item->type == FORM_INPUT_FILE ||
         item->type == FORM_INPUT_PASSWORD))
      item->size = FORM_I_TEXT_DEFAULT_SIZE;
  }
  if (tag->parsedtag_get_value(ATTR_NAME, &p))
    item->name = Strnew_charp(p);
  if (tag->parsedtag_get_value(ATTR_VALUE, &p))
    item->value = item->init_value = Strnew_charp(p);
  item->checked = item->init_checked = tag->parsedtag_exists(ATTR_CHECKED);
  item->accept = tag->parsedtag_exists(ATTR_ACCEPT);
  tag->parsedtag_get_value(ATTR_SIZE, &item->size);
  tag->parsedtag_get_value(ATTR_MAXLENGTH, &item->maxlength);
  item->readonly = tag->parsedtag_exists(ATTR_READONLY);
  int i;
  if (tag->parsedtag_get_value(ATTR_TEXTAREANUMBER, &i) && i >= 0 &&
      i < parser->textarea_str.size()) {
    item->value = item->init_value = Strnew(parser->textarea_str[i]);
  }
  if (tag->parsedtag_get_value(ATTR_ROWS, &p))
    item->rows = atoi(p);
  if (item->type == FORM_UNKNOWN) {
    /* type attribute is missing. Ignore the tag. */
    return NULL;
  }
  if (item->type == FORM_INPUT_FILE && item->value && item->value->length) {
    /* security hole ! */
    return NULL;
  }
  item->parent = this;
  item->next = NULL;
  if (!this->item) {
    this->item = this->lastitem = item;
  } else {
    this->lastitem->next = item;
    this->lastitem = item;
  }
  if (item->type == FORM_INPUT_HIDDEN) {
    return NULL;
  }
  this->nitems++;
  return item;
}

Str *textfieldrep(Str *s, int width) {
  Lineprop c_type;
  Str *n = Strnew_size(width + 2);
  int i, j, k, c_len;

  j = 0;
  for (i = 0; i < s->length; i += c_len) {
    c_type = get_mctype(&s->ptr[i]);
    c_len = get_mclen(&s->ptr[i]);
    if (s->ptr[i] == '\r')
      continue;
    k = j + get_mcwidth(&s->ptr[i]);
    if (k > width)
      break;
    if (c_type == PC_CTRL)
      Strcat_char(n, ' ');
    else if (s->ptr[i] == '&')
      Strcat_charp(n, "&amp;");
    else if (s->ptr[i] == '<')
      Strcat_charp(n, "&lt;");
    else if (s->ptr[i] == '>')
      Strcat_charp(n, "&gt;");
    else
      Strcat_charp_n(n, &s->ptr[i], c_len);
    j = k;
  }
  for (; j < width; j++)
    Strcat_char(n, ' ');
  return n;
}

static void form_fputs_decode(Str *s, FILE *f) {
  char *p;
  Str *z = Strnew();

  for (p = s->ptr; *p;) {
    switch (*p) {

    case '\r':
      if (*(p + 1) == '\n')
        p++;
    default:
      Strcat_char(z, *p);
      p++;
      break;
    }
  }
  Strfputs(z, f);
}

void input_textarea(FormItemList *fi) {
  auto tmpf = App::instance().tmpfname(TMPF_DFL, {});
  auto f = fopen(tmpf.c_str(), "w");
  if (f == NULL) {
    App::instance().disp_err_message("Can't open temporary file");
    return;
  }
  if (fi->value) {
    form_fputs_decode(fi->value, f);
  }
  fclose(f);

  if (exec_cmd(App::instance().myEditor(tmpf.c_str(), 1).c_str())) {
    goto input_end;
  }

  if (fi->readonly) {
    goto input_end;
  }

  f = fopen(tmpf.c_str(), "r");
  if (f == NULL) {
    App::instance().disp_err_message("Can't open temporary file");
    goto input_end;
  }
  fi->value = Strnew();
  Str *tmp;
  while (tmp = Strfgets(f), tmp->length > 0) {
    if (tmp->length == 1 && tmp->ptr[tmp->length - 1] == '\n') {
      /* null line with bare LF */
      tmp = Strnew_charp("\r\n");
    } else if (tmp->length > 1 && tmp->ptr[tmp->length - 1] == '\n' &&
               tmp->ptr[tmp->length - 2] != '\r') {
      Strshrink(tmp, 1);
      Strcat_charp(tmp, "\r\n");
    }
    cleanup_line(tmp, RAW_MODE);
    Strcat(fi->value, tmp);
  }
  fclose(f);
input_end:
  unlink(tmpf.c_str());
}

void do_internal(char *action, char *data) {
  for (int i = 0; internal_action[i].action; i++) {
    if (strcasecmp(internal_action[i].action, action) == 0) {
      if (internal_action[i].rout)
        internal_action[i].rout(cgistr2tagarg(data));
      return;
    }
  }
}

void form_write_data(FILE *f, char *boundary, char *name, char *value) {
  fprintf(f, "--%s\r\n", boundary);
  fprintf(f, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", name);
  fprintf(f, "%s\r\n", value);
}

void form_write_from_file(FILE *f, char *boundary, char *name, char *filename,
                          char *file) {
#ifdef _MSC_VER
#else
  FILE *fd;
  struct stat st;
  int c;

  fprintf(f, "--%s\r\n", boundary);
  fprintf(f, "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
          name, mybasename(filename));
  auto type = guessContentType(file);
  fprintf(f, "Content-Type: %s\r\n\r\n",
          type ? type : "application/octet-stream");

  if (lstat(file, &st) < 0)
    goto write_end;
  if (S_ISDIR(st.st_mode))
    goto write_end;
  fd = fopen(file, "r");
  if (fd != NULL) {
    while ((c = fgetc(fd)) != EOF)
      fputc(c, f);
    fclose(fd);
  }
write_end:
  fprintf(f, "\r\n");
#endif
}

Str *FormItemList::query_from_followform() {
  auto query = Strnew();
  for (auto f2 = this->parent->item; f2; f2 = f2->next) {
    if (f2->name == nullptr)
      continue;
    /* <ISINDEX> is translated into single text form */
    if (f2->name->length == 0 && f2->type != FORM_INPUT_TEXT)
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != this || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;
    }
    {
      /* not multipart */
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        Strcat(query, Str_form_quote(f2->name));
        Strcat(query, Sprintf(".x=%d&", x));
        Strcat(query, Str_form_quote(f2->name));
        Strcat(query, Sprintf(".y=%d", y));
      } else {
        /* not IMAGE */
        if (f2->name && f2->name->length > 0) {
          Strcat(query, Str_form_quote(f2->name));
          Strcat_char(query, '=');
        }
        if (f2->value != nullptr) {
          if (this->parent->method == FORM_METHOD_INTERNAL)
            Strcat(query, Str_form_quote(f2->value));
          else {
            Strcat(query, Str_form_quote(f2->value));
          }
        }
      }
      if (f2->next)
        Strcat_char(query, '&');
    }
  }
  {
    /* remove trailing & */
    while (Strlastchar(query) == '&')
      Strshrink(query, 1);
  }
  return query;
}

void FormItemList ::query_from_followform_multipart() {
  auto tmpf = Strnew(App::instance().tmpfname(TMPF_DFL, {}));
  auto body = fopen(tmpf->ptr, "w");
  if (body == nullptr) {
    return;
  }
  this->parent->body = tmpf->ptr;
  this->parent->boundary = Sprintf("------------------------------%d%ld%ld%ld",
                                   App::instance().pid(), this->parent,
                                   this->parent->body, this->parent->boundary)
                               ->ptr;

  // auto query = Strnew();
  for (auto f2 = this->parent->item; f2; f2 = f2->next) {
    if (f2->name == nullptr)
      continue;
    /* <ISINDEX> is translated into single text form */
    if (f2->name->length == 0)
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != this || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;
    }
    {
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        {
          auto query = f2->name->Strdup();
          Strcat_charp(query, ".x");
          form_write_data(body, this->parent->boundary, query->ptr,
                          Sprintf("%d", x)->ptr);
        }
        {
          auto query = f2->name->Strdup();
          Strcat_charp(query, ".y");
          form_write_data(body, this->parent->boundary, query->ptr,
                          Sprintf("%d", y)->ptr);
        }
      } else if (f2->name && f2->name->length > 0 && f2->value != nullptr) {
        /* not IMAGE */
        {
          auto query = f2->value;
          if (f2->type == FORM_INPUT_FILE)
            form_write_from_file(body, this->parent->boundary, f2->name->ptr,
                                 query->ptr, f2->value->ptr);
          else
            form_write_data(body, this->parent->boundary, f2->name->ptr,
                            query->ptr);
        }
      }
    }
  }
  {
    fprintf(body, "--%s--\r\n", this->parent->boundary);
    fclose(body);
  }
}

Str *Str_form_quote(Str *x) {
  Str *tmp = {};
  char *p = x->ptr, *ep = x->ptr + x->length;
  char buf[4];

  for (; p < ep; p++) {
    if (*p == ' ') {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      Strcat_char(tmp, '+');
    } else if (is_url_unsafe(*p)) {
      if (tmp == NULL)
        tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      Strcat_charp(tmp, buf);
    } else {
      if (tmp)
        Strcat_char(tmp, *p);
    }
  }
  if (tmp)
    return tmp;
  return x;
}
