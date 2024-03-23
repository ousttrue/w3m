#include "app.h"
#include "option_param.h"
#include "ioutil.h"
#include "event.h"
#include "url_decode.h"
#include "etc.h"
#include "ctrlcode.h"
#include "proc.h"
#include "quote.h"
#include "html/html_feed_env.h"
#include "html/form_item.h"
#include "html/anchorlist.h"
#include "anchor.h"
#include "html/html_quote.h"
#include "ssl_util.h"
#include "html/form.h"
#include "http_response.h"
#include "local_cgi.h"
#include "downloadlist.h"
#include "tabbuffer.h"
#include "buffer.h"
#include "myctype.h"
#include "rc.h"
#include "cookie.h"
#include "history.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

#include "ftxui/component/component.hpp"          // for Renderer
#include "ftxui/component/screen_interactive.hpp" // for ScreenInteractive, Component
#include "ftxui/dom/elements.hpp"

#ifdef _MSC_VER
#include <direct.h>
// #define getcwd _getcwd
#endif

bool displayLink = false;
bool displayLineInfo = false;
#define KEYMAP_FILE "keymap"
std::string keymap_file = KEYMAP_FILE;

const auto FRAME_INTERVAL_MS = std::chrono::milliseconds(1000 / 15);

const char *CurrentKeyData;
const char *CurrentCmdData;
int prec_num = 0;
bool on_target = true;
#define PREC_LIMIT 10000

static GC_warn_proc orig_GC_warn_proc = nullptr;
#define GC_WARN_KEEP_MAX (20)

void wrap_GC_warn_proc(char *msg, GC_word arg) {
  if (App::instance().isRawMode()) {
    /* *INDENT-OFF* */
    static struct {
      char *msg;
      GC_word arg;
    } msg_ring[GC_WARN_KEEP_MAX];
    /* *INDENT-ON* */
    static int i = 0;
    static size_t n = 0;
    static int lock = 0;
    int j;

    j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
    msg_ring[j].msg = msg;
    msg_ring[j].arg = arg;

    if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
      ++n;
    else
      ++i;

    if (!lock) {
      lock = 1;

      for (; n > 0; --n, ++i) {
        i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

        printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
        // sleep_till_anykey(1, 1);
      }

      lock = 0;
    }
  } else if (orig_GC_warn_proc)
    orig_GC_warn_proc(msg, arg);
  else
    fprintf(stderr, msg, (unsigned long)arg);
}

static void *die_oom(size_t bytes) {
  fprintf(stderr, "Out of memory: %lu bytes unavailable!\n",
          (unsigned long)bytes);
  exit(1);
  /*
   * Suppress compiler warning: function might return no value
   * This code is never reached.
   */
  return nullptr;
}

