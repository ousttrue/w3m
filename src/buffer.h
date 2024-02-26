#pragma once
#include "line_layout.h"
#include "func.h"
#include <memory>

struct FormList;
struct Anchor;
struct AnchorList;
struct HmarkerList;
struct FormItemList;
struct MapList;
struct Line;
struct LinkList;
struct AlarmEvent;
struct HttpResponse;

struct Buffer : std::enable_shared_from_this<Buffer> {
  std::shared_ptr<HttpResponse> res;
  LineLayout layout = {};
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

  static std::shared_ptr<Buffer> nullBuffer(void) {
    auto b = Buffer::create();
    b->layout.title = "*Null*";
    return b;
  }

  std::shared_ptr<CoroutineState<std::tuple<Anchor *, std::shared_ptr<Buffer>>>>
  followAnchor(bool check_target = true);

  bool checkBackBuffer() const { return this->backBuffer ? true : false; }

  // shallow copy
  void saveBufferInfo();
  std::shared_ptr<Buffer> loadLink(const char *url, HttpOption option,
                                   FormList *request);
  std::shared_ptr<Buffer> do_submit(FormItemList *fi, Anchor *a);
  std::shared_ptr<Buffer> gotoLabel(std::string_view label);
  std::shared_ptr<Buffer> cmd_loadURL(const char *url,
                                      std::optional<Url> current,
                                      const HttpOption &option,
                                      FormList *request);
  std::shared_ptr<Buffer> goURL0(const char *url, const char *prompt,
                                 bool relative);
  std::shared_ptr<Buffer> sourceBuffer();
  std::shared_ptr<CoroutineState<std::shared_ptr<Buffer>>>
  followForm(Anchor *a, bool submit);
  std::string link_info() const;
  std::shared_ptr<Buffer> page_info_panel();
  void saveBuffer(FILE *f, bool cont);
};

// std::shared_ptr<Buffer> selectBuffer(const std::shared_ptr<Buffer>
// &firstbuf,
//                                      std::shared_ptr<Buffer> currentbuf,
//                                      char *selectchar);

std::shared_ptr<Buffer> link_list_panel(const std::shared_ptr<Buffer> &buf);
