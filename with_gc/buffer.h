#pragma once
#include "line_layout.h"
#include "func.h"
#include "anchorlist.h"
#include <memory>

struct Form;
struct Anchor;
struct FormItem;
struct MapList;
struct LinkList;
struct AlarmEvent;
struct HttpResponse;

enum DefaultUrlStringType {
  DEFAULT_URL_EMPTY = 0,
  DEFAULT_URL_CURRENT = 1,
  DEFAULT_URL_LINK = 2,
};
extern DefaultUrlStringType DefaultURLString;

struct Buffer : std::enable_shared_from_this<Buffer> {
  std::shared_ptr<HttpResponse> res;
  std::shared_ptr<LineLayout> layout = {};
  std::shared_ptr<Buffer> backBuffer;

private:
  Buffer(const std::shared_ptr<HttpResponse> &res);

public:
  ~Buffer();
  Buffer(const Buffer &src) { *this = src; }
  Buffer &operator=(const Buffer &src) = default;

  static std::shared_ptr<Buffer>
  create(const std::shared_ptr<HttpResponse> &res = {}) {
    return std::shared_ptr<Buffer>(new Buffer(res));
  }
  static std::shared_ptr<Buffer> fromHtml(const std::string &html);

  std::shared_ptr<CoroutineState<std::tuple<Anchor *, std::shared_ptr<Buffer>>>>
  followAnchor(bool check_target = true);

  bool checkBackBuffer() const { return this->backBuffer ? true : false; }

  // shallow copy
  void saveBufferInfo();
  std::shared_ptr<Buffer> loadLink(const char *url, HttpOption option,
                                   const std::shared_ptr<Form> &request);
  std::shared_ptr<Buffer> do_submit(const std::shared_ptr<FormItem> &fi,
                                    Anchor *a);
  std::shared_ptr<Buffer> gotoLabel(std::string_view label);
  std::shared_ptr<Buffer> cmd_loadURL(const char *url,
                                      std::optional<Url> current,
                                      const HttpOption &option,
                                      const std::shared_ptr<Form> &form);
  std::shared_ptr<Buffer> goURL0(std::string_view url, const char *prompt,
                                 bool relative);
  std::shared_ptr<Buffer> sourceBuffer();
  std::shared_ptr<CoroutineState<std::shared_ptr<Buffer>>>
  followForm(FormAnchor *a, bool submit);
  std::string link_info() const;
  std::string page_info_panel() const;
  void saveBuffer(FILE *f, bool cont);
  void formRecheckRadio(FormAnchor *a);
};

std::string link_list_panel(const std::shared_ptr<Buffer> &buf);