App::App() {

  static int s_i = 0;
  assert(s_i == 0);
  ++s_i;

  _w3mFuncList = {
      // /*0*/ {"@@@", nulcmd},
      /*1*/ {"ABORT", quitfm},
      /*2*/ {"ACCESSKEY", accessKey},
      /*3*/ {"ADD_BOOKMARK", adBmark},
      /*4*/ {"ALARM", setAlarm},
      /*5*/ {"BACK", backBf},
      /*6*/ {"BEGIN", goLineF},
      /*7*/ {"BOOKMARK", ldBmark},
      /*8*/ {"CENTER_H", ctrCsrH},
      /*9*/ {"CENTER_V", ctrCsrV},
      /*10*/ {"CHARSET", docCSet},
      /*11*/ {"CLOSE_TAB", closeT},
      /*12*/ {"CLOSE_TAB_MOUSE", closeTMs},
      /*13*/ {"COMMAND", execCmd},
      /*14*/ {"COOKIE", cooLst},
      /*15*/ {"CURSOR_BOTTOM", cursorBottom},
      /*16*/ {"CURSOR_MIDDLE", cursorMiddle},
      /*17*/ {"CURSOR_TOP", cursorTop},
      /*18*/ {"DEFAULT_CHARSET", defCSet},
      /*19*/ {"DEFINE_KEY", defKey},
      /*20*/ {"DELETE_PREVBUF", deletePrevBuf},
      /*21*/ {"DICT_WORD", dictword},
      /*22*/ {"DICT_WORD_AT", dictwordat},
      /*23*/ {"DISPLAY_IMAGE", dispI},
      /*24*/ {"DOWN", ldown1},
      /*25*/ {"DOWNLOAD", svSrc},
      /*26*/ {"DOWNLOAD_LIST", ldDL},
      /*27*/ {"EDIT", editBf},
      /*28*/ {"EDIT_SCREEN", editScr},
      /*29*/ {"END", goLineL},
      /*30*/ {"ESCBMAP", nulcmd},
      /*31*/ {"ESCMAP", nulcmd},
      /*32*/ {"EXEC_SHELL", execsh},
      /*33*/ {"EXIT", quitfm},
      /*34*/ {"EXTERN", extbrz},
      /*35*/ {"EXTERN_LINK", linkbrz},
      /*36*/ {"FRAME", rFrame},
      /*37*/ {"GOTO", goURL},
      /*38*/ {"GOTO_HOME", goHome},
      /*39*/ {"GOTO_LINE", goLine},
      /*40*/ {"GOTO_LINK", followA},
      /*41*/ {"GOTO_RELATIVE", gorURL},
      /*42*/ {"HELP", ldhelp},
      /*43*/ {"HISTORY", ldHist},
      /*44*/ {"INFO", pginfo},
      /*45*/ {"INTERRUPT", susp},
      /*46*/ {"ISEARCH", isrchfor},
      /*47*/ {"ISEARCH_BACK", isrchbak},
      /*48*/ {"LEFT", col1L},
      /*49*/ {"LINE_BEGIN", linbeg},
      /*50*/ {"LINE_END", linend},
      /*51*/ {"LINE_INFO", curlno},
      /*52*/ {"LINK_BEGIN", topA},
      /*53*/ {"LINK_END", lastA},
      /*54*/ {"LINK_MENU", linkMn},
      /*55*/ {"LINK_N", nthA},
      /*56*/ {"LIST", linkLst},
      /*57*/ {"LIST_MENU", listMn},
      /*58*/ {"LOAD", ldfile},
      /*59*/ {"MAIN_MENU", mainMn},
      /*60*/ {"MARK", _mark},
      /*61*/ {"MARK_MID", chkNMID},
      /*62*/ {"MARK_URL", nulcmd},
      /*63*/ {"MARK_WORD", nulcmd},
      /*64*/ {"MENU", mainMn},
      /*65*/ {"MENU_MOUSE", menuMs},
      /*66*/ {"MOUSE", mouse},
      /*67*/ {"MOUSE_TOGGLE", msToggle},
      /*68*/ {"MOVE_DOWN", movD},
      /*69*/ {"MOVE_DOWN1", movD1},
      /*70*/ {"MOVE_LEFT", movL},
      /*71*/ {"MOVE_LEFT1", movL1},
      /*72*/ {"MOVE_LIST_MENU", movlistMn},
      /*73*/ {"MOVE_MOUSE", movMs},
      /*74*/ {"MOVE_RIGHT", movR},
      /*75*/ {"MOVE_RIGHT1", movR1},
      /*76*/ {"MOVE_UP", movU},
      /*77*/ {"MOVE_UP1", movU1},
      /*78*/ {"MSGS", msgs},
      /*79*/ {"MULTIMAP", nulcmd},
      /*80*/ {"NEW_TAB", newT},
      /*81*/ {"NEXT", nextBf},
      /*82*/ {"NEXT_DOWN", nextD},
      /*83*/ {"NEXT_HALF_PAGE", hpgFore},
      /*84*/ {"NEXT_LEFT", nextL},
      /*85*/ {"NEXT_LEFT_UP", nextLU},
      /*86*/ {"NEXT_LINK", nextA},
      /*87*/ {"NEXT_MARK", nextMk},
      /*88*/ {"NEXT_PAGE", pgFore},
      /*89*/ {"NEXT_RIGHT", nextR},
      /*90*/ {"NEXT_RIGHT_DOWN", nextRD},
      /*91*/ {"NEXT_TAB", nextT},
      /*92*/ {"NEXT_UP", nextU},
      /*93*/ {"NEXT_VISITED", nulcmd},
      /*94*/ {"NEXT_WORD", movRW},
      /*95*/ {"NOTHING", nulcmd},
      /*96*/ {"NULL", nulcmd},
      /*97*/ {"OPTIONS", ldOpt},
      /*98*/ {"PEEK", nulcmd},
      /*99*/ {"PEEK_IMG", nulcmd},
      /*100*/ {"PEEK_LINK", ::peekURL},
      /*101*/ {"PIPE_BUF", pipeBuf},
      /*102*/ {"PIPE_SHELL", pipesh},
      /*103*/ {"PREV", prevBf},
      /*104*/ {"PREV_HALF_PAGE", hpgBack},
      /*105*/ {"PREV_LINK", prevA},
      /*106*/ {"PREV_MARK", prevMk},
      /*107*/ {"PREV_PAGE", pgBack},
      /*108*/ {"PREV_TAB", prevT},
      /*109*/ {"PREV_VISITED", nulcmd},
      /*110*/ {"PREV_WORD", movLW},
      /*111*/ {"PRINT", svBuf},
      /*112*/ {"QUIT", qquitfm},
      /*113*/ {"READ_SHELL", readsh},
      /*114*/ {"REDO", redoPos},
      /*115*/ {"REDRAW", rdrwSc},
      /*116*/ {"REG_MARK", reMark},
      /*117*/ {"REINIT", reinit},
      /*118*/ {"RELOAD", reload},
      /*119*/ {"RESHAPE", reshape},
      /*120*/ {"RIGHT", col1R},
      /*121*/ {"SAVE", svSrc},
      /*122*/ {"SAVE_IMAGE", svI},
      /*123*/ {"SAVE_LINK", svA},
      /*124*/ {"SAVE_SCREEN", svBuf},
      /*125*/ {"SEARCH", srchfor},
      /*126*/ {"SEARCH_BACK", srchbak},
      /*127*/ {"SEARCH_FORE", srchfor},
      /*128*/ {"SEARCH_NEXT", srchnxt},
      /*129*/ {"SEARCH_PREV", srchprv},
      /*130*/ {"SELECT", selBuf},
      /*131*/ {"SELECT_MENU", selMn},
      /*132*/ {"SETENV", setEnv},
      /*133*/ {"SET_OPTION", setOpt},
      /*134*/ {"SGRMOUSE", sgrmouse},
      /*135*/ {"SHELL", execsh},
      /*136*/ {"SHIFT_LEFT", shiftl},
      /*137*/ {"SHIFT_RIGHT", shiftr},
      /*138*/ {"SOURCE", vwSrc},
      /*139*/ {"STOP_IMAGE", stopI},
      /*140*/ {"SUBMIT", submitForm},
      /*141*/ {"SUSPEND", susp},
      /*142*/ {"TAB_GOTO", tabURL},
      /*143*/ {"TAB_GOTO_RELATIVE", tabrURL},
      /*144*/ {"TAB_LEFT", tabL},
      /*145*/ {"TAB_LINK", tabA},
      /*146*/ {"TAB_MENU", tabMn},
      /*147*/ {"TAB_MOUSE", tabMs},
      /*148*/ {"TAB_RIGHT", tabR},
      /*149*/ {"UNDO", undoPos},
      /*150*/ {"UP", lup1},
      /*151*/ {"VERSION", dispVer},
      /*152*/ {"VIEW", vwSrc},
      /*153*/ {"VIEW_BOOKMARK", ldBmark},
      /*154*/ {"VIEW_IMAGE", followI},
      /*155*/ {"WHEREIS", srchfor},
      /*156*/ {"WRAP_TOGGLE", wrapToggle},
  };
  _GlobalKeymap = {
      /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g      */
      "MARK",
      "LINE_BEGIN",
      "MOVE_LEFT",
      {},
      {},
      "LINE_END",
      "MOVE_RIGHT",
      "LINE_INFO",
      /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o      */
      "HISTORY",
      "NEXT_LINK",
      "GOTO_LINK",
      "COOKIE",
      "REDRAW",
      "GOTO_LINK",
      "MOVE_DOWN",
      {},
      /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w      */
      "MOVE_UP",
      "CLOSE_TAB",
      "ISEARCH_BACK",
      "ISEARCH",
      "TAB_LINK",
      "PREV_LINK",
      "NEXT_PAGE",
      "WRAP_TOGGLE",
      /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_      */
      {},
      {},
      "SUSPEND",
      {},
      {},
      {},
      {},
      "GOTO_HOME",
      /*  SPC     !       "       #       $       %       &       '        */
      "NEXT_PAGE",
      "SHELL",
      "REG_MARK",
      "PIPE_SHELL",
      "LINE_END",
      {},
      {},
      {},
      /*  (       )       *       +       FuncId::,       -       .       / */
      "UNDO",
      "REDO",
      {},
      "NEXT_PAGE",
      "LEFT",
      "PREV_PAGE",
      "RIGHT",
      "SEARCH",
      /*  0       1       2       3       4       5       6       7        */
      {},
      {},
      {},
      {},
      {},
      {},
      {},
      {},
      /*  8       9       :       ;       <       =       >       ?        */
      {},
      {},
      "MARK_URL",
      "MARK_WORD",
      "SHIFT_LEFT",
      "INFO",
      "SHIFT_RIGHT",
      "SEARCH_BACK",
      /*  @       A       B       C       D       E       F       G        */
      "READ_SHELL",
      {},
      "BACK",
      {},
      "DOWNLOAD_LIST",
      "EDIT",
      "FRAME",
      "END",
      /*  H       I       J       K       L       M       N       O        */
      "HELP",
      "VIEW_IMAGE",
      "UP",
      "DOWN",
      "LIST",
      "EXTERN",
      "SEARCH_PREV",
      {},
      /*  P       Q       R       S       T       U       V       W        */
      {},
      "ABORT",
      "RELOAD",
      "PRINT",
      "NEW_TAB",
      "GOTO",
      "LOAD",
      "PREV_WORD",
      /*  X       Y       Z       [       \       ]       ^       _        */
      {},
      {},
      "CENTER_H",
      "LINK_BEGIN",
      {},
      "LINK_END",
      "LINK_BEGIN",
      {},
      /*  `       a       b       c       d       e       f       g        */
      {},
      "SAVE_LINK",
      "PREV_PAGE",
      "PEEK",
      {},
      {},
      {},
      "BEGIN",
      /*  h       i       j       k       l       m       n       o        */
      "MOVE_LEFT",
      "PEEK_IMG",
      "MOVE_DOWN",
      "MOVE_UP",
      "MOVE_RIGHT",
      "MOUSE_TOGGLE",
      "SEARCH_NEXT",
      "OPTIONS",
      /*  p       q       r       s       t       u       v       w        */
      {},
      "QUIT",
      "VERSION",
      "SELECT_MENU",
      {},
      "PEEK_LINK",
      "VIEW",
      "NEXT_WORD",
      /*  x       y       z       {       |       }       ~       DEL      */
      {},
      {},
      "CENTER_V",
      "PREV_TAB",
      "PIPE_BUF",
      "NEXT_TAB",
      {},
      {},
  };

  // uv_tty_init(uv_default_loop(), &g_tty_in, 0, 1);

  //
  // process
  //
  _currentPid = getpid();

  ioutil::initialize();

  if (!getenv("GC_LARGE_ALLOC_WARN_INTERVAL")) {
    set_environ("GC_LARGE_ALLOC_WARN_INTERVAL", "30000");
  }
  GC_INIT();
#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  GC_set_oom_fn(die_oom);
#else
  GC_oom_fn = die_oom;
#endif

#if (GC_VERSION_MAJOR > 7) ||                                                  \
    ((GC_VERSION_MAJOR == 7) && (GC_VERSION_MINOR >= 2))
  orig_GC_warn_proc = GC_get_warn_proc();
  GC_set_warn_proc(wrap_GC_warn_proc);
#else
  orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
#endif

  _tabs.push_back(TabBuffer::create());

  // default dispatcher
  _dispatcher.push([this](const char *buf, size_t len) {
    this->dispatchPtyIn(buf, len);
    // never exit
    return true;
  });
}

