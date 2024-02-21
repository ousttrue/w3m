#include "app.h"
#include "proto.h"
#include "quote.h"
#include "html/readbuffer.h"
#include "screen.h"
#include "screen.h"
#include "ssl_util.h"
#include "html/form.h"
#include "http_response.h"
#include "local_cgi.h"
#include "w3m.h"
#include "downloadlist.h"
#include "tabbuffer.h"
#include "buffer.h"
#include "myctype.h"
#include "func.h"
#include "alloc.h"
#include "Str.h"
#include "rc.h"
#include "cookie.h"
#include "history.h"
#include <iostream>
#include <sstream>
#include <uv.h>
#include <chrono>
// #include <unistd.h>

#ifdef _MSC_VER
#include <direct.h>
// #define getcwd _getcwd
#endif

const auto FRAME_INTERVAL_MS = std::chrono::milliseconds(1000 / 15);

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume and upstream
// PHP does this as well.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

const char *CurrentKeyData;
const char *CurrentCmdData;
int prec_num = 0;
bool on_target = true;
#define PREC_LIMIT 10000

// static void sig_chld(int signo) {
//   int p_stat;
//   pid_t pid;
//
//   while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0) {
//     DownloadList *d;
//
//     if (WIFEXITED(p_stat)) {
//       for (d = FirstDL; d != nullptr; d = d->next) {
//         if (d->pid == pid) {
//           d->err = WEXITSTATUS(p_stat);
//           break;
//         }
//       }
//     }
//   }
//   mySignal(SIGCHLD, sig_chld);
//   return;
// }

// static void SigPipe(SIGNAL_ARG) {
//   mySignal(SIGPIPE, SigPipe);
//   SIGNAL_RETURN;
// }

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

uv_tty_t g_tty_in;
uv_signal_t g_signal_resize;
uv_timer_t g_timer;

