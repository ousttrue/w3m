#include "form.h"
#include "tmpfile.h"
#include "quote.h"
#include "form.h"
#include "cmp.h"
#include "tmpfile.h"

#include <sys/stat.h>
#include <assert.h>
#include <sstream>

static const char *_formtypetbl[] = {
    "text",  "password", "checkbox", "radio",  "submit", "reset", "hidden",
    "image", "select",   "textarea", "button", "file",   NULL};

static const char *_formmethodtbl[] = {"GET", "POST", "INTERNAL", "HEAD"};

FormItemType formtype(const std::string &typestr) {
  int i;
  for (i = 0; _formtypetbl[i]; i++) {
    if (!strcasecmp(typestr, _formtypetbl[i]))
      return (FormItemType)i;
  }
  return FORM_INPUT_TEXT;
}

std::string FormItem::form2str() const {
  std::stringstream tmp;
  if (this->type != FORM_SELECT && this->type != FORM_TEXTAREA)
    tmp << "input type=";
  tmp << _formtypetbl[this->type];
  if (this->name.size())
    tmp << " name=\"" << this->name << "\"";
  if ((this->type == FORM_INPUT_RADIO || this->type == FORM_INPUT_CHECKBOX ||
       this->type == FORM_SELECT) &&
      this->value.size())
    tmp << " value=\"" << this->value << "\"";
  tmp << " (" << _formmethodtbl[this->parent->method] << " "
      << this->parent->action << ")";
  return tmp.str();
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
          type.size() ? type.c_str() : "application/octet-stream");

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
void FormItem::query_from_followform_multipart(int pid) {
  auto tmpf = TmpFile::instance().tmpfname(TMPF_DFL, {});
  auto body = fopen(tmpf.c_str(), "w");
  if (body == nullptr) {
    return;
  }
  auto form = this->parent;
  form->body = tmpf;

  std::stringstream boundary;
  boundary << "------------------------------" << pid << form.get()
           << form->body << form->boundary;
  form->boundary = boundary.str();
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
          std::stringstream ss;
          ss << x;
          form_write_data(body, form->boundary.c_str(), query.c_str(),
                          ss.str().c_str());
        }
        {
          auto query = f2->name;
          query += ".y";
          std::stringstream ss;
          ss << y;
          form_write_data(body, form->boundary.c_str(), query.c_str(),
                          ss.str().c_str());
        }
      } else if (f2->name.size() && f2->value.size()) {
        /* not IMAGE */
        {
          auto query = f2->value;
          if (f2->type == FORM_INPUT_FILE)
            form_write_from_file(body, form->boundary.c_str(), f2->name.c_str(),
                                 query.c_str(), f2->value.c_str());
          else
            form_write_data(body, form->boundary.c_str(), f2->name.c_str(),
                            query.c_str());
        }
      }
    }
  }
  {
    fprintf(body, "--%s--\r\n", form->boundary.c_str());
    fclose(body);
  }
}
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
        query << ".x=" << x << "&";
        query << form_quote(f2->name);
        query << ".y=" << y;
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