App::~App() {
  if (getpid() != _currentPid) {
    return;
  }
  endRawMode();

  // clean up only not fork process
  std::cout << "App::~App" << std::endl;

  // exit
  // term_title(""); /* XXX */
  save_cookies();
  if (UseHistory && SaveURLHist) {
    URLHist->saveHistory(URLHistSize);
  }

  for (auto &f : _fileToDelete) {
    std::cout << "fileToDelete: " << f << std::endl;
    unlink(f.c_str());
  }
  _fileToDelete.clear();

  stopDownload();
  free_ssl_ctx();
}

static int getKey2(const char **str) {
  const char *s = *str;
  int esc = 0, ctrl = 0;

  if (s == NULL || *s == '\0')
    return -1;

  if (strncasecmp(s, "C-", 2) == 0) { /* ^, ^[^ */
    s += 2;
    ctrl = 1;
  } else if (*s == '^' && *(s + 1)) { /* ^, ^[^ */
    s++;
    ctrl = 1;
  }

  if (ctrl) {
    *str = s + 1;
    if (*s >= '@' && *s <= '_') /* ^@ .. ^_ */
      return esc | (*s - '@');
    else if (*s >= 'a' && *s <= 'z') /* ^a .. ^z */
      return esc | (*s - 'a' + 1);
    else if (*s == '?') /* ^? */
      return esc | DEL_CODE;
    else
      return -1;
  }

  if (strncasecmp(s, "SPC", 3) == 0) { /* ' ' */
    *str = s + 3;
    return esc | ' ';
  } else if (strncasecmp(s, "TAB", 3) == 0) { /* ^i */
    *str = s + 3;
    return esc | '\t';
  } else if (strncasecmp(s, "DEL", 3) == 0) { /* ^? */
    *str = s + 3;
    return esc | DEL_CODE;
  }

  if (*s == '\\' && *(s + 1) != '\0') {
    s++;
    *str = s + 1;
    switch (*s) {
    case 'a': /* ^g */
      return esc | CTRL_G;
    case 'b': /* ^h */
      return esc | CTRL_H;
    case 't': /* ^i */
      return esc | CTRL_I;
    case 'n': /* ^j */
      return esc | CTRL_J;
    case 'r': /* ^m */
      return esc | CTRL_M;
    case 'e': /* ^[ */
      return esc | ESC_CODE;
    case '^': /* ^ */
      return esc | '^';
    case '\\': /* \ */
      return esc | '\\';
    default:
      return -1;
    }
  }
  *str = s + 1;
  if (IS_ASCII(*s)) /* Ascii */
    return esc | *s;
  else
    return -1;
}

