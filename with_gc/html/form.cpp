#include "form.h"
#include "form_item.h"
#include "tmpfile.h"
#include "quote.h"
#include "app.h"
#include "etc.h"
#include "alloc.h"
#include "html_tag.h"
#include "html_feed_env.h"
#include <sys/stat.h>

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

static void form_write_data(FILE *f, const char *boundary, const char *name,
                            const char *value) {
  fprintf(f, "--%s\r\n", boundary);
  fprintf(f, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", name);
  fprintf(f, "%s\r\n", value);
}

static void form_write_from_file(FILE *f, const char *boundary,
                                 const char *name, const char *filename,
                                 const char *file) {
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

std::string Form::query(const std::shared_ptr<FormItem> &item) const {
  std::stringstream query;
  for (auto &f2 : this->items) {
    if (f2->name.empty())
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2 != item || f2->value.empty())
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
        query << form_quote(f2->name);
        query << Sprintf(".x=%d&", x);
        query << form_quote(f2->name);
        query << Sprintf(".y=%d", y);
      } else {
        /* not IMAGE */
        if (f2->name.size()) {
          query << form_quote(f2->name);
          query << '=';
        }
        if (f2->value.size()) {
          query << form_quote(f2->value);
        }
      }
      query << '&';
    }
  }
  auto str = query.str();
  {
    /* remove trailing & */
    while (str.size() && str.back() == '&')
      str.pop_back();
  }
  return str;
}

void FormItem::query_from_followform_multipart() {
  auto tmpf = Strnew(TmpFile::instance().tmpfname(TMPF_DFL, {}));
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
    if (f2->name.empty())
      continue;
    switch (f2->type) {
    case FORM_INPUT_RESET:
      /* do nothing */
      continue;
    case FORM_INPUT_SUBMIT:
    case FORM_INPUT_IMAGE:
      if (f2.get() != this || f2->value.empty())
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
          auto query = f2->name;
          query += ".x";
          form_write_data(body, form->boundary, query.c_str(),
                          Sprintf("%d", x)->ptr);
        }
        {
          auto query = f2->name;
          query += ".y";
          form_write_data(body, form->boundary, query.c_str(),
                          Sprintf("%d", y)->ptr);
        }
      } else if (f2->name.size() && f2->value.size()) {
        /* not IMAGE */
        {
          auto query = f2->value;
          if (f2->type == FORM_INPUT_FILE)
            form_write_from_file(body, form->boundary, f2->name.c_str(),
                                 query.c_str(), f2->value.c_str());
          else
            form_write_data(body, form->boundary, f2->name.c_str(),
                            query.c_str());
        }
      }
    }
  }
  {
    fprintf(body, "--%s--\r\n", form->boundary);
    fclose(body);
  }
}

std::string form_quote(std::string_view x) {
  std::stringstream tmp;
  auto p = x.begin();
  auto ep = x.end();
  for (; p < ep; p++) {
    if (*p == ' ') {
      tmp << '+';
    } else if (is_url_unsafe(*p)) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)*p);
      tmp << buf;
    } else {
      tmp << *p;
    }
  }
  return tmp.str();
}
