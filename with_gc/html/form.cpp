#include "form.h"
#include "form_item.h"
#include "quote.h"
#include "app.h"
#include "etc.h"
#include "readbuffer.h"
#include "alloc.h"
#include "html_tag.h"
#include "html_feed_env.h"
#include <sys/stat.h>

#define FORM_I_TEXT_DEFAULT_SIZE 40

Form::~Form() {}

Form::Form(const std::string &action, const std::string &method,
           const std::string &enctype, const std::string &target,
           const std::string &name)
    : action(action), target(target), name(name)

{
  if (method.size()) {
    if (method == "get") {
      this->method = FORM_METHOD_GET;
    } else if (method == "post") {
      this->method = FORM_METHOD_POST;
    } else if (method == "internal") {
      this->method = FORM_METHOD_INTERNAL;
    } else {
      assert(false);
    }
  }
  if (this->method != FORM_METHOD_GET && enctype == "multipart/form-data") {
    this->enctype = FORM_ENCTYPE_MULTIPART;
  }
}

/*
 * add <input> element to FormList
 */
std::shared_ptr<FormItem> Form::formList_addInput(html_feed_environ *h_env,
                                                  struct HtmlTag *tag) {
  /* if not in <form>..</form> environment, just ignore <input> tag */
  // if (fl == NULL)
  //   return NULL;

  auto item = std::make_shared<FormItem>();
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
      i < (int)h_env->parser.textarea_str.size()) {
    item->value = item->init_value = Strnew(h_env->parser.textarea_str[i]);
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
  item->parent = shared_from_this();
  item->next = NULL;
  if (item->type == FORM_INPUT_HIDDEN) {
    return NULL;
  }
  this->items.push_back(item);
  return item;
}

static void form_write_data(FILE *f, char *boundary, char *name, char *value) {
  fprintf(f, "--%s\r\n", boundary);
  fprintf(f, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", name);
  fprintf(f, "%s\r\n", value);
}

static void form_write_from_file(FILE *f, char *boundary, char *name,
                                 char *filename, char *file) {
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

Str *FormItem::query_from_followform() {
  auto query = Strnew();
  for (auto &f2 : this->parent->items) {
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
      if (f2.get() != this || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;

    default:
      break;
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

void FormItem::query_from_followform_multipart() {
  auto tmpf = Strnew(App::instance().tmpfname(TMPF_DFL, {}));
  auto body = fopen(tmpf->ptr, "w");
  if (body == nullptr) {
    return;
  }
  auto form = this->parent;
  form->body = tmpf->ptr;
  form->boundary =
      Sprintf("------------------------------%d%ld%ld%ld",
              App::instance().pid(), form.get(), form->body, form->boundary)
          ->ptr;

  // auto query = Strnew();
  for (auto &f2 : form->items) {
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
      if (f2.get() != this || f2->value == nullptr)
        continue;
      break;
    case FORM_INPUT_RADIO:
    case FORM_INPUT_CHECKBOX:
      if (!f2->checked)
        continue;

    default:
      break;
    }
    {
      if (f2->type == FORM_INPUT_IMAGE) {
        int x = 0, y = 0;
        {
          auto query = f2->name->Strdup();
          Strcat_charp(query, ".x");
          form_write_data(body, form->boundary, query->ptr,
                          Sprintf("%d", x)->ptr);
        }
        {
          auto query = f2->name->Strdup();
          Strcat_charp(query, ".y");
          form_write_data(body, form->boundary, query->ptr,
                          Sprintf("%d", y)->ptr);
        }
      } else if (f2->name && f2->name->length > 0 && f2->value != nullptr) {
        /* not IMAGE */
        {
          auto query = f2->value;
          if (f2->type == FORM_INPUT_FILE)
            form_write_from_file(body, form->boundary, f2->name->ptr,
                                 query->ptr, f2->value->ptr);
          else
            form_write_data(body, form->boundary, f2->name->ptr, query->ptr);
        }
      }
    }
  }
  {
    fprintf(body, "--%s--\r\n", form->boundary);
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