int getKey(const char *s) {
  int c = getKey2(&s);
  return c;
}

void App::setKeymap(const char *p, int lineno, int verbose) {
  auto s = getQWord(&p);
  auto c = getKey(s);
  if (c < 0) { /* error */
    const char *emsg;
    if (lineno > 0)
      emsg = Sprintf("line %d: unknown key '%s'", lineno, s)->ptr;
    else
      emsg = Sprintf("defkey: unknown key '%s'", s)->ptr;
    App::instance().App::instance().record_err_message(emsg);
    if (verbose)
      App::instance().disp_message_nsec(emsg, 1, true);
    return;
  }
  std::string f = getWord(&p);
  if (f.empty()) {
    const char *emsg;
    if (lineno > 0)
      emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
    else
      emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
    App::instance().record_err_message(emsg);
    if (verbose)
      App::instance().disp_message_nsec(emsg, 1, true);
    return;
  }
  auto map = _GlobalKeymap;
  map[c & 0x7F] = f;
  s = getQWord(&p);
  _keyData.insert({f, s});
}

bool App::initialize() {

  init_rc();

  if (BookmarkFile.empty()) {
    BookmarkFile = rcFile(BOOKMARK);
  }

  sync_with_option();
  initCookie();

  LoadHist = Hist::newHist();
  SaveHist = Hist::newHist();
  ShellHist = Hist::newHist();
  TextHist = Hist::newHist();
  URLHist = Hist::newHist();
  if (UseHistory) {
    URLHist->loadHistory();
  }

  onResize();

  return true;
}

/*
 * Initialize routine.
 */
void App::beginRawMode(void) {
  // if (!_fmInitialized) {
  //   // initscr();
  //   uv_tty_set_mode(&g_tty_in, UV_TTY_MODE_RAW);
  //   // term_raw();
  //   // term_noecho();
  //   _fmInitialized = true;
  // }
}

void App::endRawMode(void) {
  // if (_fmInitialized) {
  //   CurrentTab->currentBuffer()->layout->clrtoeolx(
  //       {.row = LASTLINE(), .col = 0});
  //   // _screen->print();
  //   uv_tty_set_mode(&g_tty_in, UV_TTY_MODE_NORMAL);
  //   _fmInitialized = false;
  // }
}

