#pragma once
#include "line_layout.h"
#include <memory>
#include <array>

struct FormList;
struct Anchor;
struct AnchorList;
struct HmarkerList;
struct FormItemList;
struct MapList;
struct TextList;
struct Line;
struct LinkList;
struct AlarmEvent;
struct HttpResponse;

struct Buffer {
  std::shared_ptr<HttpResponse> res;
  LineLayout layout = {};
  std::shared_ptr<Buffer> backBuffer;
  bool check_url = false;

private:
  Buffer(const std::shared_ptr<HttpResponse> &res);

public:
  ~Buffer();
  Buffer(const Buffer &src) { *this = src; }
  static std::shared_ptr<Buffer>
  create(const std::shared_ptr<HttpResponse> &res = {}) {
    return std::shared_ptr<Buffer>(new Buffer(res));
  }
  // shallow copy
  Buffer &operator=(const Buffer &src) = default;
  void saveBufferInfo();
  std::shared_ptr<Buffer> loadLink(const char *url, HttpOption option,
                                   FormList *request);
  std::shared_ptr<Buffer> do_submit(FormItemList *fi, Anchor *a);
  std::shared_ptr<Buffer> gotoLabel(std::string_view label);
  std::shared_ptr<Buffer> sourceBuffer();
};

std::shared_ptr<Buffer> page_info_panel(const std::shared_ptr<Buffer> &buf);
std::shared_ptr<Buffer> nullBuffer(void);
std::shared_ptr<Buffer> nthBuffer(const std::shared_ptr<Buffer> &firstbuf,
                                  int n);
std::shared_ptr<Buffer> selectBuffer(const std::shared_ptr<Buffer> &firstbuf,
                                     std::shared_ptr<Buffer> currentbuf,
                                     char *selectchar);
void reshapeBuffer(const std::shared_ptr<Buffer> &buf);
std::shared_ptr<Buffer> forwardBuffer(const std::shared_ptr<Buffer> &first,
                                      const std::shared_ptr<Buffer> &buf);
void chkURLBuffer(const std::shared_ptr<Buffer> &buf);

void execdict(const char *word);
void invoke_browser(const char *url);
int checkBackBuffer(const std::shared_ptr<Buffer> &buf);
int cur_real_linenumber(const std::shared_ptr<Buffer> &buf);
Str *Str_form_quote(Str *x);
void saveBuffer(const std::shared_ptr<Buffer> &buf, FILE *f, int cont);
void cmd_loadfile(const char *fn);
std::shared_ptr<Buffer> link_list_panel(const std::shared_ptr<Buffer> &buf);