App::App() : _screen(new Screen) {
  static int s_i = 0;
  assert(s_i == 0);
  ++s_i;

  w3mFuncList = {
      /*0*/ {"@@@", nulcmd},
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
      /*62*/ {"MARK_URL", chkURL},
      /*63*/ {"MARK_WORD", chkWORD},
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
      /*93*/ {"NEXT_VISITED", nextVA},
      /*94*/ {"NEXT_WORD", movRW},
      /*95*/ {"NOTHING", nulcmd},
      /*96*/ {"NULL", nulcmd},
      /*97*/ {"OPTIONS", ldOpt},
      /*98*/ {"PEEK", curURL},
      /*99*/ {"PEEK_IMG", peekIMG},
      /*100*/ {"PEEK_LINK", peekURL},
      /*101*/ {"PIPE_BUF", pipeBuf},
      /*102*/ {"PIPE_SHELL", pipesh},
      /*103*/ {"PREV", prevBf},
      /*104*/ {"PREV_HALF_PAGE", hpgBack},
      /*105*/ {"PREV_LINK", prevA},
      /*106*/ {"PREV_MARK", prevMk},
      /*107*/ {"PREV_PAGE", pgBack},
      /*108*/ {"PREV_TAB", prevT},
      /*109*/ {"PREV_VISITED", prevVA},
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
  GlobalKeymap = {
      /*  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g      */
      FuncId::_mark,
      FuncId::linbeg,
      FuncId::movL,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::linend,
      FuncId::movR,
      FuncId::curlno,
      /*  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o      */
      FuncId::ldHist,
      FuncId::nextA,
      FuncId::followA,
      FuncId::cooLst,
      FuncId::rdrwSc,
      FuncId::followA,
      FuncId::movD,
      FuncId::nulcmd,
      /*  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w      */
      FuncId::movU,
      FuncId::closeT,
      FuncId::isrchbak,
      FuncId::isrchfor,
      FuncId::tabA,
      FuncId::prevA,
      FuncId::pgFore,
      FuncId::wrapToggle,
      /*  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_      */
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::susp,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::goHome,
      /*  SPC     !       "       #       $       %       &       '        */
      FuncId::pgFore,
      FuncId::execsh,
      FuncId::reMark,
      FuncId::pipesh,
      FuncId::linend,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      /*  (       )       *       +       FuncId::,       -       .       / */
      FuncId::undoPos,
      FuncId::redoPos,
      FuncId::nulcmd,
      FuncId::pgFore,
      FuncId::col1L,
      FuncId::pgBack,
      FuncId::col1R,
      FuncId::srchfor,
      /*  0       1       2       3       4       5       6       7        */
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      /*  8       9       :       ;       <       =       >       ?        */
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::chkURL,
      FuncId::chkWORD,
      FuncId::shiftl,
      FuncId::pginfo,
      FuncId::shiftr,
      FuncId::srchbak,
      /*  @       A       B       C       D       E       F       G        */
      FuncId::readsh,
      FuncId::nulcmd,
      FuncId::backBf,
      FuncId::nulcmd,
      FuncId::ldDL,
      FuncId::editBf,
      FuncId::rFrame,
      FuncId::goLineL,
      /*  H       I       J       K       L       M       N       O        */
      FuncId::ldhelp,
      FuncId::followI,
      FuncId::lup1,
      FuncId::ldown1,
      FuncId::linkLst,
      FuncId::extbrz,
      FuncId::srchprv,
      FuncId::nulcmd,
      /*  P       Q       R       S       T       U       V       W        */
      FuncId::nulcmd,
      FuncId::quitfm,
      FuncId::reload,
      FuncId::svBuf,
      FuncId::newT,
      FuncId::goURL,
      FuncId::ldfile,
      FuncId::movLW,
      /*  X       Y       Z       [       \       ]       ^       _        */
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::ctrCsrH,
      FuncId::topA,
      FuncId::nulcmd,
      FuncId::lastA,
      FuncId::linbeg,
      FuncId::nulcmd,
      /*  `       a       b       c       d       e       f       g        */
      FuncId::nulcmd,
      FuncId::svA,
      FuncId::pgBack,
      FuncId::curURL,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::goLineF,
      /*  h       i       j       k       l       m       n       o        */
      FuncId::movL,
      FuncId::peekIMG,
      FuncId::movD,
      FuncId::movU,
      FuncId::movR,
      FuncId::msToggle,
      FuncId::srchnxt,
      FuncId::ldOpt,
      /*  p       q       r       s       t       u       v       w        */
      FuncId::nulcmd,
      FuncId::qquitfm,
      FuncId::dispVer,
      FuncId::selMn,
      FuncId::nulcmd,
      FuncId::peekURL,
      FuncId::vwSrc,
      FuncId::movRW,
      /*  x       y       z       {       |       }       ~       DEL      */
      FuncId::nulcmd,
      FuncId::nulcmd,
      FuncId::ctrCsrV,
      FuncId::prevT,
      FuncId::pipeBuf,
      FuncId::nextT,
      FuncId::nulcmd,
      FuncId::nulcmd,
  };
  _funcTable = {
      {"SUSPEND", FuncId::susp},
      {"SET_OPTION", FuncId::setOpt},
      {"END", FuncId::goLineL},
      {"VIEW", FuncId::vwSrc},
      {"PREV_VISITED", FuncId::prevVA},
      {"EXTERN", FuncId::extbrz},
      {"UNDO", FuncId::undoPos},
      {"SHELL", FuncId::execsh},
      {"RIGHT", FuncId::col1R},
      {"PREV_WORD", FuncId::movLW},
      {"LEFT", FuncId::col1L},
      {"INTERRUPT", FuncId::susp},
      {"TAB_GOTO_RELATIVE", FuncId::tabrURL},
      {"NEXT_UP", FuncId::nextU},
      {"CLOSE_TAB_MOUSE", FuncId::closeTMs},
      {"DOWN", FuncId::ldown1},
      {"HISTORY", FuncId::ldHist},
      {"SEARCH", FuncId::srchfor},
      {"NEXT_VISITED", FuncId::nextVA},
      {"NEXT_LEFT_UP", FuncId::nextLU},
      {"MOVE_UP", FuncId::movU},
      {"DOWNLOAD", FuncId::svSrc},
      {"VIEW_IMAGE", FuncId::followI},
      {"MARK", FuncId::_mark},
      {"INFO", FuncId::pginfo},
      {"VERSION", FuncId::dispVer},
      {"BEGIN", FuncId::goLineF},
      {"REDRAW", FuncId::rdrwSc},
      {"QUIT", FuncId::qquitfm},
      {"DOWNLOAD_LIST", FuncId::ldDL},
      {"REG_MARK", FuncId::reMark},
      {"MOVE_RIGHT", FuncId::movR},
      {"MARK_MID", FuncId::chkNMID},
      {"LOAD", FuncId::ldfile},
      {"EXEC_SHELL", FuncId::execsh},
      {"VIEW_BOOKMARK", FuncId::ldBmark},
      {"TAB_MOUSE", FuncId::tabMs},
      {"STOP_IMAGE", FuncId::stopI},
      {"GOTO_HOME", FuncId::goHome},
      {"SHIFT_RIGHT", FuncId::shiftr},
      {"SEARCH_NEXT", FuncId::srchnxt},
      {"PEEK", FuncId::curURL},
      {"DICT_WORD_AT", FuncId::dictwordat},
      {"SOURCE", FuncId::vwSrc},
      {"SGRMOUSE", FuncId::sgrmouse},
      {"SAVE_LINK", FuncId::svA},
      {"GOTO", FuncId::goURL},
      {"ACCESSKEY", FuncId::accessKey},
      {"ABORT", FuncId::quitfm},
      {"MENU", FuncId::mainMn},
      {"EXIT", FuncId::quitfm},
      {"PREV_HALF_PAGE", FuncId::hpgBack},
      {"LINE_INFO", FuncId::curlno},
      {"ADD_BOOKMARK", FuncId::adBmark},
      {"WHEREIS", FuncId::srchfor},
      {"SELECT_MENU", FuncId::selMn},
      {"GOTO_LINE", FuncId::goLine},
      {"MOUSE", FuncId::mouse},
      {"PIPE_BUF", FuncId::pipeBuf},
      {"LINK_BEGIN", FuncId::topA},
      {"PEEK_IMG", FuncId::peekIMG},
      {"CHARSET", FuncId::docCSet},
      {"GOTO_LINK", FuncId::followA},
      {"EXTERN_LINK", FuncId::linkbrz},
      {"MARK_WORD", FuncId::chkWORD},
      {"MOVE_LIST_MENU", FuncId::movlistMn},
      {"LINK_MENU", FuncId::linkMn},
      {"CURSOR_MIDDLE", FuncId::cursorMiddle},
      {"CURSOR_TOP", FuncId::cursorTop},
      {"REDO", FuncId::redoPos},
      {"BOOKMARK", FuncId::ldBmark},
      {"NEXT_RIGHT_DOWN", FuncId::nextRD},
      {"NEXT_LEFT", FuncId::nextL},
      {"COMMAND", FuncId::execCmd},
      {"@@@", FuncId::nulcmd},
      {"RESHAPE", FuncId::reshape},
      {"ALARM", FuncId::setAlarm},
      {"UP", FuncId::lup1},
      {"SETENV", FuncId::setEnv},
      {"RELOAD", FuncId::reload},
      {"LIST", FuncId::linkLst},
      {"NEXT_PAGE", FuncId::pgFore},
      {"MOVE_MOUSE", FuncId::movMs},
      {"DISPLAY_IMAGE", FuncId::dispI},
      {"TAB_LEFT", FuncId::tabL},
      {"PIPE_SHELL", FuncId::pipesh},
      {"NEXT_TAB", FuncId::nextT},
      {"NEXT_DOWN", FuncId::nextD},
      {"MAIN_MENU", FuncId::mainMn},
      {"LIST_MENU", FuncId::listMn},
      {"REINIT", FuncId::reinit},
      {"EDIT", FuncId::editBf},
      {"SAVE_IMAGE", FuncId::svI},
      {"NEXT_MARK", FuncId::nextMk},
      {"COOKIE", FuncId::cooLst},
      {"LINK_END", FuncId::lastA},
      {"LINE_BEGIN", FuncId::linbeg},
      {"DELETE_PREVBUF", FuncId::deletePrevBuf},
      {"WRAP_TOGGLE", FuncId::wrapToggle},
      {"DEFAULT_CHARSET", FuncId::defCSet},
      {"NOTHING", FuncId::nulcmd},
      {"FRAME", FuncId::rFrame},
      {"SEARCH_PREV", FuncId::srchprv},
      {"NEXT", FuncId::nextBf},
      {"LINE_END", FuncId::linend},
      {"SELECT", FuncId::selBuf},
      {"PREV_TAB", FuncId::prevT},
      {"MOVE_LEFT", FuncId::movL},
      {"SEARCH_BACK", FuncId::srchbak},
      {"SAVE", FuncId::svSrc},
      {"NEXT_HALF_PAGE", FuncId::hpgFore},
      {"SHIFT_LEFT", FuncId::shiftl},
      {"READ_SHELL", FuncId::readsh},
      {"PRINT", FuncId::svBuf},
      {"MOVE_DOWN", FuncId::movD},
      {"EDIT_SCREEN", FuncId::editScr},
      {"ISEARCH_BACK", FuncId::isrchbak},
      {"NEXT_LINK", FuncId::nextA},
      {"MSGS", FuncId::msgs},
      {"MULTIMAP", FuncId::multimap},
      {"CENTER_H", FuncId::ctrCsrH},
      {"TAB_LINK", FuncId::tabA},
      {"TAB_GOTO", FuncId::tabURL},
      {"PREV_PAGE", FuncId::pgBack},
      {"SAVE_SCREEN", FuncId::svBuf},
      {"TAB_MENU", FuncId::tabMn},
      {"SEARCH_FORE", FuncId::srchfor},
      {"MOVE_UP1", FuncId::movU1},
      {"MOVE_LEFT1", FuncId::movL1},
      {"PREV_MARK", FuncId::prevMk},
      {"PEEK_LINK", FuncId::peekURL},
      {"MARK_URL", FuncId::chkURL},
      {"GOTO_RELATIVE", FuncId::gorURL},
      {"SUBMIT", FuncId::submitForm},
      {"NEXT_WORD", FuncId::movRW},
      {"NEW_TAB", FuncId::newT},
      {"HELP", FuncId::ldhelp},
      {"MOVE_DOWN1", FuncId::movD1},
      {"ISEARCH", FuncId::isrchfor},
      {"NEXT_RIGHT", FuncId::nextR},
      {"CLOSE_TAB", FuncId::closeT},
      {"CENTER_V", FuncId::ctrCsrV},
      {"MOVE_RIGHT1", FuncId::movR1},
      {"ESCMAP", FuncId::escmap},
      {"ESCBMAP", FuncId::escbmap},
      {"MENU_MOUSE", FuncId::menuMs},
      {"PREV", FuncId::prevBf},
      {"NULL", FuncId::nulcmd},
      {"LINK_N", FuncId::nthA},
      {"DICT_WORD", FuncId::dictword},
      {"PREV_LINK", FuncId::prevA},
      {"CURSOR_BOTTOM", FuncId::cursorBottom},
      {"TAB_RIGHT", FuncId::tabR},
      {"MOUSE_TOGGLE", FuncId::msToggle},
      {"BACK", FuncId::backBf},
      {"OPTIONS", FuncId::ldOpt},
      {"DEFINE_KEY", FuncId::defKey},
  };

  uv_tty_init(uv_default_loop(), &g_tty_in, 0, 1);

  //
  // process
  //
  _currentPid = getpid();

#ifdef MAXPATHLEN
  {
    char buf[MAXPATHLEN];
    getcwd(buf, MAXPATHLEN);
    _currentDir = buf;
  }
#else
  _currentDir = getcwd(nullptr, 0);
#endif

  {
    char hostname[HOST_NAME_MAX + 2];
    if (gethostname(hostname, HOST_NAME_MAX + 2) == 0) {
      /* Don't use hostname if it is truncated.  */
      hostname[HOST_NAME_MAX + 1] = '\0';
      auto hostname_len = strlen(hostname);
      if (hostname_len <= HOST_NAME_MAX && hostname_len < STR_SIZE_MAX) {
        _hostName = hostname;
      }
    }
  }

  if (_editor.empty()) {
    if (auto p = getenv("EDITOR")) {
      _editor = p;
    }
  }

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

  _firstTab = _lastTab = _currentTab = new TabBuffer;
  assert(_firstTab);
  _nTab = 1;

  // default dispatcher
  _dispatcher.push([this](const char *buf, int len) {
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
    saveHistory(URLHist, URLHistSize);
  }

  for (auto &f : _fileToDelete) {
    std::cout << "fileToDelete: " << f << std::endl;
    unlink(f.c_str());
  }
  _fileToDelete.clear();

  stopDownload();
  free_ssl_ctx();
}

void App::setKeymap(const char *p, int lineno, int verbose) {
  const char *s, *emsg;
  int c;

  s = getQWord(&p);
  c = getKey(s);
  if (c < 0) { /* error */
    if (lineno > 0)
      emsg = Sprintf("line %d: unknown key '%s'", lineno, s)->ptr;
    else
      emsg = Sprintf("defkey: unknown key '%s'", s)->ptr;
    App::instance().App::instance().record_err_message(emsg);
    if (verbose)
      App::instance().disp_message_nsec(emsg, 1, true);
    return;
  }
  s = getWord(&p);
  auto f = getFuncList(s);
  if ((int)f < 0) {
    if (lineno > 0)
      emsg = Sprintf("line %d: invalid command '%s'", lineno, s)->ptr;
    else
      emsg = Sprintf("defkey: invalid command '%s'", s)->ptr;
    App::instance().record_err_message(emsg);
    if (verbose)
      App::instance().disp_message_nsec(emsg, 1, true);
    return;
  }
  auto map = GlobalKeymap;
  map[c & 0x7F] = (FuncId)f;
  s = getQWord(&p);
  keyData.insert({(FuncId)f, s});
}
bool App::initialize() {
  init_rc();

  if (!BookmarkFile) {
    BookmarkFile = rcFile(BOOKMARK);
  }

  sync_with_option();
  initCookie();

  LoadHist = newHist();
  SaveHist = newHist();
  ShellHist = newHist();
  TextHist = newHist();
  URLHist = newHist();
  if (UseHistory) {
    loadHistory(URLHist);
  }

  return true;
}

/*
 * Initialize routine.
 */
void App::beginRawMode(void) {
  if (!_fmInitialized) {
    // initscr();
    uv_tty_set_mode(&g_tty_in, UV_TTY_MODE_RAW);
    // term_raw();
    // term_noecho();
    _fmInitialized = true;
  }
}

void App::endRawMode(void) {
  if (_fmInitialized) {
    _screen->clrtoeolx({.row = LASTLINE(), .col = 0});
    _screen->print();
    uv_tty_set_mode(&g_tty_in, UV_TTY_MODE_NORMAL);
    _fmInitialized = false;
  }
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
    set_environ("W3M_TITLE", buf->layout.title.c_str());
    set_environ("W3M_URL", buf->res->currentURL.to_Str().c_str());
    set_environ("W3M_TYPE",
                buf->res->type.size() ? buf->res->type.c_str() : "unknown");
  }
  l = buf->layout.currentLine;
  if (l && (buf != prev_buf || l != prev_line || buf->layout.pos != prev_pos)) {
    Url pu;
    auto s = buf->layout.getCurWord();
    set_environ("W3M_CURRENT_WORD", s.c_str());
    auto a = buf->layout.retrieveCurrentAnchor();
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      set_environ("W3M_CURRENT_LINK", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_LINK", "");
    a = buf->layout.retrieveCurrentImg();
    if (a) {
      pu = urlParse(a->url, buf->res->getBaseURL());
      set_environ("W3M_CURRENT_IMG", pu.to_Str().c_str());
    } else
      set_environ("W3M_CURRENT_IMG", "");
    a = buf->layout.retrieveCurrentForm();
    if (a)
      set_environ("W3M_CURRENT_FORM", form2str((FormItemList *)a->url));
    else
      set_environ("W3M_CURRENT_FORM", "");
    set_environ("W3M_CURRENT_LINE", Sprintf("%ld", l->real_linenumber)->ptr);
    set_environ("W3M_CURRENT_COLUMN", Sprintf("%d", buf->layout.currentColumn +
                                                        buf->layout.cursorX + 1)
                                          ->ptr);
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
  prev_pos = buf->layout.pos;
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
  *buf = uv_buf_init((char *)malloc(suggested_size), suggested_size);
}

int App::mainLoop() {
  App::instance().beginRawMode();
  onResize();

  //
  // stdin tty read
  //
  uv_read_start((uv_stream_t *)&g_tty_in, &alloc_buffer,
                [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
                  if (nread < 0) {
                    if (nread == UV_EOF) {
                      // end of file
                      uv_close((uv_handle_t *)&g_tty_in, NULL);
                    }
                  } else if (nread > 0) {
                    // process key input
                    App::instance().dispatch(buf->base, nread);
                  }

                  // OK to free buffer as write_data copies it.
                  if (buf->base) {
                    free(buf->base);
                  }
                });

  //
  // SIGWINCH
  //
  uv_signal_init(uv_default_loop(), &g_signal_resize);
  uv_signal_start(
      &g_signal_resize,
      [](uv_signal_t *handle, int signum) { App::instance().onResize(); },
      SIGWINCH);

  //
  // timer
  //
  uv_timer_init(uv_default_loop(), &g_timer);
  uv_timer_start(
      &g_timer, [](uv_timer_t *handle) { App::instance().onFrame(); }, 0,
      FRAME_INTERVAL_MS.count());

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());
  return 0;
}

#ifdef _MSC_VER
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
  _screen->setupscreen(_size);
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
  uv_read_stop((uv_stream_t *)&g_tty_in);
  uv_signal_stop(&g_signal_resize);
  uv_timer_stop(&g_timer);
}

int App::searchKeyNum() {
  int n = 1;
  if (auto d = searchKeyData()) {
    n = atoi(d);
  }
  return n * PREC_NUM;
}

std::string App::myEditor(const char *file, int line) const {
  std::stringstream tmp;
  bool set_file = false;
  bool set_line = false;
  for (auto p = _editor.begin(); *p; p++) {
    if (*p == '%' && *(p + 1) == 's' && !set_file) {
      tmp << file;
      set_file = true;
      p++;
    } else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0) {
      tmp << line;
      set_line = true;
      p++;
    } else {
      tmp << *p;
    }
  }
  if (!set_file) {
    if (!set_line && line > 1 && strcasestr(_editor.c_str(), "vi")) {
      tmp << " +" << line;
    }
    tmp << " " << file;
  }
  return tmp.str();
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

void App::_peekURL(bool only_img) {
  Anchor *a;
  static Str *s = nullptr;
  static int offset = 0;

  if (_currentTab->currentBuffer()->layout.firstLine == nullptr) {
    return;
  }

  if (_currentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS())
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
    goto disp;
  } else {
    offset = 0;
  }
  s = nullptr;
  a = (only_img ? nullptr
                : _currentTab->currentBuffer()->layout.retrieveCurrentAnchor());
  if (a == nullptr) {
    a = (only_img ? nullptr
                  : _currentTab->currentBuffer()->layout.retrieveCurrentForm());
    if (a == nullptr) {
      a = _currentTab->currentBuffer()->layout.retrieveCurrentImg();
      if (a == nullptr)
        return;
    } else
      s = Strnew_charp(form2str((FormItemList *)a->url));
  }
  if (s == nullptr) {
    auto pu = urlParse(a->url, _currentTab->currentBuffer()->res->getBaseURL());
    s = Strnew(pu.to_Str());
  }
  if (DecodeURL)
    s = Strnew_charp(url_decode0(s->ptr));
disp:
  int n = searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS() - 1))
    offset = (n - 1) * (COLS() - 1);
  disp_message(&s->ptr[offset]);
}

