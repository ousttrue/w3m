#pragma once
#include "url.h"
#include "line.h"
#include "line_layout.h"
#include <memory>
#include <gc_cpp.h>
#include <stddef.h>
#include <optional>

#define NO_BUFFER ((Buffer *)1)

/* Link Buffer */
enum LinkBuffer {
  LB_FRAME = 0 /* rFrame() */,
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
  const char *buffername = "";
  LineLayout layout = {};
  Buffer *nextBuffer = nullptr;
  std::array<Buffer *, MAX_LB> linkBuffer = {0};
  AnchorList *href = nullptr;
  AnchorList *name = nullptr;
  AnchorList *img = nullptr;
  AnchorList *formitem = nullptr;
  LinkList *linklist = nullptr;
  FormList *formlist = nullptr;
  MapList *maplist = nullptr;
  HmarkerList *hmarklist = nullptr;
  HmarkerList *imarklist = nullptr;
  const char *baseTarget = nullptr;
  std::shared_ptr<Clone> clone;

  size_t trbyte = 0;
  bool check_url = false;
  FormItemList *form_submit = nullptr;
  const char *edit = nullptr;
  const char *ssl_certificate = nullptr;
  char image_flag = 0;
  char image_loaded = 0;

  // always reshape new buffers to mark URLs
  bool need_reshape = true;
  Anchor *submit = nullptr;
  AlarmEvent *event = nullptr;

  Buffer(int width);
  ~Buffer();
  Buffer(const Buffer &src) { *this = src; }

  // shallow copy
  Buffer &operator=(const Buffer &src);
};

void _followForm(int submit);
void set_buffer_environ(Buffer *buf);
char *GetWord(Buffer *buf);
char *getCurWord(Buffer *buf, int *spos, int *epos);

inline void nextChar(int &s, Line *l) { s++; }
inline void prevChar(int &s, Line *l) { s--; }
// #define getChar(p) ((int)*(p))

extern void discardBuffer(Buffer *buf);
extern std::optional<Url> baseURL(Buffer *buf);
extern Buffer *page_info_panel(Buffer *buf);

extern Buffer *nullBuffer(void);
extern Buffer *namedBuffer(Buffer *first, char *name);
extern Buffer *replaceBuffer(Buffer *first, Buffer *delbuf, Buffer *newbuf);
extern Buffer *nthBuffer(Buffer *firstbuf, int n);
extern void gotoRealLine(Buffer *buf, int n);
extern Buffer *selectBuffer(Buffer *firstbuf, Buffer *currentbuf,
                            char *selectchar);
extern void reshapeBuffer(Buffer *buf);
extern Buffer *prevBuffer(Buffer *first, Buffer *buf);

extern void chkURLBuffer(Buffer *buf);
void isrch(int (*func)(Buffer *, const char *), const char *prompt);
void srch(int (*func)(Buffer *, const char *), const char *prompt);
void srch_nxtprv(int reverse);
void shiftvisualpos(Buffer *buf, int shift);
void execdict(const char *word);
void invoke_browser(const char *url);
void _peekURL(int only_img);
int checkBackBuffer(Buffer *buf);
void nextY(int d);
void nextX(int d, int dy);
void _prevA(int visited);
void _nextA(int visited);
int cur_real_linenumber(Buffer *buf);
void _movL(int n);
void _movD(int n);
void _movU(int n);
void _movR(int n);
int prev_nonnull_line(Line *line);
int next_nonnull_line(Line *line);
void repBuffer(Buffer *oldbuf, Buffer *buf);
void _goLine(const char *l);
void query_from_followform(Str **query, FormItemList *fi, int multipart);

struct Str;
Str *Str_form_quote(Str *x);