static void set_buffer_environ(const std::shared_ptr<Buffer> &buf) {
  static std::shared_ptr<Buffer> prev_buf;
  static Line *prev_line = nullptr;
  static int prev_pos = -1;
  Line *l;

  if (buf == nullptr)
    return;
  if (buf != prev_buf) {
    set_environ("W3M_SOURCEFILE", buf->res->sourcefile.c_str());
    set_environ("W3M_FILENAME", buf->res->filename.c_str());
    set_environ("W3M_TITLE", buf->layout->data.title.c_str());
    set_environ("W3M_URL", buf->res->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE",
                buf->res->type.size() ? buf->res->type.c_str() : "unknown");
  }
  l = buf->layout->currentLine();
  if (l && (buf != prev_buf || l != prev_line ||
            buf->layout->cursorPos() != prev_pos)) {
    auto s = buf->layout->getCurWord();
    set_environ("W3M_CURRENT_WORD", s.c_str());

    if (auto a = buf->layout->retrieveCurrentAnchor()) {
      Url pu(a->url, buf->layout->data.baseURL);
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else {
      set_environ("W3M_CURRENT_LINK", "");
    }

    if (auto a = buf->layout->retrieveCurrentImg()) {
      Url pu(a->url, buf->layout->data.baseURL);
      set_environ("W3M_CURRENT_IMG", pu.to_Str().c_str());
    } else {
      set_environ("W3M_CURRENT_IMG", "");
    }

    if (auto a = buf->layout->retrieveCurrentForm()) {
      set_environ("W3M_CURRENT_FORM", a->formItem->form2str().c_str());
    } else {
      set_environ("W3M_CURRENT_FORM", "");
    }

    set_environ("W3M_CURRENT_LINE",
                Sprintf("%ld", buf->layout->linenumber(l))->ptr);
    set_environ("W3M_CURRENT_COLUMN",
                Sprintf("%d", buf->layout->visual.cursor().col + 1)->ptr);
  } else if (!l) {
    set_environ("W3M_CURRENT_WORD", "");
    set_environ("W3M_CURRENT_LINK", "");
    set_environ("W3M_CURRENT_IMG", "");
    set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", "0");
    set_environ("W3M_CURRENT_COLUMN", "0");
  }
  prev_buf = buf;
  prev_line = l;
  prev_pos = buf->layout->cursorPos();
}

// static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
//                          uv_buf_t *buf) {
//   *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
// }

int App::mainLoop() {
  // App::instance().beginRawMode();
  _fmInitialized = true;

  using namespace ftxui;

  auto screen = ScreenInteractive::Fullscreen();

  auto r = Renderer([this] { return this->dom(); });
  auto e = std::bind(&App::onEvent, this, std::placeholders::_1);
  auto component = CatchEvent(r, e);

  screen.Loop(component);

  return 0;
}

bool App::onEvent(const ftxui::Event &event) {
  // ScreenInteractive::EventLister 経由で別スレッド？

  _event_status.push_front(ftxui::text(stringify(event)));
  while (_event_status.size() > 4) {
    _event_status.pop_back();
  }

  // [&](Event event) {
  if (popAddDownloadList()) {
    ldDL(context());
  }

  auto buf = currentTab()->currentBuffer();
  if (auto a = buf->layout->data.submit) {
    buf->layout->data.submit = NULL;
    buf->layout->visual.cursorRow(a->start.line);
    buf->layout->cursorPos(a->start.pos);
    if (auto newbuf = buf->followForm(a, true)->return_value().value()) {
      pushBuffer(newbuf, a->target);
      buf = newbuf;
    }
  }

  // reload
  if (buf->layout->data.need_reshape) {
    buf->layout->data.need_reshape = false;
    auto body = buf->res->getBody();
    auto visual = buf->layout->visual;
    // if (buf->res->is_html_type()) {
    loadHTMLstream(buf->layout, _size.col, buf->res->currentURL, body);
    // } else {
    //   loadBuffer(buf->layout, buf->res.get(), body);
    // }
    buf->layout->visual = visual;
  }

  if (event.is_mouse()) {
  } else if (event == ftxui::Event::Custom) {
    onResize();
    return true;
  } else {
    // && !std::isdigit(event.character()[0]);
    auto c = event.character();
    dispatchPtyIn(c.c_str(), c.size());
    return true;
  }

  return false;
}

#ifdef _MSC_VER
#include <Windows.h>
RowCol getTermSize() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  int columns, rows;

  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

  return {
      .row = rows,
      .col = columns,
  };
}
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
RowCol getTermSize() {
  struct winsize win;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
  return {
      .row = win.ws_row,
      .col = win.ws_col,
  };
}
#endif

void App::onResize() {
  _size = getTermSize();
  auto buf = CurrentTab->currentBuffer();
  if (!buf) {
    return;
  }
  if (_size.col == buf->layout->data._cols) {
    return;
  }
  buf->layout->setupscreen(_size);
}

void App::exit(int) {
  // const char *ans = "y";
  // if (checkDownloadList()) {
  //   ans = inputChar("Download process retains. "
  //                   "Do you want to exit w3m? (y/n)");
  // } else if (confirm) {
  //   ans = inputChar("Do you want to exit w3m? (y/n)");
  // }

  // if (!(ans && TOLOWER(*ans) == 'y')) {
  //   // cancel
  //   App::instance().invalidate();
  //   return;
  // }

  // App::instance().exit(0);
  // stop libuv
  // exit(i);
  // uv_read_stop((uv_stream_t *)&g_tty_in);
  // uv_signal_stop(&g_signal_resize);
  // uv_timer_stop(&g_timer);
}

int App::searchKeyNum() {
  int n = 1;
  if (auto d = searchKeyData()) {
    n = atoi(d);
  }
  return n * PREC_NUM;
}

const char *App::searchKeyData() {
  const char *data = NULL;
  if (CurrentKeyData != NULL && *CurrentKeyData != '\0') {
    data = CurrentKeyData;
  } else if (CurrentCmdData != NULL && *CurrentCmdData != '\0') {
    data = CurrentCmdData;
  } else {
    data = getKeyData(_currentKey);
  }
  CurrentKeyData = NULL;
  CurrentCmdData = NULL;
  if (data == NULL || *data == '\0')
    return NULL;
  return allocStr(data, -1);
}

void App::peekURL() {
  auto buf = currentTab()->currentBuffer();
  if (buf->layout->empty()) {
    return;
  }

  if (_currentKey == _prev_key && _peekUrl.size()) {
    if (_peekUrl.size() - _peekUrlOffset >= COLS()) {
      _peekUrlOffset++;
    } else if (_peekUrl.size() <= _peekUrlOffset) { /* bug ? */
      _peekUrlOffset = 0;
    }
    goto disp;
  } else {
    _peekUrlOffset = 0;
  }

  _peekUrl = "";
  if (auto a = buf->layout->retrieveCurrentAnchor()) {
    _peekUrl = a->url;
    if (_peekUrl.empty()) {
      Url pu(a->url, buf->layout->data.baseURL);
      _peekUrl = pu.to_Str();
    }
  } else if (auto a = buf->layout->retrieveCurrentForm()) {
    _peekUrl = a->formItem->form2str();
    if (_peekUrl.empty()) {
      Url pu(a->url, buf->layout->data.baseURL);
      _peekUrl = pu.to_Str();
    }
  } else if (auto a = buf->layout->retrieveCurrentImg()) {
    _peekUrl = a->url;
    if (_peekUrl.empty()) {
      Url pu(a->url, buf->layout->data.baseURL);
      _peekUrl = pu.to_Str();
    }
  } else {
    return;
  }

  if (DecodeURL) {
    _peekUrl = url_decode0(_peekUrl);
  }

disp:
  int n = searchKeyNum();
  if (n > 1 && _peekUrl.size() > (n - 1) * (COLS() - 1)) {
    _peekUrlOffset = (n - 1) * (COLS() - 1);
  }
  disp_message(_peekUrl.substr(_peekUrlOffset).c_str());
}

