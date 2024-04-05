#include "load_buffer.h"
#include "line_layout.h"
#include "quote.h"
#include "myctype.h"
#include "http_response.h"

int squeezeBlankLine = false;

/*
 * Check character type
 */
#define LINELEN 256 /* Initial line length */

static std::string checkType(std::string s, std::vector<Lineprop> *oprop) {
  static std::vector<Lineprop> prop_buffer;

  auto str = s.data();
  auto endp = s.data() + s.size();
  const char *bs = NULL;

  if (prop_buffer.size() < s.size()) {
    prop_buffer.resize(s.size() > LINELEN ? s.size() : LINELEN);
  }
  auto prop = prop_buffer.data();

  bool do_copy = false;
  if (!do_copy) {
    for (; str < endp && IS_ASCII(*str); str++) {
      *(prop++) = PE_NORMAL | (IS_CNTRL(*str) ? PC_CTRL : PC_ASCII);
    }
  }

  Lineprop effect = PE_NORMAL;
  while (str < endp) {
    if (prop - prop_buffer.data() >= prop_buffer.size())
      break;
    if (bs != NULL) {
      if (str == bs - 1 && *str == '_') {
        str += 2;
        effect = PE_UNDER;
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      } else if (str == bs) {
        if (*(str + 1) == '_') {
          if (s.size()) {
            str += 2;
            *(prop - 1) |= PE_UNDER;
          } else {
            str++;
          }
        } else {
          if (s.size()) {
            if (*(str - 1) == *(str + 1)) {
              *(prop - 1) |= PE_BOLD;
              str += 2;
            } else {
              s.pop_back();
              prop--;
              str++;
            }
          } else {
            str++;
          }
        }
        if (str < endp)
          bs = (char *)memchr(str, '\b', endp - str);
        continue;
      }
    }

    auto mode = get_mctype(str) | effect;
    *(prop++) = mode;
    {
      if (do_copy)
        s.push_back((char)*str);
      str++;
    }
    effect = PE_NORMAL;
  }
  *oprop = prop_buffer;
  return s;
}void loadBuffer(const std::shared_ptr<LineLayout> &layout, int width,
                HttpResponse *res, std::string_view page) {
  layout->clear();
  auto nlines = 0;
  // auto stream = newStrStream(page);
  char pre_lbuf = '\0';

  auto begin = page.begin();
  while (begin != page.end()) {
    auto it = begin;
    for (; it != page.end(); ++it) {
      if (*it == '\n') {
        break;
      }
    }
    std::string_view l(begin, it);
    begin = it;
    if (begin != page.end()) {
      ++begin;
    }
    auto line = cleanup_line(l, PAGER_MODE);

    if (squeezeBlankLine) {
      if (line[0] == '\n' && pre_lbuf == '\n') {
        ++nlines;
        continue;
      }
      pre_lbuf = line[0];
    }
    ++nlines;
    auto lineBuf2 = line;
    Strchop(lineBuf2);

    std::vector<Lineprop> propBuffer;
    lineBuf2 = checkType(lineBuf2, &propBuffer);
    layout->formated->addnewline(lineBuf2.c_str(), propBuffer, lineBuf2.size());
  }
  // layout->_scroll.row = 0;
  // layout->_cursor.row = 0;
  res->type = "text/plain";
}

// std::shared_ptr<Buffer>
// loadHTMLString(const std::shared_ptr<LineLayout> &layout, int width,
//                const Url &currentUrl, std::string_view html) {
//   auto newBuf = Buffer::create();
//   newBuf->layout = loadHTMLstream(layout, width, currentUrl, html, true);
//   newBuf->res->raw = {html.begin(), html.end()};
//   newBuf->res->type = "text/html";
//   return newBuf;
// }

#define SHELLBUFFERNAME "*Shellout*"
std::shared_ptr<HttpResponse> getshell(const std::string &cmd) {
// #ifdef _MSC_VER
#if 1
  return {};
#else
  if (cmd.empty()) {
    return {};
  }

  auto f = popen(cmd, "r");
  if (f == nullptr) {
    return {};
  }

  // auto buf = Buffer::create();
  // UrlStream uf(SCM_UNKNOWN, newFileStream(f, pclose));
  // TODO:
  // loadBuffer(buf->res.get(), &buf->layout);
  // buf->res->filename = cmd;
  // buf->layout->data.title = Sprintf("%s %s", SHELLBUFFERNAME, cmd)->ptr;
  // if (res->type.empty()) {
  //   buf->res->type = "text/plain";
  // }
  return {};
#endif
}