std::string App::currentUrl() const {
  static Str *s = nullptr;
  static int offset = 0;

  if (_currentKey == prev_key && s != nullptr) {
    if (s->length - offset >= COLS())
      offset++;
    else if (s->length <= offset) /* bug ? */
      offset = 0;
  } else {
    offset = 0;
    s = Strnew(_currentTab->currentBuffer()->res->currentURL.to_Str());
    if (DecodeURL)
      s = Strnew_charp(url_decode0(s->ptr));
  }
  auto n = App::instance().searchKeyNum();
  if (n > 1 && s->length > (n - 1) * (COLS() - 1))
    offset = (n - 1) * (COLS() - 1);

  return &s->ptr[offset];
}

void App::doCmd() {
  CurrentKeyData = nullptr; /* not allowed in w3m-control: */
  auto data = searchKeyData();
  if (data == nullptr || *data == '\0') {
    // data = inputStrHist("command [; ...]: ", "", TextHist);
    if (data == nullptr) {
      invalidate();
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
    auto cmd = getFuncList(p);
    if ((int)cmd < 0)
      break;
    p = getQWord(&data);
    doCmd(cmd, p);
  }
  invalidate();
}

void App::doCmd(FuncId cmd, const char *data) {
  _currentKey = -1;
  CurrentKeyData = nullptr;
  CurrentCmdData = *data ? data : nullptr;
  w3mFuncList[(int)cmd].func();
  CurrentCmdData = nullptr;
}

void App::dispatchPtyIn(const char *buf, size_t len) {
  if (len == 0) {
    return;
  }
  auto c = buf[0];
  if (IS_ASCII(c)) { /* Ascii */
    if (('0' <= c) && (c <= '9') &&
        (prec_num || (GlobalKeymap[(int)c] == FuncId::nulcmd))) {
      prec_num = prec_num * 10 + (int)(c - '0');
      if (prec_num > PREC_LIMIT)
        prec_num = PREC_LIMIT;
    } else {
      set_buffer_environ(_currentTab->currentBuffer());
      _currentTab->currentBuffer()->layout.save_buffer_position();
      _currentKey = c;
      w3mFuncList[(int)GlobalKeymap[(int)c]].func();
      prec_num = 0;
    }
  }
  prev_key = _currentKey;
  _currentKey = -1;
  CurrentKeyData = NULL;
}

void App::onFrame() {
  if (popAddDownloadList()) {
    ldDL();
  }

  if (auto a = _currentTab->currentBuffer()->layout.submit) {
    _currentTab->currentBuffer()->layout.submit = NULL;
    _currentTab->currentBuffer()->layout.gotoLine(a->start.line);
    _currentTab->currentBuffer()->layout.pos = a->start.pos;
    if (auto buf = _currentTab->currentBuffer()->followForm(a, true)) {
      pushBuffer(buf, a->target);
    }
  }

  if (_dirty > 0) {
    _dirty = 0;
    _screen->display(INIT_BUFFER_WIDTH(), _currentTab);
  }
}

struct TimerTask {
  FuncId cmd;
  std::string data;
  uv_timer_t handle;
};
std::list<std::shared_ptr<TimerTask>> g_timers;

void App::task(int sec, FuncId cmd, const char *data, bool repeat) {
  if ((int)cmd >= 0) {
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
    disp_message_nsec(
        Sprintf("%dsec %s %s", sec, w3mFuncList[(int)cmd].id, data)->ptr, 1,
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

TabBuffer *App::numTab(int n) const {
  if (n == 0)
    return _currentTab;
  if (n == 1)
    return _firstTab;
  if (_nTab <= 1)
    return nullptr;

  TabBuffer *tab;
  int i;
  for (tab = _firstTab, i = 1; tab && i < n; tab = tab->nextTab, i++)
    ;
  return tab;
}

void App::drawTabs() {
  if (_nTab > 1) {
    _screen->clrtoeolx({0, 0});
    int y = 0;
    for (auto t = _firstTab; t; t = t->nextTab) {
      y = t->draw(_screen.get(), _currentTab);
    }
    RowCol pos{.row = y + 1, .col = 0};
    for (int i = 0; i < COLS(); i++) {
      _screen->addch(pos, '~');
      ++pos.col;
    }
  }
}

void App::nextTab() {
  if (_nTab <= 1) {
    return;
  }

  for (int i = 0; i < PREC_NUM; i++) {
    if (_currentTab->nextTab)
      _currentTab = _currentTab->nextTab;
    else
      _currentTab = _firstTab;
  }
}

void App::prevTab() {
  if (_nTab <= 1) {
    return;
  }
  for (int i = 0; i < PREC_NUM; i++) {
    if (_currentTab->prevTab)
      _currentTab = _currentTab->prevTab;
    else
      _currentTab = _lastTab;
  }
}

void App::tabRight() {
  TabBuffer *tab;
  int i;
  for (tab = _currentTab, i = 0; tab && i < PREC_NUM; tab = tab->nextTab, i++)
    ;
  moveTab(_currentTab, tab ? tab : _lastTab, true);
}

void App::tabLeft() {
  TabBuffer *tab;
  int i;
  for (tab = _currentTab, i = 0; tab && i < PREC_NUM; tab = tab->prevTab, i++)
    ;
  moveTab(_currentTab, tab ? tab : _firstTab, false);
}

int App::calcTabPos() {
  if (_nTab <= 0) {
    return _lastTab->y;
  }

  TabBuffer *tab;
  int lcol = 0, rcol = 0, col;

  int n2, ny;
  int n1 = (COLS() - rcol - lcol) / TabCols;
  if (n1 >= _nTab) {
    n2 = 1;
    ny = 1;
  } else {
    if (n1 < 0)
      n1 = 0;
    n2 = COLS() / TabCols;
    if (n2 == 0)
      n2 = 1;
    ny = (_nTab - n1 - 1) / n2 + 2;
  }

  // int n2, na, nx, ny, ix, iy;
  int na = n1 + n2 * (ny - 1);
  n1 -= (na - _nTab) / ny;
  if (n1 < 0)
    n1 = 0;
  na = n1 + n2 * (ny - 1);
  tab = _firstTab;
  for (int iy = 0; iy < ny && tab; iy++) {
    int nx;
    if (iy == 0) {
      nx = n1;
      col = COLS() - rcol - lcol;
    } else {
      nx = n2 - (na - _nTab + (iy - 1)) / (ny - 1);
      col = COLS();
    }
    for (int ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
      tab->x1 = col * ix / nx;
      tab->x2 = col * (ix + 1) / nx - 1;
      tab->y = iy;
      if (iy == 0) {
        tab->x1 += lcol;
        tab->x2 += lcol;
      }
    }
  }
  return _lastTab->y;
}

TabBuffer *App::deleteTab(TabBuffer *tab) {
  if (_nTab <= 1)
    return _firstTab;
  if (tab->prevTab) {
    if (tab->nextTab)
      tab->nextTab->prevTab = tab->prevTab;
    else
      _lastTab = tab->prevTab;
    tab->prevTab->nextTab = tab->nextTab;
    if (tab == _currentTab)
      _currentTab = tab->prevTab;
  } else { /* tab == FirstTab */
    tab->nextTab->prevTab = nullptr;
    _firstTab = tab->nextTab;
    if (tab == _currentTab)
      _currentTab = tab->nextTab;
  }
  _nTab--;
  return _firstTab;
}

void App::moveTab(TabBuffer *t, TabBuffer *t2, int right) {
  if (!t2) {
    t2 = _firstTab;
  }
  if (!t || !t2 || t == t2 || !t) {
    return;
  }
  if (t->prevTab) {
    if (t->nextTab)
      t->nextTab->prevTab = t->prevTab;
    else
      _lastTab = t->prevTab;
    t->prevTab->nextTab = t->nextTab;
  } else {
    t->nextTab->prevTab = nullptr;
    _firstTab = t->nextTab;
  }
  if (right) {
    t->nextTab = t2->nextTab;
    t->prevTab = t2;
    if (t2->nextTab)
      t2->nextTab->prevTab = t;
    else
      _lastTab = t;
    t2->nextTab = t;
  } else {
    t->prevTab = t2->prevTab;
    t->nextTab = t2;
    if (t2->prevTab)
      t2->prevTab->nextTab = t;
    else
      _firstTab = t;
    t2->prevTab = t;
  }
  invalidate();
}

void App::newTab(std::shared_ptr<Buffer> buf) {
  auto tab = new TabBuffer();
  if (!buf) {
    buf = Buffer::create();
    *buf = *_currentTab->currentBuffer();
  }
  tab->firstBuffer = tab->_currentBuffer = buf;

  tab->nextTab = _currentTab->nextTab;
  tab->prevTab = _currentTab;
  if (_currentTab->nextTab)
    _currentTab->nextTab->prevTab = tab;
  else
    _lastTab = tab;
  _currentTab->nextTab = tab;
  _currentTab = tab;
  _nTab++;

  invalidate();
}

void App::pushBuffer(const std::shared_ptr<Buffer> &buf,
                     std::string_view target) {

  if (target.empty() || /* no target specified (that means this page is
  not a
                           frame page) */
      target == "_top"  /* this link is specified to be opened as an
                                   indivisual * page */
  ) {
    CurrentTab->pushBuffer(buf);
  } else {
    Anchor *al = nullptr;
    if (!al) {
      auto label = Strnew_m_charp("_", target, nullptr)->ptr;
      al = CurrentTab->currentBuffer()->layout.name()->searchAnchor(label);
    }
    if (al) {
      CurrentTab->currentBuffer()->layout.gotoLine(al->start.line);
      if (label_topline)
        CurrentTab->currentBuffer()->layout.topLine =
            CurrentTab->currentBuffer()->layout.lineSkip(
                CurrentTab->currentBuffer()->layout.topLine,
                CurrentTab->currentBuffer()->layout.currentLine->linenumber -
                    CurrentTab->currentBuffer()->layout.topLine->linenumber,
                false);
      CurrentTab->currentBuffer()->layout.pos = al->start.pos;
      CurrentTab->currentBuffer()->layout.arrangeCursor();
    }
  }
}

void App::message(const char *s, int return_x, int return_y) {
  if (_fmInitialized) {
    RowCol pos{.row = LASTLINE(), .col = 0};
    pos = _screen->addnstr(pos, s, COLS() - 1);
    _screen->clrtoeolx(pos);
    _screen->cursor({.row = return_y, .col = return_x});
  } else {
    fprintf(stderr, "%s\n", s);
  }
}

void App::record_err_message(const char *s) {
  if (!_fmInitialized) {
    return;
  }
  if (!message_list)
    message_list = newGeneralList();
  if (message_list->nitem >= LINES())
    popValue(message_list);
  pushValue(message_list, allocStr(s, -1));
}

/*
 * List of error messages
 */
std::shared_ptr<Buffer> App::message_list_panel(void) {
  Str *tmp = Strnew_size(LINES() * COLS());
  Strcat_charp(tmp,
               "<html><head><title>List of error messages</title></head><body>"
               "<h1>List of error messages</h1><table cellpadding=0>\n");
  if (message_list)
    for (auto p = message_list->last; p; p = p->prev)
      Strcat_m_charp(tmp, "<tr><td><pre>", html_quote((char *)p->ptr),
                     "</pre></td></tr>\n", NULL);
  else
    Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
  Strcat_charp(tmp, "</table></body></html>");
  return loadHTMLString(tmp);
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

  if (CurrentTab != NULL && CurrentTab->currentBuffer() != NULL)
    message(s, CurrentTab->currentBuffer()->layout.AbsCursorX(),
            CurrentTab->currentBuffer()->layout.AbsCursorY());
  else
    message(s, LASTLINE(), 0);
  _screen->print();
  // sleep_till_anykey(sec, purge);
}

void App::disp_message(const char *s) { disp_message_nsec(s, 10, false); }

void App::set_delayed_message(const char *s) { delayed_msg = allocStr(s, -1); }

void App::refresh_message() {
  if (delayed_msg != NULL) {
    disp_message(delayed_msg);
    delayed_msg = NULL;
    _screen->print();
  }
}

void App::showProgress(long long *linelen, long long *trbyte,
                       long long current_content_length) {
  int i, j, rate, duration, eta, pos;
  static time_t last_time, start_time;
  time_t cur_time;
  Str *messages;
  char *fmtrbyte, *fmrate;

  if (!_fmInitialized) {
    return;
  }

  if (*linelen < 1024)
    return;
  if (current_content_length > 0) {
    double ratio;
    cur_time = time(0);
    if (*trbyte == 0) {
      _screen->clrtoeolx({.row = LASTLINE(), .col = 0});
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    ratio = 100.0 * (*trbyte) / current_content_length;
    fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
    duration = cur_time - start_time;
    if (duration) {
      rate = *trbyte / duration;
      fmrate = convert_size(rate, 1);
      eta = rate ? (current_content_length - *trbyte) / rate : -1;
      messages = Sprintf("%11s %3.0f%% "
                         "%7s/s "
                         "eta %02d:%02d:%02d     ",
                         fmtrbyte, ratio, fmrate, eta / (60 * 60),
                         (eta / 60) % 60, eta % 60);
    } else {
      messages =
          Sprintf("%11s %3.0f%%                          ", fmtrbyte, ratio);
    }
    RowCol pixel{.row = LASTLINE(), .col = 0};
    pixel = _screen->addstr(pixel, messages->ptr);
    pos = 42;
    i = pos + (COLS() - pos - 1) * (*trbyte) / current_content_length;
    _screen->standout();
    pixel = {.row = LASTLINE(), .col = pos};
    _screen->addch(pixel, ' ');
    ++pixel.col;
    for (j = pos + 1; j <= i; j++) {
      _screen->addch(pixel, '|');
      ++pixel.col;
    }
    _screen->standend();
    /* no_clrtoeol(); */
    _screen->print();
  } else {
    cur_time = time(0);
    if (*trbyte == 0) {
      _screen->clrtoeolx({.row = LASTLINE(), .col = 0});
      start_time = cur_time;
    }
    *trbyte += *linelen;
    *linelen = 0;
    if (cur_time == last_time)
      return;
    last_time = cur_time;
    _screen->cursor({.row = LASTLINE(), .col = 0});
    fmtrbyte = convert_size(*trbyte, 1);
    duration = cur_time - start_time;
    if (duration) {
      fmrate = convert_size(*trbyte / duration, 1);
      messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
    } else {
      messages = Sprintf("%7s loaded", fmtrbyte);
    }
    message(messages->ptr, 0, 0);
    _screen->print();
  }
}