void App::doCmd() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == nullptr) {
      return;
    }
  }
  /* data: FUNC [DATA] [; FUNC [DATA] ...] */
  while (*data) {
    SKIP_BLANKS(data);
    if (*data == ';') {
      data++;
      continue;
    }
    auto p = getWord(&data);
    std::string cmd = p;
    if (cmd.empty()) {
      break;
    }
    p = getQWord(&data);
    doCmd(cmd, p);
  }
}

void App::doCmd(const std::string &cmd, const char *data) {
  auto found = _w3mFuncList.find(cmd);
  if (found != _w3mFuncList.end()) {
    _currentKey = -1;
    CurrentKeyData = nullptr;
    CurrentCmdData = *data ? data : nullptr;
    found->second(context());
    CurrentCmdData = nullptr;
  }
}

void App::dispatchPtyIn(const char *buf, size_t len) {
  if (len == 0) {
    return;
  }
  auto c = buf[0];
  _lastKeyCmd.str("");
  if (std::isalnum(c) || std::ispunct(c)) {
    _lastKeyCmd << c;
  } else {
    _lastKeyCmd << "0x" << std::hex << (int)c;
  }
  if (IS_ASCII(c)) { /* Ascii */
    if (('0' <= c) && (c <= '9') && (prec_num || (_GlobalKeymap[c].empty()))) {
      prec_num = prec_num * 10 + (int)(c - '0');
      if (prec_num > PREC_LIMIT)
        prec_num = PREC_LIMIT;
    } else {
      set_buffer_environ(currentTab()->currentBuffer());
      // currentTab()->currentBuffer()->layout.save_buffer_position();
      _currentKey = c;
      auto cmd = _GlobalKeymap[c];
      if (cmd.size()) {
        _lastKeyCmd << " => " << cmd;
        auto callback = _w3mFuncList[cmd];
        callback(context());
      } else {
        _lastKeyCmd << " => not found";
      }
      prec_num = 0;
    }
  }
  _prev_key = _currentKey;
  _currentKey = -1;
  CurrentKeyData = NULL;
}

ftxui::Element App::dom() {
  auto buf = currentTab()->currentBuffer();

  std::vector<ftxui::Element> events;
  for (auto it = _event_status.begin(); it != _event_status.end(); ++it) {
    if (it != _event_status.begin()) {
      events.push_back(ftxui::separator());
    }
    events.push_back(*it);
  }

  return ftxui::vbox({
      // tabs
      tabs(),
      // address
      ftxui::text(buf->res->currentURL.to_Str()) | ftxui::inverted,
      // content
      buf->layout | ftxui::flex,
      // message
      ftxui::text(_message) | ftxui::inverted,
      // status
      ftxui::text(buf->layout->data.status()),
      ftxui::text(buf->layout->row_status()),
      ftxui::text(buf->layout->col_status()),
      ftxui::hbox(events),
  });
}

ftxui::Element App::tabs() {

  std::vector<ftxui::Element> tabs;

  for (auto tab : _tabs) {
    if (tabs.size()) {
      tabs.push_back(ftxui::separator());
    }
    if (tab == currentTab()) {
      tabs.push_back(ftxui::text(tab->currentBuffer()->layout->data.title) |
                     ftxui::inverted);
    } else {
      tabs.push_back(ftxui::text(tab->currentBuffer()->layout->data.title));
    }
  }

  return ftxui::vbox(tabs);
}

struct TimerTask {
  std::string cmd;
  std::string data;
  uv_timer_t handle;
};
std::list<std::shared_ptr<TimerTask>> g_timers;

void App::task(int sec, const std::string &cmd, const char *data, bool repeat) {
  if (cmd.size()) {
    auto t = std::make_shared<TimerTask>();
    t->cmd = cmd;
    t->data = data ? data : "";
    t->handle.data = t.get();
    g_timers.push_back(t);

    uv_timer_init(uv_default_loop(), &t->handle);
    if (repeat) {
      uv_timer_start(
          &t->handle,
          [](uv_timer_t *handle) {
            auto _t = (TimerTask *)handle->data;
            App::instance().doCmd(_t->cmd, _t->data.c_str());
          },
          0, sec * 1000);
    } else {
      uv_timer_start(
          &t->handle,
          [](uv_timer_t *handle) {
            auto _t = (TimerTask *)handle->data;
            App::instance().doCmd(_t->cmd, _t->data.c_str());
            for (auto it = g_timers.begin(); it != g_timers.end(); ++it) {
              if (it->get() == _t) {
                g_timers.erase(it);
                break;
              }
            }
          },
          sec * 1000, 0);
    }
    disp_message_nsec(Sprintf("%dsec %s %s", sec, cmd.c_str(), data)->ptr, 1,
                      false);
  } else {
    // cancel timer
    g_timers.clear();
  }
}

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp", "src", "frame", "cache", "cookie", "hist",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

std::string App::tmpfname(TmpfType type, const std::string &ext) {
  std::stringstream ss;
  ss << rc_dir << "/w3m" << tmpf_base[type] << App::instance().pid()
     << tmpf_seq[type]++ << ext;
  auto tmpf = ss.str();
  _fileToDelete.push_back(tmpf);
  return tmpf;
}

std::shared_ptr<TabBuffer> App::numTab(int n) const {
  if (n == 0)
    return currentTab();

  auto it = _tabs.begin();

  for (int i = 1; i < n && it != _tabs.end(); ++i) {
    ++it;
  }
  return *it ? *it : _tabs.back();
}

// void App::drawTabs() {
//   CurrentTab->currentBuffer()->layout->clrtoeolx({0, 0});
//   int y = 0;
//   for (auto t : _tabs) {
//     y = t->draw(_content.get(), currentTab().get());
//   }
//   RowCol pos{.row = y + 1, .col = 0};
//   for (int i = 0; i < COLS(); i++) {
//     _content->addch(pos, '~');
//     ++pos.col;
//   }
// }

