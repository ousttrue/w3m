#pragma once
#include "line.h"
#include "line_layout.h"
#include <memory>
#include <array>

/* Link const Buffer */
enum LinkBuffer {
  LB_FRAME = 0, /* rFrame() */
  LB_N_FRAME = 1,
  LB_INFO = 2 /* pginfo() */,
  LB_N_INFO = 3,
  LB_SOURCE = 4 /* vwSrc() */,
  // LB_N_SOURCE = LB_SOURCE,
  MAX_LB = 5,
  LB_NOLINK = -1,
};

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

struct Clone {
  int count = 1;
};

struct Buffer : public gc_cleanup {
  std::shared_ptr<HttpResponse> info;
  LineLayout layout = {};
  std::shared_ptr<Buffer> backBuffer;
  std::array<std::shared_ptr<Buffer>, MAX_LB> linkBuffer = {0};
  std::shared_ptr<Clone> clone;
  bool check_url = false;

private:
  Buffer(int width);

public:
  ~Buffer();
  Buffer(const Buffer &src) { *this = src; }
  static std::shared_ptr<Buffer> create(int width) {
    return std::shared_ptr<Buffer>(new Buffer(width));
  }

  // shallow copy
  Buffer &operator=(const Buffer &src);
};

void _followForm(int submit);
void set_buffer_environ(const std::shared_ptr<Buffer> &buf);
char *GetWord(const std::shared_ptr<Buffer> &buf);
char *getCurWord(const std::shared_ptr<Buffer> &buf, int *spos, int *epos);

inline void nextChar(int &s, Line *l) { s++; }
inline void prevChar(int &s, Line *l) { s--; }
// #define getChar(p) ((int)*(p))

extern void discardBuffer(const std::shared_ptr<Buffer> &buf);
std::shared_ptr<Buffer> page_info_panel(const std::shared_ptr<Buffer> &buf);

std::shared_ptr<Buffer> nullBuffer(void);
std::shared_ptr<Buffer> nthBuffer(const std::shared_ptr<Buffer> &firstbuf,
                                  int n);
std::shared_ptr<Buffer> selectBuffer(const std::shared_ptr<Buffer> &firstbuf,
                                     std::shared_ptr<Buffer> currentbuf,
                                     char *selectchar);
extern void reshapeBuffer(const std::shared_ptr<Buffer> &buf);
std::shared_ptr<Buffer> forwardBuffer(const std::shared_ptr<Buffer> &first,
                                      const std::shared_ptr<Buffer> &buf);

extern void chkURLBuffer(const std::shared_ptr<Buffer> &buf);
void isrch(int (*func)(LineLayout *, const char *), const char *prompt);
void srch(int (*func)(LineLayout *, const char *), const char *prompt);
void srch_nxtprv(int reverse);
void shiftvisualpos(const std::shared_ptr<Buffer> &buf, int shift);
void execdict(const char *word);
void invoke_browser(const char *url);
void _peekURL(int only_img);
int checkBackBuffer(const std::shared_ptr<Buffer> &buf);

void _prevA(int visited);
void _nextA(int visited);
int cur_real_linenumber(const std::shared_ptr<Buffer> &buf);
void _movL(int n);
void _movD(int n);
void _movU(int n);
void _movR(int n);
int prev_nonnull_line(Line *line);
int next_nonnull_line(Line *line);
void _goLine(const char *l);
void query_from_followform(Str **query, FormItemList *fi, int multipart);

struct Str;
Str *Str_form_quote(Str *x);

void saveBuffer(const std::shared_ptr<Buffer> &buf, FILE *f, int cont);

void cmd_loadBuffer(const std::shared_ptr<Buffer> &buf, int linkid);
void cmd_loadfile(const char *fn);

std::shared_ptr<Buffer> link_list_panel(const std::shared_ptr<Buffer> &buf);