void App::nextTab(int n) {
  assert(_tabs.size() > 0);
  for (int i = 0; i < n; i++) {
    ++_currentTab;
  }
  if (_currentTab >= (int)_tabs.size()) {
    _currentTab = _tabs.size() - 1;
  }
}

void App::prevTab(int n) {
  assert(_tabs.size() > 0);
  for (int i = 0; i < n; i++) {
    --_currentTab;
  }
  if (_currentTab < 0) {
    _currentTab = 0;
  }
}

void App::tabRight(int n) {
  auto src = _tabs.begin();
  for (int i = 0; i < _currentTab && src != _tabs.end(); ++i) {
    ++src;
  }
  if (src == _tabs.end()) {
    assert(false);
    return;
  }
  auto end = src;
  ++end;

  // right position
  auto dst = src;
  ++dst;
  for (int i = 0; i < n && dst != _tabs.end(); ++i) {
    ++dst;
  };

  _tabs.splice(dst, _tabs, src, end);
}

void App::tabLeft(int n) {
  auto src = _tabs.begin();
  for (int i = 0; i < _currentTab && src != _tabs.end(); ++i) {
    ++src;
  }
  if (src == _tabs.end()) {
    assert(false);
    return;
  }
  auto end = src;
  ++end;

  auto dst = src;
  for (int i = 0; i < n && dst != _tabs.begin(); ++i) {
    --dst;
  }

  _tabs.splice(dst, _tabs, src, end);
}

int App::calcTabPos() {
  int lcol = 0, rcol = 0, col;

  int n2, ny;
  int n1 = (COLS() - rcol - lcol) / TabCols;
  if (n1 >= (int)_tabs.size()) {
    n2 = 1;
    ny = 1;
  } else {
    if (n1 < 0)
      n1 = 0;
    n2 = COLS() / TabCols;
    if (n2 == 0)
      n2 = 1;
    ny = (_tabs.size() - n1 - 1) / n2 + 2;
  }

  // int n2, na, nx, ny, ix, iy;
  int na = n1 + n2 * (ny - 1);
  n1 -= (na - _tabs.size()) / ny;
  if (n1 < 0)
    n1 = 0;
  na = n1 + n2 * (ny - 1);

  auto tab = _tabs.begin();
  ;
  for (int iy = 0; iy < ny && tab != _tabs.end(); iy++) {
    int nx;
    if (iy == 0) {
      nx = n1;
      col = COLS() - rcol - lcol;
    } else {
      nx = n2 - (na - _tabs.size() + (iy - 1)) / (ny - 1);
      col = COLS();
    }
    for (int ix = 0; ix < nx && tab != _tabs.end(); ix++, ++tab) {
      (*tab)->x1 = col * ix / nx;
      (*tab)->x2 = col * (ix + 1) / nx - 1;
      (*tab)->y = iy;
      if (iy == 0) {
        (*tab)->x1 += lcol;
        (*tab)->x2 += lcol;
      }
    }
  }
  return _tabs.back()->y;
}

void App::deleteTab(const std::shared_ptr<TabBuffer> &tab) {
  auto found = std::find(_tabs.begin(), _tabs.end(), tab);
  if (found == _tabs.end()) {
    return;
  }
  _tabs.erase(found);
}

std::shared_ptr<TabBuffer> App::newTab(std::shared_ptr<Buffer> buf) {
  if (!buf) {
    buf = Buffer::create();
    *buf = *currentTab()->currentBuffer();
  }
  auto tab = TabBuffer::create();
  tab->firstBuffer = tab->_currentBuffer = buf;

  _tabs.push_back(tab);
  _currentTab = _tabs.size() - 1;

  return tab;
}

void App::pushBuffer(const std::shared_ptr<Buffer> &buf,
                     std::string_view target) {
  //   if (check_target && open_tab_blank && a->target.size() &&
  //       (a->target == "_new" || a->target == "_blank")) {
  //     // open in new tab
  //     App::instance().newTab(buf);
  //   }

  if (target.empty() || /* no target specified (that means this page is
  not a
                           frame page) */
      target == "_top"  /* this link is specified to be opened as an
                                   indivisual * page */
  ) {
    CurrentTab->pushBuffer(buf);
  } else {
    auto label = Strnew_m_charp("_", target, nullptr)->ptr;
    auto buf = CurrentTab->currentBuffer();
    auto al = buf->layout->data._name->searchAnchor(label);
    if (al) {
      buf->layout->visual.cursorRow(al->start.line);
      // if (label_topline) {
      //   buf->layout._topLine += buf->layout.cursor.row;
      // }
      buf->layout->cursorPos(al->start.pos);
    }
  }
}

void App::message(const std::string &s) {
  _message = s;
  if (!_fmInitialized) {
    // RowCol pos{.row = LASTLINE(), .col = 0};
    // pos = _content->addnstr(pos, s, COLS() - 1);
    // _content->clrtoeolx(pos);
    // this->cursor(cursor);
    fprintf(stderr, "%s\n", s.c_str());
  }
}

void App::record_err_message(const char *s) {
  if (!_fmInitialized) {
    return;
  }
  if ((int)message_list.size() >= LINES()) {
    message_list.pop_back();
  }
  message_list.push_back(s);
}

/*
 * List of error messages
 */
std::string App::message_list_panel() {
  std::stringstream tmp;
  tmp << "<html><head><title>List of error messages</title></head><body>"
         "<h1>List of error messages</h1><table cellpadding=0>\n";
  if (message_list.size()) {
    for (auto p = message_list.rbegin(); p != message_list.rend(); ++p)
      tmp << "<tr><td><pre>" << html_quote(p->c_str()) << "</pre></td></tr>\n";
  } else {
    tmp << "<tr><td>(no message recorded)</td></tr>\n";
  }
  tmp << "</table></body></html>";
  return tmp.str();
}

void App::disp_err_message(const char *s) {
  record_err_message(s);
  disp_message(s);
}

void App::disp_message_nsec(const char *s, int sec, int purge) {
  if (IsForkChild)
    return;
  if (!_fmInitialized) {
    fprintf(stderr, "%s\n", s);
    return;
  }

  message(s);
  // _screen->print();
  // sleep_till_anykey(sec, purge);
}

void App::disp_message(const char *s) { disp_message_nsec(s, 10, false); }

void App::set_delayed_message(const char *s) { delayed_msg = allocStr(s, -1); }

void App::refresh_message() {
  if (delayed_msg != NULL) {
    disp_message(delayed_msg);
    delayed_msg = NULL;
    // _screen->print();
  }
}

static void interpret_keymap(FILE *kf, struct stat *current, int force) {
  int fd;
  struct stat kstat;
  Str *line;
  const char *p, *s, *emsg;
  int lineno;
  int verbose = 1;

  if ((fd = fileno(kf)) < 0 || fstat(fd, &kstat) ||
      (!force && kstat.st_mtime == current->st_mtime &&
       kstat.st_dev == current->st_dev && kstat.st_ino == current->st_ino &&
       kstat.st_size == current->st_size))
    return;
  *current = kstat;

  lineno = 0;
  while (!feof(kf)) {
    line = Strfgets(kf);
    lineno++;
    Strchop(line);
    Strremovefirstspaces(line);
    if (line->length == 0)
      continue;
    p = line->ptr;
    s = getWord(&p);
    if (*s == '#') /* comment */
      continue;
    if (!strcmp(s, "keymap"))
      ;
    else if (!strcmp(s, "verbose")) {
      s = getWord(&p);
      if (*s)
        verbose = str_to_bool(s, verbose);
      continue;
    } else { /* error */
      emsg = Sprintf("line %d: syntax error '%s'", lineno, s)->ptr;
      App::instance().record_err_message(emsg);
      if (verbose)
        App::instance().disp_message_nsec(emsg, 1, true);
      continue;
    }
    App::instance().setKeymap(p, lineno, verbose);
  }
}

static char keymap_initialized = false;
static struct stat sys_current_keymap_file;
static struct stat current_keymap_file;

void App::initKeymap(bool force) {

  if (auto kf = fopen(confFile(KEYMAP_FILE).c_str(), "rt")) {
    interpret_keymap(kf, &sys_current_keymap_file,
                     force || !keymap_initialized);
    fclose(kf);
  }
  if (auto kf = fopen(rcFile(keymap_file).c_str(), "rt")) {
    interpret_keymap(kf, &current_keymap_file, force || !keymap_initialized);
    fclose(kf);
  }
  keymap_initialized = true;
}

std::string App::make_lastline_link(const std::shared_ptr<Buffer> &buf,
                                    const char *title, const char *url) {
  int l = this->COLS() - 1;
  std::stringstream s;
  if (title && *title) {
    s << "[" << title << "]";
    // for (auto p = s.begin(); p != s.end(); p++) {
    //   if (IS_CNTRL(*p) || IS_SPACE(*p)) {
    //     *p = ' ';
    //   }
    // }
    if (url) {
      s << " ";
    }
    l -= get_strwidth(s.str());
    if (l <= 0) {
      return s.str();
    }
  }
  if (!url) {
    return s.str();
  }

  Url pu(url, buf->layout->data.baseURL);
  auto u = pu.to_Str();
  if (DecodeURL) {
    u = url_decode0(u);
  }

  if (l <= 4 || l >= get_strwidth(u)) {
    if (s.str().empty()) {
      return u;
    }
    s << u;
    return s.str();
  }

  int i = (l - 2) / 2;
  s << u.substr(0, i);
  s << "..";
  i = get_strwidth(u) - (this->COLS() - 1 - get_strwidth(s.str()));
  s << u.c_str() + i;
  return s.str();
}

// std::string App::make_lastline_message(const std::shared_ptr<Buffer> &buf) {
//
//   std::string s;
//   if (displayLink) {
//
//     Anchor *a = buf->layout->retrieveCurrentAnchor();
//     std::string p;
//     if (a && a->title.size())
//       p = a->title;
//     else {
//       auto a_img = buf->layout->retrieveCurrentImg();
//       if (a_img && a_img->title.size())
//         p = a_img->title;
//     }
//
//     if (p.size() || a)
//       s = this->make_lastline_link(buf, p.c_str(), a ? a->url.c_str() :
//       NULL);
//
//     if (s.size()) {
//       auto sl = get_strwidth(s.c_str());
//       if (sl >= this->COLS() - 3)
//         return s;
//     }
//   }
//
//   std::stringstream ss;
//   int sl = 0;
//   if (displayLineInfo && buf->layout->currentLine()) {
//     int cl = buf->layout->visual.cursor().row;
//     int ll = buf->layout->linenumber(buf->layout->lastLine());
//     int r = (int)((double)cl * 100.0 / (double)(ll ? ll : 1) + 0.5);
//     ss << Sprintf("%d/%d (%d%%)", cl, ll, r)->ptr;
//   } else {
//     ss << "Viewing";
//   }
//
//   if (buf->res->ssl_certificate) {
//     ss << "[SSL]";
//   }
//
//   ss << " <" << buf->layout->data.title;
//
//   auto msg = ss.str();
//   if (s.size()) {
//     int l = this->COLS() - 3 - sl;
//     if (get_strwidth(msg.c_str()) > l) {
//       msg = msg.substr(0, l);
//     }
//     msg += "> ";
//     msg += s;
//   } else {
//     msg += ">";
//   }
//
//   ss << "::" << _message;
//
//   return msg;
// }
